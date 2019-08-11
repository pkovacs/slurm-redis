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
    RedisModuleString *err;
    const char *prefix;
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
    assert (init != NULL);
    job_query_t qry = RedisModule_Calloc(1, sizeof(struct job_query));
    qry->ctx = init->ctx;
    qry->prefix = init->prefix;
    qry->uuid = init->uuid;
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

int job_query_prepare(job_query_t qry)
{
    assert(qry != NULL);

    // Open the primary query key
    RedisModuleString *primary_s = RedisModule_CreateStringPrintf(qry->ctx,
        "%s:qry:%s", qry->prefix, qry->uuid);
    RedisModuleKey *primary = RedisModule_OpenKey(qry->ctx, primary_s,
        REDISMODULE_READ);
    if (RedisModule_KeyType(primary) == REDISMODULE_KEYTYPE_EMPTY) {
        return QUERY_NULL;
    }
    if (RedisModule_KeyType(primary) != REDISMODULE_KEYTYPE_HASH) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            REDISMODULE_ERRORMSG_WRONGTYPE);
        return QUERY_ERR;
    }

    // Fetch the scalar criteria on primary query key
    RedisModuleString *start = NULL, *end = NULL;
    if (RedisModule_HashGet(primary, REDISMODULE_HASH_CFIELDS, "Start", &start,
            "End", &end, NULL) == REDISMODULE_ERR) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            "expected key(s) missing");
        return QUERY_ERR;
    }

    const char *start_c = RedisModule_StringPtrLen(start, NULL);
    const char *end_c = RedisModule_StringPtrLen(end, NULL);
    long long start_time, end_time;

#ifdef ISO8601_DATES
    start_time = mk_time(start_c);
    if (start_time == (-1)) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            "invalid iso8601 start date/time");
        return QUERY_ERR;
    }
    end_time = mk_time(end_c);
    if (end_time == (-1)) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            "invalid iso8601 end date/time");
        return QUERY_ERR;
    }
    strncpy(qry->start_time, start_c, ISO8601_SZ-1);
    strncpy(qry->end_time, end_c, ISO8601_SZ-1);
#else
    start_time = strtoll(start_c, NULL, 10);
    if ((errno == ERANGE &&
            (start_time == LLONG_MAX || start_time == LLONG_MIN))
        || (errno != 0 && start_time == 0)) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            "invalid start time");
        return QUERY_ERR;
    }
    end_time = strtoll(end_c, NULL, 10);
    if ((errno == ERANGE &&
            (end_time == LLONG_MAX || end_time == LLONG_MIN))
        || (errno != 0 && end_time == 0)) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            "invalid end time");
        return QUERY_ERR;
    }
    qry->start_time = start_time;
    qry->end_time = end_time;
#endif
    qry->start_day = start_time / SECONDS_PER_DAY;
    qry->end_day = end_time / SECONDS_PER_DAY;

    return QUERY_OK;
}

int job_query_error(job_query_t qry, const char **err, size_t *len)
{
    assert(qry != NULL);

    if (qry->err && err) {
        *err = RedisModule_StringPtrLen(qry->err, len);
        return QUERY_ERR;
    }
    return QUERY_OK;
}

int job_query_match_job(const job_query_t qry, long long job)
{
    assert(qry != NULL);
    assert(job > 0);

    // Open job key
    RedisModuleString *job_s = RedisModule_CreateStringPrintf(qry->ctx,
        "%s:%lld", qry->prefix, job);
    RedisModuleKey *jobkey = RedisModule_OpenKey(qry->ctx, job_s,
        REDISMODULE_READ);
    if (RedisModule_KeyType(jobkey) == REDISMODULE_KEYTYPE_EMPTY) {
        return QUERY_NULL;
    }
    if (RedisModule_KeyType(jobkey) != REDISMODULE_KEYTYPE_HASH) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            REDISMODULE_ERRORMSG_WRONGTYPE);
        return QUERY_ERR;
    }

    // Fetch job data
    RedisModuleString *start = NULL, *end = NULL;
    if (RedisModule_HashGet(jobkey, REDISMODULE_HASH_CFIELDS, "Start", &start,
            "End", &end, NULL) == REDISMODULE_ERR) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            "error fetching job");
        return QUERY_ERR;
    }
    const char *start_c = RedisModule_StringPtrLen(start, NULL);
    const char *end_c = RedisModule_StringPtrLen(end, NULL);

#ifdef ISO8601_DATES
    if (strncmp(qry->start_time, start_c, ISO8601_SZ-1) > 0 ||
            strncmp(qry->end_time, end_c, ISO8601_SZ-1) < 0) {
        return QUERY_FAIL;
    }
#else
    long long start_time = strtoll(start_c, NULL, 10);
    if ((errno == ERANGE &&
            (start_time == LLONG_MAX || start_time == LLONG_MIN))
        || (errno != 0 && start_time == 0)) {
        return QUERY_FAIL;
    }
    long long end_time = strtoll(end_c, NULL, 10);
    if ((errno == ERANGE &&
            (end_time == LLONG_MAX || end_time == LLONG_MIN))
        || (errno != 0 && end_time == 0)) {
        return QUERY_FAIL;
    }
    if (qry->start_time > start_time || qry->end_time < end_time) {
        return QUERY_FAIL;
    }
#endif

    RedisModule_CloseKey(jobkey);
    return QUERY_PASS;
}

int job_query_start_day(const job_query_t qry, long long *start_day)
{
    assert(qry != NULL);
    if (start_day) {
        *start_day = qry->start_day;
    }
    return QUERY_OK;
}

int job_query_end_day(const job_query_t qry, long long *end_day)
{
    assert(qry != NULL);
    if (end_day) {
        *end_day = qry->end_day;
    }
    return QUERY_OK;
}
