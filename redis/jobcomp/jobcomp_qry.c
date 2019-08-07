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
#include <stdio.h> //sscanf
#include <stdlib.h>
#include <time.h>  //timegm

typedef struct job_query {
    RedisModuleCtx *ctx;
    const char *keytag;
    const char *uuid;
    long long start_time;
    long long end_time;
} *job_query_t;

// Create a job query
job_query_t create_job_query(const job_query_init_t *init)
{
    job_query_t qry = RedisModule_Calloc(1, sizeof(struct job_query));
    AUTO_PTR(destroy_job_query) job_query_t qry_auto = qry;

    qry->ctx = init->ctx;
    qry->keytag = init->keytag;
    qry->uuid = init->uuid;
    qry->start_time = init->start_time;
    qry->end_time = init->end_time;
#if 0
    // Grab key with scalar critera
    RedisModuleString *keyname = RedisModule_CreateStringPrintf(init->ctx,
        "%s:qry:%s", init->keytag, init->uuid);
    RedisModuleKey *key = RedisModule_OpenKey(init->ctx, keyname, REDISMODULE_READ);
    if ((RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) ||
        (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_HASH)) {
        RedisModule_ReplyWithError(init->ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return NULL;
    }

    RedisModuleString *start = NULL, *end = NULL;
    if (RedisModule_HashGet(key, REDISMODULE_HASH_CFIELD, "Start", &start,
            "End", &end, NULL) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(init->ctx,
    }
#endif
    qry_auto = NULL;
    return qry;
}

// Destroy a job query
void destroy_job_query(job_query_t *qry)
{
    if (!qry) {
        return;
    }
    RedisModule_Free(*qry);
    *qry = NULL;
}

// Check if a job matches job query criteria
int job_query_match_job(const job_query_t qry, long long job)
{
    assert(qry != NULL);
    assert (job > 0);

    int match = 0;

    // Fetch job data
    RedisModuleString *keyname = RedisModule_CreateStringPrintf(qry->ctx,
        "%s:%lld", qry->keytag, job);
    RedisModuleKey *key = RedisModule_OpenKey(qry->ctx, keyname,
        REDISMODULE_READ);
    if ((RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY) &&
        (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_HASH)) {
        //RedisModule_ReplyWithError(qry->ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return match;
    }

    RedisModuleString *start = NULL, *end = NULL;
    if (RedisModule_HashGet(key, REDISMODULE_HASH_CFIELDS, "Start", &start,
            "End", &end, NULL) == REDISMODULE_ERR) {
        //RedisModule_ReplyWithError(qry->ctx, "expected key(s) missing");
        return match;
    }
    RedisModule_CloseKey(key);

    const char *start_iso8601 = RedisModule_StringPtrLen(start, NULL);
    const char *end_iso8601 = RedisModule_StringPtrLen(end, NULL);
    int y, M, d, h, m, s;
    if (sscanf(start_iso8601, "%d-%d-%dT%d:%d:%d", &y, &M, &d, &h, &m, &s)
        != 6) {
        return match;
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
    if (sscanf(end_iso8601, "%d-%d-%dT%d:%d:%d", &y, &M, &d, &h, &m, &s)
        != 6) {
        return match;
    }
    tm_date.tm_year = y - 1900;
    tm_date.tm_mon = M - 1;
    tm_date.tm_mday = d;
    tm_date.tm_hour = h;
    tm_date.tm_min = m;
    tm_date.tm_sec = s;
    tm_date.tm_isdst = 01;
    time_t end_time = timegm(&tm_date);

    //RedisModule_Log(qry->ctx, "notice", "%lld <= (%ld, %ld) <= %lld",
    //    qry->start_time, start_time, end_time, qry->end_time);

    if ((qry->start_time <= start_time) && (end_time <= qry->end_time)) {
        match = 1;
    }
    return match;
}
