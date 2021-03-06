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
#include "common/sscan_cursor.h"
#include "jobcomp_auto.h"

// The redis-side representation of slurm's slurmdb_job_cond_t
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
    // nnodes range
    long long nnodes_min;
    long long nnodes_max;
    // arrays for set-based criteria
    RedisModuleString **gids;
    long long *jobs;
    RedisModuleString **jobnames;
    RedisModuleString **partitions;
    RedisModuleString **states;
    RedisModuleString **uids;
    size_t gids_sz;
    size_t jobs_sz;
    size_t jobnames_sz;
    size_t partitions_sz;
    size_t states_sz;
    size_t uids_sz;
} *job_query_t;

static int add_criteria(job_query_t qry, const RedisModuleString *key,
    RedisModuleString ***arr, size_t *len);

static int add_job_criteria(job_query_t qry, const RedisModuleString *key,
    long long **arr, size_t *len);

static int job_query_match_job(const job_query_t qry, long long jobid);

/*
 * Create a job query object, used to match jobs against provided criteria
 */
job_query_t create_job_query(const job_query_init_t *init)
{
    assert (init != NULL);
    job_query_t qry = RedisModule_Calloc(1, sizeof(struct job_query));
    qry->ctx = init->ctx;
    qry->prefix = init->prefix;
    qry->uuid = init->uuid;
    return qry;
}

/*
 * Destroy a job query object
 */
void destroy_job_query(job_query_t *qry)
{
    if (!qry) {
        return;
    }
    job_query_t q = *qry;
    size_t i = 0;
    if (q->err) {
        RedisModule_FreeString(q->ctx, q->err);
    }
    if (q->gids_sz) {
        for (i = 0; i < q->gids_sz; ++i) {
            RedisModule_FreeString(q->ctx, q->gids[i]);
        }
        RedisModule_Free(q->gids);
    }
    if (q->jobs_sz) {
        RedisModule_Free(q->jobs);
    }
    if (q->jobnames_sz) {
        for (i = 0; i < q->jobnames_sz; ++i) {
            RedisModule_FreeString(q->ctx, q->jobnames[i]);
        }
        RedisModule_Free(q->jobnames);
    }
    if (q->partitions_sz) {
        for (i = 0; i < q->partitions_sz; ++i) {
            RedisModule_FreeString(q->ctx, q->partitions[i]);
        }
        RedisModule_Free(q->partitions);
    }
    if (q->states_sz) {
        for (i = 0; i < q->states_sz; ++i) {
            RedisModule_FreeString(q->ctx, q->states[i]);
        }
        RedisModule_Free(q->states);
    }
    if (q->uids_sz) {
        for (i = 0; i < q->uids_sz; ++i) {
            RedisModule_FreeString(q->ctx, q->uids[i]);
        }
        RedisModule_Free(q->uids);
    }
    RedisModule_Free(q);
    *qry = NULL;
}

/*
 * Read the transient query keys sent to us by slurm's jobcomp_redis plugin
 * and populate the job query object with the contained job criteria
 */
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
    AUTO_RMSTR redis_module_string_t nnodes_min = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t nnodes_max = { .ctx = qry->ctx };
    char nnodes_min_label[16] = {0};
    char nnodes_max_label[16] = {0};
    snprintf(nnodes_min_label, sizeof(nnodes_min_label)-1, "%sMin",
        redis_field_labels[kNNodes]);
    snprintf(nnodes_max_label, sizeof(nnodes_min_label)-1, "%sMax",
        redis_field_labels[kNNodes]);
    if (RedisModule_HashGet(query_key, REDISMODULE_HASH_CFIELDS,
        redis_field_labels[kABI], &abi.str,
        redis_field_labels[kTimeFormat], &tmf.str,
        redis_field_labels[kStart], &start.str,
        redis_field_labels[kEnd], &end.str,
        nnodes_min_label, &nnodes_min.str,
        nnodes_max_label, &nnodes_max.str,
        NULL) == REDISMODULE_ERR) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            "error fetching query data");
        return QUERY_ERR;
    }

    // Load the start/end time criteria into the query: ISO8601 or unix epoch
    long long _tmf;
    if (RedisModule_StringToLongLong(tmf.str, &_tmf) == REDISMODULE_ERR) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx, "invalid _tmf");
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

    // Load the node count criteria into the query
    if (nnodes_min.str) {
        if (RedisModule_StringToLongLong(nnodes_min.str, &qry->nnodes_min)
            == REDISMODULE_ERR) {
            qry->err = RedisModule_CreateStringPrintf(qry->ctx,
                "invalid nnodes min value");
            return QUERY_ERR;
        }
    }

    if (nnodes_max.str) {
        if (RedisModule_StringToLongLong(nnodes_max.str, &qry->nnodes_max)
            == REDISMODULE_ERR) {
            qry->err = RedisModule_CreateStringPrintf(qry->ctx,
                "invalid nnodes max value");
            return QUERY_ERR;
        }
    }

    // Load the other set-based critiera into the query: gids, job ids,
    // job names, partitions, job states, uids, etc.
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
    if ((add_criteria(qry, gid_key.str, &qry->gids, &qry->gids_sz)
            == QUERY_ERR) ||
        (add_job_criteria(qry, job_key.str, &qry->jobs, &qry->jobs_sz)
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

/*
 * Provide the last job query error and error length to the caller
 */
int job_query_error(job_query_t qry, const char **err, size_t *len)
{
    assert(qry != NULL);
    if (qry->err && err) {
        *err = RedisModule_StringPtrLen(qry->err, len);
        return QUERY_ERR;
    }
    return QUERY_OK;
}

/*
 * Match jobs in redis against the criteria in the job query.  Matches are
 * stored on the indicated matchset key (a set of job ids)
 */
int job_query_match(job_query_t qry, const RedisModuleString *matchset)
{
    assert(qry != NULL);

    if (qry->err) {
        RedisModule_FreeString(qry->ctx, qry->err);
        qry->err = NULL;
    }

    if (qry->jobs_sz) {
        // Scan user-specified job set
        size_t i = 0;
        for (; i < qry->jobs_sz; ++i) {
            if (job_query_match_job(qry, qry->jobs[i]) == QUERY_PASS) {
                // Add job to sorted matchset
                AUTO_RMREPLY RedisModuleCallReply *reply =
                    RedisModule_Call(qry->ctx, "ZADD", "sll", matchset,
                        qry->jobs[i], qry->jobs[i]);
            }
        }
    } else {
        // Scan indices for jobs that match
        int rc;
        const char *err = NULL;
        long long start_day = qry->start_time / SECONDS_PER_DAY;
        long long end_day = qry->end_time / SECONDS_PER_DAY;
        long long day;

        for (day = start_day; day <= end_day; ++day) {

            AUTO_RMSTR redis_module_string_t idx = {
                .ctx = qry->ctx,
                .str = RedisModule_CreateStringPrintf(qry->ctx,
                    "%s:idx:end:%lld", qry->prefix, day)
            };
            sscan_cursor_init_t init = {
                .ctx = qry->ctx,
                .set = idx.str,
                .count = JCR_FETCH_COUNT
            };
            AUTO_PTR(destroy_sscan_cursor) sscan_cursor_t cursor =
                create_sscan_cursor(&init);
            if (sscan_error(cursor, &err, NULL) == SSCAN_ERR) {
                qry->err = RedisModule_CreateStringPrintf(qry->ctx, err);
                return QUERY_ERR;
            }
            do {
                AUTO_RMSTR redis_module_string_t job = { .ctx = qry->ctx };
                rc = sscan_next_element(cursor, &job.str);
                if (rc == SSCAN_ERR) {
                    sscan_error(cursor, &err, NULL);
                    qry->err = RedisModule_CreateStringPrintf(qry->ctx, err);
                    return QUERY_ERR;
                }
                if ((rc == SSCAN_OK) && job.str) {
                    long long jobid;
                    if (RedisModule_StringToLongLong(job.str, &jobid)
                        == REDISMODULE_ERR) {
                        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
                            "invalid job id");
                        return QUERY_ERR;
                    }
                    int job_match = job_query_match_job(qry, jobid);
                    if (job_match == QUERY_ERR) {
                        return QUERY_ERR;
                    }
                    if (job_match == QUERY_PASS) {
                        // Add job to sorted matchset
                        AUTO_RMREPLY RedisModuleCallReply *reply =
                            RedisModule_Call(qry->ctx, "ZADD", "sll", matchset,
                                jobid, jobid);
                    }
                }
            } while (rc != SSCAN_EOF);
        }
    }

    return QUERY_OK;
}

/*
 * Helper function which reads a key of job criteria containing a set
 * of strings.  The provided string array and array size variable are
 * loaded with those strings
 */
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
        "SMEMBERS", "s", key);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_NULL) {
        return QUERY_OK;
    }
    if (RedisModule_CallReplyType(reply) != REDISMODULE_REPLY_ARRAY) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            REDISMODULE_ERRORMSG_WRONGTYPE);
        return QUERY_ERR;
    }
    *len = RedisModule_CallReplyLength(reply);
    if (*len > 0) {
        *arr = RedisModule_Calloc(*len, sizeof(RedisModuleString *));
    }

    size_t i = 0;
    for (; i < *len; ++i) {
        RedisModuleCallReply *subreply =
            RedisModule_CallReplyArrayElement(reply, i); // no AUTO_RMREPLY
        if (RedisModule_CallReplyType(subreply) != REDISMODULE_REPLY_STRING) {
            qry->err = RedisModule_CreateStringPrintf(qry->ctx,
                REDISMODULE_ERRORMSG_WRONGTYPE);
            return QUERY_ERR;
        }
        (*arr)[i] = RedisModule_CreateStringFromCallReply(subreply);
    }

    return QUERY_OK;
}

/*
 * Helper function which reads a key of job criteria containing a set
 * of integers. The provided integer array and array size variable are
 * loaded with those integers
 */
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
        "SMEMBERS", "s", key);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_NULL) {
        return QUERY_OK;
    }
    if (RedisModule_CallReplyType(reply) != REDISMODULE_REPLY_ARRAY) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            REDISMODULE_ERRORMSG_WRONGTYPE);
        return QUERY_ERR;
    }
    *len = RedisModule_CallReplyLength(reply);
    if (*len > 0) {
        *arr = RedisModule_Calloc(*len, sizeof(long long));
    }

    size_t i = 0;
    for (; i < *len; ++i) {
        RedisModuleCallReply *subreply =
            RedisModule_CallReplyArrayElement(reply, i); // no AUTO_RMREPLY
        if (RedisModule_CallReplyType(subreply) != REDISMODULE_REPLY_STRING) {
            qry->err = RedisModule_CreateStringPrintf(qry->ctx,
                REDISMODULE_ERRORMSG_WRONGTYPE);
            return QUERY_ERR;
        }
        AUTO_RMSTR redis_module_string_t job = {
            .ctx = qry->ctx,
            .str = RedisModule_CreateStringFromCallReply(subreply)
        };
        if (RedisModule_StringToLongLong(job.str, (*arr)+i)
            == REDISMODULE_ERR) {
            return QUERY_ERR;
        }
    }

    return QUERY_OK;
}

/*
 * Helper function which looks at an individual job and determines if it
 * matches the query criteria or not
 */
static int job_query_match_job(const job_query_t qry, long long jobid)
{
    assert(qry != NULL);
    assert(jobid > 0);

    size_t i = 0;
    int match = QUERY_PASS;

    if (qry->err) {
        RedisModule_FreeString(qry->ctx, qry->err);
        qry->err = NULL;
    }

    // Open job key
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

    // Fetch data on job key
    AUTO_RMSTR redis_module_string_t abi = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t tmf = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t start = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t end = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t gid = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t nnodes = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t jobname = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t partition = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t state = { .ctx = qry->ctx };
    AUTO_RMSTR redis_module_string_t uid = { .ctx = qry->ctx };
    if (RedisModule_HashGet(job_key, REDISMODULE_HASH_CFIELDS,
        redis_field_labels[kABI], &abi.str,
        redis_field_labels[kTimeFormat], &tmf.str,
        redis_field_labels[kStart], &start.str,
        redis_field_labels[kEnd], &end.str,
        redis_field_labels[kGID], &gid.str,
        redis_field_labels[kNNodes], &nnodes.str,
        redis_field_labels[kJobName], &jobname.str,
        redis_field_labels[kPartition], &partition.str,
        redis_field_labels[kState], &state.str,
        redis_field_labels[kUID], &uid.str,
        NULL) == REDISMODULE_ERR) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx,
            "error fetching job data");
        return QUERY_ERR;
    }

    long long _tmf;
    if (RedisModule_StringToLongLong(tmf.str, &_tmf) == REDISMODULE_ERR) {
        qry->err = RedisModule_CreateStringPrintf(qry->ctx, "invalid _tmf");
        return QUERY_ERR;
    }

    // Check job time
    if (_tmf == 1) {
        const char *start_c = RedisModule_StringPtrLen(start.str, NULL);
        const char *end_c = RedisModule_StringPtrLen(end.str, NULL);
        if (strncmp(qry->start_time_c, start_c, ISO8601_SZ-1) > 0 ||
                strncmp(qry->end_time_c, end_c, ISO8601_SZ-1) < 0) {
            match = QUERY_FAIL;
        }
    } else do {
        long long start_time, end_time;
        if (RedisModule_StringToLongLong(start.str, &start_time)
            == REDISMODULE_ERR) {
            match = QUERY_FAIL;
            break;
        }
        if (RedisModule_StringToLongLong(end.str, &end_time)
            == REDISMODULE_ERR) {
            match = QUERY_FAIL;
            break;
        }
        if (qry->start_time > start_time || qry->end_time < end_time) {
            match = QUERY_FAIL;
        }
    } while (0);
    if (match == QUERY_FAIL) {
        return match;
    }

    // Check gid
    if (qry->gids_sz) {
        match = QUERY_FAIL;
        if (gid.str) {
            for (i = 0; i < qry->gids_sz; ++i) {
                if (strcmp(RedisModule_StringPtrLen(qry->gids[i], NULL),
                        RedisModule_StringPtrLen(gid.str, NULL)) == 0 ) {
                    match = QUERY_PASS;
                    break;
                }
            }
        }
    }
    if (match == QUERY_FAIL) {
        return match;
    }

    // Check nnodes_min/max
    if ((qry->nnodes_min > 0) || (qry->nnodes_max > 0)) {
        match = QUERY_FAIL;
        if (nnodes.str) {
            long long nds;
            if (RedisModule_StringToLongLong(nnodes.str, &nds)
                == REDISMODULE_OK) {
                if (qry->nnodes_min <= nds) {
                    match = QUERY_PASS;
                }
                if ((qry->nnodes_max > 0) && (nds > qry->nnodes_max)) {
                    match = QUERY_FAIL;
                }
            }
        }
    }
    if (match == QUERY_FAIL) {
        return match;
    }

    // Check jobname
    if (qry->jobnames_sz) {
        match = QUERY_FAIL;
        if (jobname.str) {
            for (i = 0; i < qry->jobnames_sz; ++i) {
                if (strcmp(RedisModule_StringPtrLen(qry->jobnames[i], NULL),
                        RedisModule_StringPtrLen(jobname.str, NULL)) == 0 ) {
                    match = QUERY_PASS;
                    break;
                }
            }
        }
    }
    if (match == QUERY_FAIL) {
        return match;
    }

    // Check partition
    if (qry->partitions_sz) {
        match = QUERY_FAIL;
        if (partition.str) {
            for (i = 0; i < qry->partitions_sz; ++i) {
                if (strcmp(RedisModule_StringPtrLen(qry->partitions[i], NULL),
                        RedisModule_StringPtrLen(partition.str, NULL)) == 0 ) {
                    match = QUERY_PASS;
                    break;
                }
            }
        }
    }
    if (match == QUERY_FAIL) {
        return match;
    }

    // Check job state
    if (qry->states_sz) {
        match = QUERY_FAIL;
        if (state.str) {
            for (i = 0; i < qry->states_sz; ++i) {
                if (strcmp(RedisModule_StringPtrLen(qry->states[i], NULL),
                        RedisModule_StringPtrLen(state.str, NULL)) == 0 ) {
                    match = QUERY_PASS;
                    break;
                }
            }
        }
    }
    if (match == QUERY_FAIL) {
        return match;
    }

    // Check uid
    if (qry->uids_sz) {
        match = QUERY_FAIL;
        if (uid.str) {
            for (i = 0; i < qry->uids_sz; ++i) {
                if (strcmp(RedisModule_StringPtrLen(qry->uids[i], NULL),
                        RedisModule_StringPtrLen(uid.str, NULL)) == 0 ) {
                    match = QUERY_PASS;
                    break;
                }
            }
        }
    }
    return match;
}
