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

#include "src/redis/common/sscan_cursor.h"

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

#if 0
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
#endif

static int create_match_set(RedisModuleCtx *ctx, long long start_time,
                            long long end_time, const char *keytag,
                            const char *uuid)
{
    // Identify index range
    long long start_day = start_time / SECONDS_PER_DAY;
    long long end_day = end_time / SECONDS_PER_DAY;

    long long day = start_day;
    for (; day <= end_day; ++day) {

        sscan_cursor_init_t init = {
            .ctx = ctx,
            .set = RedisModule_CreateStringPrintf(ctx, "%s:idx:%ld:end",
                keytag, day),
            .count = 100
        };
        const char *element, *err;
        size_t element_sz, err_sz;

        sscan_cursor_t cursor = open_sscan_cursor(&init);
        err = sscan_error(cursor, &err_sz);
        if (err) {
            RedisModule_ReplyWithError(ctx, err);
            close_sscan_cursor(cursor);
            return REDISMODULE_ERR;
        }
        do {
            element = sscan_next_element(cursor, &element_sz);
            err = sscan_error(cursor, &err_sz);
            if (err) {
                RedisModule_ReplyWithError(ctx, err);
                close_sscan_cursor(cursor);
                return REDISMODULE_ERR;
            }
            if (element) {
                long long job = strtoll(element, NULL, 10);
                if ((errno == ERANGE &&
                        (job == LLONG_MAX || job == LLONG_MIN))
                    || (errno != 0 && job == 0)) {
                    RedisModule_ReplyWithError(ctx, "invalid job id");
                    close_sscan_cursor(cursor);
                    return REDISMODULE_ERR;
                }

                //if (accept_job(ctx, job, start_time, end_time, keytag, uuid)) {
                    RedisModule_Call(ctx, "SADD", "cl", "tmp", job);
                //}
            }
        } while (element);
        close_sscan_cursor(cursor);
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
