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

#include "jobcomp_query.h"

#include <assert.h>
#include <string.h>

#include "common/iso8601_format.h"
#include "jobcomp_auto.h"

typedef struct job_query {
    RedisModuleCtx *ctx;
    RedisModuleString *err;
    const char *prefix;
    const char *uuid;
    // secs since unix epoch
    long long start_time;
    long long end_time;
    // iso8601 date/times w/tz "Z"
    char start_time_c[ISO8601_SZ];
    char end_time_c[ISO8601_SZ];
    // set-based criteria: arrays and array sizes
    RedisModuleString **accounts;
    RedisModuleString **clusters;
    RedisModuleString **gids;
    long long *jobs;
    RedisModuleString **jobnames;
    RedisModuleString **partitions;
    RedisModuleString **states;
    RedisModuleString **uids;
    size_t accounts_sz;
    size_t clusters_sz;
    size_t gids_sz;
    size_t jobs_sz;
    size_t jobnames_sz;
    size_t partitions_sz;
    size_t states_sz;
    size_t uids_sz;
} *job_query_t;

static int add_criteria(job_query_t qry, const RedisModuleString *key,
    RedisModuleString ***arr, size_t *len)
{
    assert(qry != NULL);
    assert(key != NULL);
    assert(arr != NULL);
    assert(len != NULL);

    if (qry->err) {
        RedisModule_FreeString(qry->ctx, qry->err);
        qry->err = NULL;
    }

    AUTO_RMREPLY RedisModuleCallReply *reply = RedisModule_Call(qry->ctx,
        "SCARD", "s", key);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_NULL) {
        return QUERY_OK;
    }
    if (RedisModule_CallReplyType(reply) != REDISMODULE_REPLY_INTEGER) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            REDISMODULE_ERRORMSG_WRONGTYPE);
        return QUERY_ERR;
    }
    *len = RedisModule_CallReplyInteger(reply);
    if (*len > 0) {
        *arr = RedisModule_Calloc(*len, sizeof(RedisModuleString *));
    }

    size_t i = 0;
    for (; i < *len; ++i) {
        AUTO_RMREPLY RedisModuleCallReply *reply = RedisModule_Call(qry->ctx,
            "SPOP", "s", key);
        if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
            qry->err = RedisModule_CreateStringFromCallReply(reply);
            return QUERY_ERR;
        }
        if (RedisModule_CallReplyType(reply) != REDISMODULE_REPLY_STRING) {
            qry->err = RedisModule_CreateStringPrintf(qry->ctx,
                REDISMODULE_ERRORMSG_WRONGTYPE);
            return QUERY_ERR;
        }
        *arr[i] = RedisModule_CreateStringFromCallReply(reply);
    }

    return QUERY_OK;
}

static int add_job_criteria(job_query_t qry, const RedisModuleString *key,
    long long **arr, size_t *len)
{
    assert(qry != NULL);
    assert(key != NULL);
    assert(arr != NULL);
    assert(len != NULL);

    if (qry->err) {
        RedisModule_FreeString(qry->ctx, qry->err);
        qry->err = NULL;
    }

    AUTO_RMREPLY RedisModuleCallReply *reply = RedisModule_Call(qry->ctx,
        "SCARD", "s", key);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_NULL) {
        return QUERY_OK;
    }
    if (RedisModule_CallReplyType(reply) != REDISMODULE_REPLY_INTEGER) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            REDISMODULE_ERRORMSG_WRONGTYPE);
        return QUERY_ERR;
    }
    *len = RedisModule_CallReplyInteger(reply);
    if (*len > 0) {
        *arr = RedisModule_Calloc(*len, sizeof(RedisModuleString *));
    }

    size_t i = 0;
    for (; i < *len; ++i) {
        AUTO_RMREPLY RedisModuleCallReply *reply = RedisModule_Call(qry->ctx,
            "SPOP", "s", key);
        if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
            qry->err = RedisModule_CreateStringFromCallReply(reply);
            return QUERY_ERR;
        }
        if (RedisModule_CallReplyType(reply) != REDISMODULE_REPLY_STRING) {
            qry->err = RedisModule_CreateStringPrintf(qry->ctx,
                REDISMODULE_ERRORMSG_WRONGTYPE);
            return QUERY_ERR;
        }
        AUTO_RMSTR redis_module_string_t job = {
            .ctx = qry->ctx,
            .str = RedisModule_CreateStringFromCallReply(reply)
        };
        if (RedisModule_StringToLongLong(job.str, &*arr[i])
            == REDISMODULE_ERR) {
            return QUERY_ERR;
        }
    }

    return QUERY_OK;
}

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
    job_query_t q = *qry;
    if (q->err) {
        RedisModule_FreeString(q->ctx, q->err);
        q->err = NULL;
    }
    if (q->accounts_sz) {
        size_t i = 0;
        for (; i < q->accounts_sz; ++i) {
            RedisModule_FreeString(q->ctx, q->accounts[i]);
        }
        RedisModule_Free(q->accounts);
        q->accounts = NULL;
    }
    if (q->clusters_sz) {
        size_t i = 0;
        for (; i < q->clusters_sz; ++i) {
            RedisModule_FreeString(q->ctx, q->clusters[i]);
        }
        RedisModule_Free(q->clusters);
        q->clusters = NULL;
    }
    if (q->gids_sz) {
        size_t i = 0;
        for (; i < q->gids_sz; ++i) {
            RedisModule_FreeString(q->ctx, q->gids[i]);
        }
        RedisModule_Free(q->gids);
        q->gids = NULL;
    }
    if (q->jobs_sz) {
        RedisModule_Free(q->jobs);
        q->jobs = NULL;
    }
    if (q->jobnames_sz) {
        size_t i = 0;
        for (; i < q->jobnames_sz; ++i) {
            RedisModule_FreeString(q->ctx, q->jobnames[i]);
        }
        RedisModule_Free(q->jobnames);
        q->jobnames = NULL;
    }
    if (q->partitions_sz) {
        size_t i = 0;
        for (; i < q->partitions_sz; ++i) {
            RedisModule_FreeString(q->ctx, q->partitions[i]);
        }
        RedisModule_Free(q->partitions);
        q->partitions = NULL;
    }
    if (q->states_sz) {
        size_t i = 0;
        for (; i < q->states_sz; ++i) {
            RedisModule_FreeString(q->ctx, q->states[i]);
        }
        RedisModule_Free(q->states);
        q->states = NULL;
    }
    if (q->uids_sz) {
        size_t i = 0;
        for (; i < q->uids_sz; ++i) {
            RedisModule_FreeString(q->ctx, q->uids[i]);
        }
        RedisModule_Free(q->uids);
        q->uids = NULL;
    }
    RedisModule_Free(q);
    *qry = NULL;
}

int job_query_prepare(job_query_t qry)
{
    assert(qry != NULL);

    // Open query key
    AUTO_RMSTR redis_module_string_t query_keyname = {
        .ctx = qry->ctx,
        .str = RedisModule_CreateStringPrintf(qry->ctx, "%s:qry:%s",
            qry->prefix, qry->uuid)
    };
    AUTO_RMKEY RedisModuleKey *query_key = RedisModule_OpenKey(qry->ctx,
        query_keyname.str, REDISMODULE_READ);
    if (RedisModule_KeyType(query_key) == REDISMODULE_KEYTYPE_EMPTY) {
        return QUERY_NULL;
    }
    if (RedisModule_KeyType(query_key) != REDISMODULE_KEYTYPE_HASH) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            REDISMODULE_ERRORMSG_WRONGTYPE);
        return QUERY_ERR;
    }

    // Fetch scalar criteria
    AUTO_RMSTR redis_module_string_t abi = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t tmf = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t start = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t end = { .ctx = qry->ctx };
    if (RedisModule_HashGet(query_key, REDISMODULE_HASH_CFIELDS,
        redis_field_labels[kABI], &abi.str,
        redis_field_labels[kTimeFormat], &tmf.str,
        redis_field_labels[kStart], &start.str,
        redis_field_labels[kEnd], &end.str,
        NULL) == REDISMODULE_ERR) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            "error fetching query data");
        return QUERY_ERR;
    }

    long long _tmf;
    if (RedisModule_StringToLongLong(tmf.str, &_tmf) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(qry->ctx, "invalid _tmf");
        return QUERY_ERR;
    }

    long long start_time, end_time;
    if (_tmf == 1) {
        const char *start_c = RedisModule_StringPtrLen(start.str, NULL);
        const char *end_c = RedisModule_StringPtrLen(end.str, NULL);
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
        strncpy(qry->start_time_c, start_c, ISO8601_SZ-1);
        strncpy(qry->end_time_c, end_c, ISO8601_SZ-1);
    } else {
        if (RedisModule_StringToLongLong(start.str, &start_time)
            == REDISMODULE_ERR) {
            qry->err = RedisModule_CreateStringPrintf(qry->ctx,
                "invalid start time");
            return QUERY_ERR;
        }
        if (RedisModule_StringToLongLong(end.str, &end_time)
            == REDISMODULE_ERR) {
            qry->err = RedisModule_CreateStringPrintf(qry->ctx,
                "invalid end time");
            return QUERY_ERR;
        }
    }
    qry->start_time = start_time;
    qry->end_time = end_time;

    // Fetch set-based critiera
    AUTO_RMSTR redis_module_string_t account_key = {
        .ctx = qry->ctx,
        .str = RedisModule_CreateStringPrintf(qry->ctx, "%s:qry:%s:act",
            qry->prefix, qry->uuid)
    };
    AUTO_RMSTR redis_module_string_t cluster_key = {
        .ctx = qry->ctx,
        .str = RedisModule_CreateStringPrintf(qry->ctx, "%s:qry:%s:cls",
            qry->prefix, qry->uuid)
    };
    AUTO_RMSTR redis_module_string_t gid_key = {
        .ctx = qry->ctx,
        .str = RedisModule_CreateStringPrintf(qry->ctx, "%s:qry:%s:gid",
            qry->prefix, qry->uuid)
    };
    AUTO_RMSTR redis_module_string_t job_key = {
        .ctx = qry->ctx,
        .str = RedisModule_CreateStringPrintf(qry->ctx, "%s:qry:%s:job",
            qry->prefix, qry->uuid)
    };
    AUTO_RMSTR redis_module_string_t jobname_key = {
        .ctx = qry->ctx,
        .str = RedisModule_CreateStringPrintf(qry->ctx, "%s:qry:%s:jnm",
            qry->prefix, qry->uuid)
    };
    AUTO_RMSTR redis_module_string_t partition_key = {
        .ctx = qry->ctx,
        .str = RedisModule_CreateStringPrintf(qry->ctx, "%s:qry:%s:prt",
            qry->prefix, qry->uuid)
    };
    AUTO_RMSTR redis_module_string_t state_key = {
        .ctx = qry->ctx,
        .str = RedisModule_CreateStringPrintf(qry->ctx, "%s:qry:%s:stt",
            qry->prefix, qry->uuid)
    };
    AUTO_RMSTR redis_module_string_t uid_key = {
        .ctx = qry->ctx,
        .str = RedisModule_CreateStringPrintf(qry->ctx, "%s:qry:%s:uid",
            qry->prefix, qry->uuid)
    };
    if ((add_criteria(qry, account_key.str, &qry->accounts, &qry->accounts_sz)
            == QUERY_ERR) ||
        (add_criteria(qry, cluster_key.str, &qry->clusters, &qry->clusters_sz)
            == QUERY_ERR) ||
        (add_criteria(qry, gid_key.str, &qry->gids, &qry->gids_sz)
            == QUERY_ERR) ||
        (add_job_criteria(qry, gid_key.str, &qry->jobs, &qry->jobs_sz)
            == QUERY_ERR) ||
        (add_criteria(qry, jobname_key.str, &qry->jobnames, &qry->jobnames_sz)
            == QUERY_ERR) ||
        (add_criteria(qry, partition_key.str, &qry->partitions,
            &qry->partitions_sz) == QUERY_ERR) ||
        (add_criteria(qry, state_key.str, &qry->states, &qry->states_sz)
            == QUERY_ERR) ||
        (add_criteria(qry, uid_key.str, &qry->uids, &qry->uids_sz)
            == QUERY_ERR)) {
        return QUERY_ERR;
    }

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

int job_query_match_job(const job_query_t qry, long long jobid)
{
    assert(qry != NULL);
    assert(jobid > 0);

    if (qry->err) {
        RedisModule_FreeString(qry->ctx, qry->err);
        qry->err = NULL;
    }

    // Open the job key
    AUTO_RMSTR redis_module_string_t job_keyname = {
        .ctx = qry->ctx,
        .str = RedisModule_CreateStringPrintf(qry->ctx, "%s:%lld",
            qry->prefix, jobid)
    };
    AUTO_RMKEY RedisModuleKey *job_key = RedisModule_OpenKey(qry->ctx,
        job_keyname.str, REDISMODULE_READ);
    if (RedisModule_KeyType(job_key) == REDISMODULE_KEYTYPE_EMPTY) {
        return QUERY_NULL;
    }
    if (RedisModule_KeyType(job_key) != REDISMODULE_KEYTYPE_HASH) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            REDISMODULE_ERRORMSG_WRONGTYPE);
        return QUERY_ERR;
    }

    // Fetch data on the job key
    AUTO_RMSTR redis_module_string_t abi = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t tmf = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t start = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t end = { .ctx = qry->ctx };
    if (RedisModule_HashGet(job_key, REDISMODULE_HASH_CFIELDS,
        redis_field_labels[kABI], &abi.str,
        redis_field_labels[kTimeFormat], &tmf.str,
        redis_field_labels[kStart], &start.str,
        redis_field_labels[kEnd], &end.str,
        NULL) == REDISMODULE_ERR) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            "error fetching job data");
        return QUERY_ERR;
    }

    long long _tmf;
    if (RedisModule_StringToLongLong(tmf.str, &_tmf) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(qry->ctx, "invalid _tmf");
        return QUERY_ERR;
    }

    if (_tmf == 1) {
        const char *start_c = RedisModule_StringPtrLen(start.str, NULL);
        const char *end_c = RedisModule_StringPtrLen(end.str, NULL);
        if (strncmp(qry->start_time_c, start_c, ISO8601_SZ-1) > 0 ||
                strncmp(qry->end_time_c, end_c, ISO8601_SZ-1) < 0) {
            return QUERY_FAIL;
        }
    } else {
        long long start_time, end_time;
        if (RedisModule_StringToLongLong(start.str, &start_time)
            == REDISMODULE_ERR) {
            return QUERY_FAIL;
        }
        if (RedisModule_StringToLongLong(end.str, &end_time)
            == REDISMODULE_ERR) {
            return QUERY_FAIL;
        }
        if (qry->start_time > start_time || qry->end_time < end_time) {
            return QUERY_FAIL;
        }
    }

    return QUERY_PASS;
}

int job_query_start_day(const job_query_t qry, long long *start_day)
{
    assert(qry != NULL);
    if (start_day) {
        *start_day = qry->start_time / SECONDS_PER_DAY;
    }
    return QUERY_OK;
}

int job_query_end_day(const job_query_t qry, long long *end_day)
{
    assert(qry != NULL);
    if (end_day) {
        *end_day = qry->end_time / SECONDS_PER_DAY;
    }
    return QUERY_OK;
}
