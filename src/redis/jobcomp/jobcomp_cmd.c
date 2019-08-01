/*
 * MIT License
 *
 * Copyright (c) 2019 Philip Kovacs
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "jobcomp_cmd.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define QUERY_KEY_TTL 300
#define SECONDS_PER_DAY 86400

static int days_since_unix_epoch(const char *iso8601_date, long long *days,
                                 long long *residual_seconds)
{
    int y, M, d, h, m, s;
    if (sscanf(iso8601_date, "%d-%d-%dT%d:%d:%d", &y, &M, &d, &h, &m, &s)
        != 6) {
        return REDISMODULE_ERR;
    }
    struct tm tm_date = {
        .tm_year = y - 1900,
        .tm_mon = M - 1,
        .tm_mday = d,
        .tm_hour = h,
        .tm_min = m,
        .tm_sec = s,
        .tm_isdst = -1
    };
    time_t start_time = timegm(&tm_date);
    if (start_time < 0) {
        return REDISMODULE_ERR;
    }
    if (days) {
        *days = start_time / SECONDS_PER_DAY;
    }
    if (residual_seconds) {
        *residual_seconds = start_time % SECONDS_PER_DAY;
    }
    return REDISMODULE_OK;
}

static int zrangebyscore_to_set(RedisModuleCtx *ctx, RedisModuleString *zkey_in,
                                RedisModuleString* set_out, double min_score,
                                double max_score)
{
    RedisModuleString *job = NULL;
    RedisModuleCallReply *reply = NULL;
    RedisModuleKey *zkey = RedisModule_OpenKey(ctx, zkey_in, REDISMODULE_READ);
    if (RedisModule_KeyType(zkey) == REDISMODULE_KEYTYPE_EMPTY) {
        return REDISMODULE_OK;
    }
    if (RedisModule_KeyType(zkey) != REDISMODULE_KEYTYPE_ZSET) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }
    // Match zset elements by score (time) and save them to a set
    RedisModule_ZsetFirstInScoreRange(zkey, min_score, max_score, 0, 0);
    while (!RedisModule_ZsetRangeEndReached(zkey)) {
        job = RedisModule_ZsetRangeCurrentElement(zkey, NULL);
        reply = RedisModule_Call(ctx, "SADD", "ss", set_out, job);
        if (RedisModule_CallReplyType(reply)
            == REDISMODULE_REPLY_ERROR) {
            RedisModule_ReplyWithError(ctx, "failed to SADD job");
            return REDISMODULE_ERR;
        }
        RedisModule_FreeString(ctx, job); job = NULL;
        RedisModule_FreeCallReply(reply); reply = NULL;
        RedisModule_ZsetRangeNext(zkey);
    }
    RedisModule_ZsetRangeStop(zkey);
    reply = RedisModule_Call(ctx, "EXPIRE", "sl", set_out, QUERY_KEY_TTL);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
        RedisModule_ReplyWithError(ctx, "failed to EXPIRE set");
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}

int jobcomp_cmd_index(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    RedisModule_AutoMemory(ctx);
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    const char *keytag = RedisModule_StringPtrLen(argv[1], NULL);
    const char *jobid = RedisModule_StringPtrLen(argv[2], NULL);
    RedisModuleString *keyname =
        RedisModule_CreateStringPrintf(ctx, "%s:%s", keytag, jobid);
    RedisModuleCallReply *reply = NULL;

    // Open the job key
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyname, REDISMODULE_READ);
    if ((RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) ||
        (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_HASH)) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    // Fetch the start and end values that we want to index
    RedisModuleString *start = NULL, *end = NULL;
    if (RedisModule_HashGet(key, REDISMODULE_HASH_CFIELDS, "Start", &start,
            "End", &end, NULL) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "expected key(s) missing");
        return REDISMODULE_ERR;
    }

    // Create or update the start index
    long long start_days = 0, start_score = 0;
    if (days_since_unix_epoch(RedisModule_StringPtrLen(start, NULL),
            &start_days, &start_score) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "Failed to parse start date");
        return REDISMODULE_ERR;
    }
    RedisModuleString *idx_start = RedisModule_CreateStringPrintf(ctx,
        "%s:idx:%ld:start", keytag, start_days);
    reply = RedisModule_Call(ctx, "ZADD", "slc", idx_start, start_score,
        jobid);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
        RedisModule_ReplyWithCallReply(ctx, reply);
        return REDISMODULE_ERR;
    }

    // Create or update the end index
    long long end_days = 0, end_score = 0;
    if (days_since_unix_epoch(RedisModule_StringPtrLen(end, NULL),
            &end_days, &end_score) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "Failed to parse end date");
        return REDISMODULE_ERR;
    }
    RedisModuleString *idx_end = RedisModule_CreateStringPrintf(ctx,
        "%s:idx:%ld:end", keytag, end_days);
    reply = RedisModule_Call(ctx, "ZADD", "slc", idx_end, end_score, jobid);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
        RedisModule_ReplyWithCallReply(ctx, reply);
        return REDISMODULE_ERR;
    }

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

int jobcomp_cmd_match(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    //RedisModule_AutoMemory(ctx);
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    const char *keytag = RedisModule_StringPtrLen(argv[1], NULL);
    const char *uuid = RedisModule_StringPtrLen(argv[2], NULL);
    RedisModuleString *keyname =
        RedisModule_CreateStringPrintf(ctx, "%s:qry:%s", keytag, uuid);

    // Open the main query key
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyname, REDISMODULE_READ);
    if ((RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) ||
        (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_HASH)) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    // Fetch the time range for this query
    RedisModuleString *start = NULL, *end = NULL;
    if (RedisModule_HashGet(key, REDISMODULE_HASH_CFIELDS, "Start", &start,
            "End", &end, NULL) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "expected key(s) missing");
        return REDISMODULE_ERR;
    }

    time_t start_time, end_time;
    start_time = strtol(RedisModule_StringPtrLen(start, NULL), NULL, 10);
    end_time = strtol(RedisModule_StringPtrLen(end, NULL), NULL, 10);
    if ((start_time == LONG_MIN) || (start_time == LONG_MAX) ||
        (end_time == LONG_MIN) || (end_time == LONG_MAX)) {
        RedisModule_ReplyWithError(ctx, "invalid time range");
        return REDISMODULE_ERR;
    }

    // Identify index buckets and scores for start and end time
    long int start_day = start_time / SECONDS_PER_DAY;
    long int start_score = start_time % SECONDS_PER_DAY;
    long int end_day = end_time / SECONDS_PER_DAY;
    long int end_score = end_time % SECONDS_PER_DAY;

    long int i = start_day;
    for (; i <= end_day; ++i) {

        if (i == start_day) {
            // Match ids from start index and save them to tmp (qrs) set
            RedisModuleString *zkey_in = RedisModule_CreateStringPrintf(ctx,
                "%s:idx:%ld:start", keytag, start_day);
            RedisModuleString *set_out = RedisModule_CreateStringPrintf(ctx,
                "%s:qrs:%s", keytag, uuid);
            if (zrangebyscore_to_set(ctx, zkey_in, set_out,
                (double)start_score, REDISMODULE_POSITIVE_INFINITE)
                == REDISMODULE_ERR) {
                    return REDISMODULE_ERR;
            }
        }

        if (i == end_day) {
            // Match ids from end index and save them to tmp (qre) set
            RedisModuleString *zkey_in = RedisModule_CreateStringPrintf(ctx,\
                "%s:idx:%ld:end", keytag, end_day);
            RedisModuleString *set_out = RedisModule_CreateStringPrintf(ctx,
                "%s:qre:%s", keytag, uuid);
            if (zrangebyscore_to_set(ctx, zkey_in, set_out,
                REDISMODULE_NEGATIVE_INFINITE, (double)end_score)
                == REDISMODULE_ERR) {
                    return REDISMODULE_ERR;
            }
        }

        if ((i > start_day) && (i < end_day)) {
        }
    }

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}
