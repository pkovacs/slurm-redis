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

#include "jobcomp_qry.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "common/iso8601_format.h"

typedef struct job_query {
    RedisModuleCtx *ctx;
    const char *keytag;
    const char *uuid;
    long long start_day;
    long long end_day;
#ifdef ISO8601_DATES
    char start_time[ISO8601_SZ];
    char end_time[ISO8601_SZ];
#else
    long long start_time;
    long long end_time;
#endif
} *job_query_t;

job_query_t create_job_query(const job_query_init_t *init)
{
    job_query_t qry = RedisModule_Calloc(1, sizeof(struct job_query));
    AUTO_PTR(destroy_job_query) job_query_t qry_auto = qry;

    qry->ctx = init->ctx;
    qry->keytag = init->keytag;
    qry->uuid = init->uuid;

    RedisModuleString *keyname = RedisModule_CreateStringPrintf(qry->ctx,
        "%s:qry:%s", qry->keytag, qry->uuid);

    // Open the main query key
    RedisModuleKey *key = RedisModule_OpenKey(qry->ctx, keyname,
        REDISMODULE_READ);
    if ((RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) ||
        (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_HASH)) {
        RedisModule_ReplyWithError(qry->ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return NULL;
    }

    // Fetch the scalar criteria on main query key
    RedisModuleString *start = NULL, *end = NULL;
    if (RedisModule_HashGet(key, REDISMODULE_HASH_CFIELDS, "Start", &start,
            "End", &end, NULL) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(qry->ctx, "expected key(s) missing");
        return NULL;
    }
    const char *start_s = RedisModule_StringPtrLen(start, NULL);
    const char *end_s = RedisModule_StringPtrLen(end, NULL);
    long long start_time, end_time;

#ifdef ISO8601_DATES
    start_time = mk_time(start_s);
    if (start_time == (-1)) {
        RedisModule_ReplyWithError(qry->ctx, "invalid iso8601 start date/time");
        return NULL;
    }
    end_time = mk_time(end_s);
    if (end_time == (-1)) {
        RedisModule_ReplyWithError(qry->ctx, "invalid iso8601 end date/time");
        return NULL;
    }
    strncpy(qry->start_time, start_s, ISO8601_SZ-1);
    strncpy(qry->end_time, end_s, ISO8601_SZ-1);
#else
    start_time = strtoll(start_s, NULL, 10);
    if ((errno == ERANGE &&
            (start_time == LLONG_MAX || start_time == LLONG_MIN))
        || (errno != 0 && start_time == 0)) {
        RedisModule_ReplyWithError(qry->ctx, "invalid start time");
        return NULL;
    }
    end_time = strtoll(end_s, NULL, 10);
    if ((errno == ERANGE &&
            (end_time == LLONG_MAX || end_time == LLONG_MIN))
        || (errno != 0 && end_time == 0)) {
        RedisModule_ReplyWithError(qry->ctx, "invalid end time");
        return NULL;
    }
    qry->start_time = start_time;
    qry->end_time = end_time;
#endif
    qry->start_day = start_time / SECONDS_PER_DAY;
    qry->end_day = end_time / SECONDS_PER_DAY;

    qry_auto = NULL; // prevent qry destruction on scope close
    return qry;
}

void destroy_job_query(job_query_t *qry)
{
    if (!qry) {
        return;
    }
    RedisModule_Free(*qry);
    *qry = NULL;
}

int job_query_match_job(const job_query_t qry, long long job)
{
    assert(qry != NULL);
    assert(job > 0);

    // Open job key
    RedisModuleString *keyname = RedisModule_CreateStringPrintf(qry->ctx,
        "%s:%lld", qry->keytag, job);
    RedisModuleKey *key = RedisModule_OpenKey(qry->ctx, keyname,
        REDISMODULE_READ);
    if ((RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY) &&
        (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_HASH)) {
        return 0;
    }

    // Fetch job data
    RedisModuleString *start = NULL, *end = NULL;
    if (RedisModule_HashGet(key, REDISMODULE_HASH_CFIELDS, "Start", &start,
            "End", &end, NULL) == REDISMODULE_ERR) {
        return 0;
    }
    const char *start_s = RedisModule_StringPtrLen(start, NULL);
    const char *end_s = RedisModule_StringPtrLen(end, NULL);

#ifdef ISO8601_DATES
    if (strncmp(qry->start_time, start_s, ISO8601_SZ-1) > 0 ||
            strncmp(qry->end_time, end_s, ISO8601_SZ-1) < 0) {
        return 0;
    }
#else
    long long start_time = strtoll(start_s, NULL, 10);
    if ((errno == ERANGE &&
            (start_time == LLONG_MAX || start_time == LLONG_MIN))
        || (errno != 0 && start_time == 0)) {
        return 0;
    }
    long long end_time = strtoll(end_s, NULL, 10);
    if ((errno == ERANGE &&
            (end_time == LLONG_MAX || end_time == LLONG_MIN))
        || (errno != 0 && end_time == 0)) {
        return 0;
    }
    if (qry->start_time > start_time || qry->end_time < end_time) {
        return 0;
    }
#endif

    RedisModule_CloseKey(key);
    return 1;
}

long long job_query_start_day(const job_query_t qry)
{
    assert(qry != NULL);
    return qry->start_day;
}

long long job_query_end_day(const job_query_t qry)
{
    assert(qry != NULL);
    return qry->end_day;
}
