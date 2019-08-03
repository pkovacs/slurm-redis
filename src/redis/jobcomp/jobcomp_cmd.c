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

#include <errno.h>
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

struct job_criteria {
    const char *keytag;
    const char *uuid;
    long long start_time;
    long long end_time;
    // etc. from the qry keys
} job_criteria_t;

static job_criteria_t * create_job_criteria()
{
    return NULL;
}

static void destroy_job_criteria(job_criteria_t *criteria)
{
}

static int accept_job(RedisModuleCtx *ctx, const job_criteria_t *criteria,
                      long long job)
{
    return 1;
}

static int create_match_set(RedisModuleCtx *ctx, long long start_time,
                            long long end_time, const char *keytag,
                            const char *uuid)
{
    // Identify index range
    long long start_day = start_time / SECONDS_PER_DAY;
    long long end_day = end_time / SECONDS_PER_DAY;

    long long day = start_day;
    for (; day <= end_day; ++day) {

        // The current index we will inspect for jobs
        RedisModuleString *idx = RedisModule_CreateStringPrintf(ctx,
            "%s:idx:%ld:end", keytag, day);

        long long cursor = 0;
        do {
            RedisModuleCallReply *reply = NULL;
            RedisModuleCallReply *subreply_cursor = NULL;
            RedisModuleCallReply *subreply_job_array = NULL;

            // Open a redis cursor for this index
            reply = RedisModule_Call(ctx, "SSCAN","slcl", idx, cursor, "COUNT",
                100);
            if ((RedisModule_CallReplyType(reply) != REDISMODULE_REPLY_ARRAY) ||
                (RedisModule_CallReplyLength(reply) != 2)) {
                RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
                return REDISMODULE_ERR;
            }

            subreply_cursor = RedisModule_CallReplyArrayElement(reply, 0);
            if ((RedisModule_CallReplyType(subreply_cursor)
                != REDISMODULE_REPLY_STRING)) {
                RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
                return REDISMODULE_ERR;
            }
            subreply_job_array = RedisModule_CallReplyArrayElement(reply, 1);
            if (!subreply_job_array) {
                // Empty or non-existent index
                continue;
            }
            if ((RedisModule_CallReplyType(subreply_job_array)
                != REDISMODULE_REPLY_ARRAY)) {
                RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
                return REDISMODULE_ERR;
            }

            // Cursor value for next loop
            cursor = strtoll(RedisModule_CallReplyStringPtr(subreply_cursor,
                NULL), NULL, 10);
            if ((errno == ERANGE &&
                    (cursor == LLONG_MAX || cursor == LLONG_MIN))
                || (errno != 0 && cursor == 0)) {
                RedisModule_ReplyWithError(ctx, "invalid cursor");
                return REDISMODULE_ERR;
            }

            // Fetch each job id
            size_t j = 0;
            for (; j < RedisModule_CallReplyLength(subreply_job_array); ++j) {
                RedisModuleCallReply *subreply_job =
                    RedisModule_CallReplyArrayElement(subreply_job_array, j);
                if (!subreply_job) {
                    continue;
                }
                long long job = strtoll(RedisModule_CallReplyStringPtr(
                    subreply_job, NULL), NULL, 10);
                if ((errno == ERANGE &&
                        (cursor == LLONG_MAX || cursor == LLONG_MIN))
                    || (errno != 0 && cursor == 0)) {
                    RedisModule_ReplyWithError(ctx, "invalid job id");
                    return REDISMODULE_ERR;
                }

                if (accept_job(ctx, job, start_time, end_time, keytag, uuid)) {
                    RedisModule_Call(ctx, "SADD", "cl", "tmp", job);
                }
            }
        } while (cursor);
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

    // Fetch the end time
    RedisModuleString *end = NULL;
    if (RedisModule_HashGet(key, REDISMODULE_HASH_CFIELDS, "End", &end, NULL)
        == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "end date missing");
        return REDISMODULE_ERR;
    }

    // Create or update the end index
    long long end_days = 0;
    if (days_since_unix_epoch(RedisModule_StringPtrLen(end, NULL), &end_days,
        NULL) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "failed to parse end date");
        return REDISMODULE_ERR;
    }
    RedisModuleString *idx_end = RedisModule_CreateStringPrintf(ctx,
        "%s:idx:%ld:end", keytag, end_days);
    reply = RedisModule_Call(ctx, "SADD", "sc", idx_end, jobid);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
        RedisModule_ReplyWithCallReply(ctx, reply);
        return REDISMODULE_ERR;
    }

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

int jobcomp_cmd_match(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    RedisModule_AutoMemory(ctx);
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

    long long start_time, end_time;
    start_time = strtoll(RedisModule_StringPtrLen(start, NULL), NULL, 10);
    if ((errno == ERANGE &&
            (start_time == LLONG_MAX || start_time == LLONG_MIN))
        || (errno != 0 && start_time == 0)) {
        RedisModule_ReplyWithError(ctx, "invalid start time");
        return REDISMODULE_ERR;
    }
    end_time = strtol(RedisModule_StringPtrLen(end, NULL), NULL, 10);
    if ((errno == ERANGE &&
            (end_time == LLONG_MAX || end_time == LLONG_MIN))
        || (errno != 0 && end_time == 0)) {
        RedisModule_ReplyWithError(ctx, "invalid end time");
        return REDISMODULE_ERR;
    }

    // Build a set of matching jobs
    if (create_match_set(ctx, start_time, end_time, keytag, uuid)
        == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}
