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

static int days_since_unix_epoch(const char *iso8601_date, long long *days,
                                 long long *residual_seconds)
{
    int y, M, d, h, m, s;
    if (sscanf(iso8601_date, "%d-%d-%dT%d:%d:%d", &y, &M, &d, &h, &m, &s)
        != 6) {
        return -1;
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
        return -1;
    }
    if (days) {
        *days = start_time / 86400;
    }
    if (residual_seconds) {
        *residual_seconds = start_time % 86400;
    }
    return 0;
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
        goto error;
    }

    // Fetch the index values for this job
    RedisModuleString *start = NULL, *end = NULL, *uid = NULL;
    if (RedisModule_HashGet(key, REDISMODULE_HASH_CFIELDS, "Start", &start,
            "End", &end, "UID", &uid, NULL) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "expected key(s) missing");
        goto error;
    }

    // Create or update the start index
    long long days = 0, zscore = 0;
    if (days_since_unix_epoch(RedisModule_StringPtrLen(start, NULL), &days,
            &zscore) < 0) {
        RedisModule_ReplyWithError(ctx, "Failed to parse start date");
        goto error;
    }
    RedisModuleString *idx_start =
        RedisModule_CreateStringPrintf(ctx, "%s:idx:%ld:start", keytag, days);
    reply = RedisModule_Call(ctx, "ZADD", "slc", idx_start, zscore, jobid);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
        RedisModule_ReplyWithCallReply(ctx, reply);
        goto error;
    }

    // Create or update the uid index
    const char *uid_c = RedisModule_StringPtrLen(uid, NULL);
    RedisModuleString *idx_uid =
        RedisModule_CreateStringPrintf(ctx, "%s:idx:%ld:uid:%s", keytag, days,
            uid_c);
    reply = RedisModule_Call(ctx, "SADD", "sc", idx_uid, jobid);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
        RedisModule_ReplyWithCallReply(ctx, reply);
        goto error;
    }

    // Create or update the end index
    days = 0, zscore = 0;
    if (days_since_unix_epoch(RedisModule_StringPtrLen(end, NULL), &days,
            &zscore) < 0) {
        RedisModule_ReplyWithError(ctx, "Failed to parse end date");
        goto error;
    }
    RedisModuleString *idx_end =
        RedisModule_CreateStringPrintf(ctx, "%s:idx:%ld:end", keytag, days);
    reply = RedisModule_Call(ctx, "ZADD", "slc", idx_end, zscore, jobid);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
        RedisModule_ReplyWithCallReply(ctx, reply);
        goto error;
    }

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    RedisModule_CloseKey(key);
    return REDISMODULE_OK;

error:
    RedisModule_CloseKey(key);
    return REDISMODULE_ERR;
}

int jobcomp_cmd_match(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    RedisModule_AutoMemory(ctx);
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    //const char *keyname = RedisModule_StringPtrLen(argv[1], NULL);

    // Open the query key
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    if ((RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) ||
        (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_HASH)) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        goto error;
    }

    // Fetch the time range for this query
    RedisModuleString *start = NULL, *end = NULL;
    if (RedisModule_HashGet(key, REDISMODULE_HASH_CFIELDS, "Start", &start,
            "End", &end, NULL) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "expected key(s) missing");
        goto error;
    }

    time_t start_time, end_time;
    start_time = strtol(RedisModule_StringPtrLen(start, NULL), NULL, 10);
    end_time = strtol(RedisModule_StringPtrLen(end, NULL), NULL, 10);
    if ((start_time == LONG_MIN) || (start_time == LONG_MAX) ||
        (end_time == LONG_MIN) || (end_time == LONG_MAX)) {
        RedisModule_ReplyWithError(ctx, "invalid time range");
        goto error;
    }

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    RedisModule_CloseKey(key);
    return REDISMODULE_OK;

error:
    RedisModule_CloseKey(key);
    return REDISMODULE_ERR;
}
