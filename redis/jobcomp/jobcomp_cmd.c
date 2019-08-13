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

#include <string.h>
#include <time.h>

#include "common/iso8601_format.h"
#include "common/redis_fields.h"
#include "common/sscan_cursor.h"
#include "common/stringto.h"
#include "jobcomp_qry.h"

#define FETCH_MAX_COUNT 500

/*
 * SLURMJC.INDEX <prefix> <job>
 */
int jobcomp_cmd_index(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    RedisModule_AutoMemory(ctx);
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    const char *prefix = RedisModule_StringPtrLen(argv[1], NULL);
    const char *jobid = RedisModule_StringPtrLen(argv[2], NULL);
    RedisModuleString *keyname = RedisModule_CreateStringPrintf(ctx,
        "%s:%s", prefix, jobid);
    RedisModuleCallReply *reply = NULL;
    long long end_time;

    // Open the job key
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyname, REDISMODULE_READ);
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }
    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_HASH) {
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

#ifdef ISO8601_DATES
    end_time = mk_time(RedisModule_StringPtrLen(end, NULL));
    if (end_time == (-1)) {
        RedisModule_ReplyWithError(ctx, "invalid iso8601 end date/time");
        return REDISMODULE_ERR;
    }
#else
    if (sr_strtoll(RedisModule_StringPtrLen(end, NULL), &end_time) < 0) {
        RedisModule_ReplyWithError(ctx, "invalid end date/time");
        return REDISMODULE_ERR;
    }
#endif

    // Create or update the index
    long long end_days = end_time / SECONDS_PER_DAY;
    RedisModuleString *idx = RedisModule_CreateStringPrintf(ctx,
        "%s:idx:end:%lld", prefix, end_days);
    reply = RedisModule_Call(ctx, "SADD", "sc", idx, jobid);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
        RedisModule_ReplyWithCallReply(ctx, reply);
        return REDISMODULE_ERR;
    }
    RedisModule_FreeCallReply(reply);
    if (TTL > 0) {
        reply = RedisModule_Call(ctx, "EXPIRE", "sl", idx, TTL);
        if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
            RedisModule_ReplyWithCallReply(ctx, reply);
            return REDISMODULE_ERR;
        }
    }

    RedisModule_ReplyWithString(ctx, idx);
    return REDISMODULE_OK;
}

/*
 * SLURMJC.MATCH <prefix> <uuid>
 */
int jobcomp_cmd_match(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    RedisModule_AutoMemory(ctx);
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    int rc;
    const char *prefix = RedisModule_StringPtrLen(argv[1], NULL);
    const char *uuid = RedisModule_StringPtrLen(argv[2], NULL);
    const char *err = NULL;
    job_query_init_t init = {
        .ctx = ctx,
        .prefix = prefix,
        .uuid = uuid,
    };
    AUTO_PTR(destroy_job_query) job_query_t qry = create_job_query(&init);
    rc = job_query_prepare(qry);
    if (rc == QUERY_NULL) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }
    if (rc == QUERY_ERR) {
        job_query_error(qry, &err, NULL);
        RedisModule_ReplyWithError(ctx, err);
        return REDISMODULE_ERR;
    }
    RedisModuleString *match_set = RedisModule_CreateStringPrintf(ctx,
        "%s:mat:%s", prefix, uuid);

    // The range of index keys we will inspect
    long long start_day, end_day, day;
    job_query_start_day(qry, &start_day);
    job_query_end_day(qry, &end_day);

    // Inspect each index for jobs that match
    for (day = start_day; day <= end_day; ++day) {

        int job_match;
        const char *job_c;
        RedisModuleCallReply *reply;
        RedisModuleString *idx = RedisModule_CreateStringPrintf(ctx,
            "%s:idx:end:%lld", prefix, day);
        sscan_cursor_init_t init = {
            .ctx = ctx,
            .set = idx,
            .count = 500
        };
        AUTO_PTR(destroy_sscan_cursor) sscan_cursor_t cursor =
            create_sscan_cursor(&init);
        if (sscan_error(cursor, &err, NULL) == SSCAN_ERR) {
            RedisModule_ReplyWithError(ctx, err);
            return REDISMODULE_ERR;
        }
        do {
            rc = sscan_next_element(cursor, &job_c, NULL);
            if (rc == SSCAN_ERR) {
                sscan_error(cursor, &err, NULL);
                RedisModule_ReplyWithError(ctx, err);
                return REDISMODULE_ERR;
            }
            if ((rc == SSCAN_OK) && *job_c) {
                long long job;
                if (sr_strtoll(job_c, &job) < 0) {
                    RedisModule_ReplyWithError(ctx, "invalid job id");
                    return REDISMODULE_ERR;
                }
                job_match = job_query_match_job(qry, job);
                if (job_match == QUERY_ERR) {
                    job_query_error(qry, &err, NULL);
                    RedisModule_ReplyWithError(ctx, err);
                   return REDISMODULE_ERR;
                }
                if (job_match == QUERY_PASS) {
                    // Add ijob to sorted set with score = job so that we can
                    // pop them off later in job sorted order
                    reply = RedisModule_Call(ctx, "ZADD", "sll", match_set,
                        job, job);
                    RedisModule_FreeCallReply(reply);
                }
            }
        } while (rc != SSCAN_EOF);
    }

    RedisModuleKey *match_set_key = RedisModule_OpenKey(ctx, match_set,
        REDISMODULE_WRITE);
    if (RedisModule_KeyType(match_set_key) == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }
    if (RedisModule_KeyType(match_set_key) != REDISMODULE_KEYTYPE_ZSET) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetExpire(match_set_key, QUERY_TTL * 1000)
        == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "failed to set ttl on match set");
        return REDISMODULE_ERR;
    }

    RedisModule_ReplyWithString(ctx, match_set);
    return REDISMODULE_OK;
}

/*
 * SLURMJC.FETCH <prefix> <uuid> <max count>
 *
 * Reply is a nested array of job data.  The size of the outer array is the
 * number of jobs returned.  Each job is itself an array of MAX_REDIS_FIELDS
 * with each field of the job data in its designated array slot.  Fields may
 * contain empty (nil) data.
 *
 * This api consumes the match set as it reads it and is thus stateless.
 * The caller is expected to issue repeated calls to SLURMJC.FETCH until
 * the count of returned jobs is zero.  The max count parameter serves
 * to limit the job count of each reponse.  Receipt of fewer jobs than
 * max count is not a guarantee that all jobs have been returned.
 */
int jobcomp_cmd_fetch(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    RedisModule_AutoMemory(ctx);
    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }

    long long max_count;
    const char *prefix = RedisModule_StringPtrLen(argv[1], NULL);
    const char *uuid = RedisModule_StringPtrLen(argv[2], NULL);
    RedisModuleString *match = RedisModule_CreateStringPrintf(ctx, "%s:mat:%s",
        prefix, uuid);

    if (RedisModule_StringToLongLong(argv[3], &max_count) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, "invalid max count");
        return REDISMODULE_ERR;
    }
    if (max_count > FETCH_MAX_COUNT) {
        max_count = FETCH_MAX_COUNT;
    }
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    long long count = 0;
    while (count < max_count) {
        RedisModuleString *fields[MAX_REDIS_FIELDS];
        memset(fields, 0, sizeof(fields));
        RedisModuleCallReply *reply = RedisModule_Call(ctx, "ZPOPMIN", "sl",
            match, 100);
        if ((RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_NULL) ||
            (RedisModule_CallReplyType(reply) != REDISMODULE_REPLY_ARRAY) ||
            (RedisModule_CallReplyLength(reply)) == 0) {
            break;
        }
        size_t s = 0;
        for (; (s < RedisModule_CallReplyLength(reply)) && (count < max_count);
            s += 2) {
            RedisModuleCallReply *subreply =
                RedisModule_CallReplyArrayElement(reply, s);
            const char *job_c = RedisModule_CallReplyStringPtr(subreply, NULL);
            long long job;
            if (sr_strtoll(job_c, &job) < 0) {
                continue;
            }
            RedisModuleString *job_key_s = RedisModule_CreateStringPrintf(ctx,
                "%s:%lld", prefix, job);
            RedisModuleKey *job_key = RedisModule_OpenKey(ctx, job_key_s,
                REDISMODULE_READ);
            if (RedisModule_KeyType(job_key) == REDISMODULE_KEYTYPE_EMPTY) {
                continue;
            }
            if (RedisModule_HashGet(job_key, REDISMODULE_HASH_CFIELDS,
                redis_field_labels[0], &fields[0],
                redis_field_labels[1], &fields[1],
                redis_field_labels[2], &fields[2],
                redis_field_labels[3], &fields[3],
                redis_field_labels[4], &fields[4],
                redis_field_labels[5], &fields[5],
                redis_field_labels[6], &fields[6],
                redis_field_labels[7], &fields[7],
                redis_field_labels[8], &fields[8],
                redis_field_labels[9], &fields[9],
                redis_field_labels[10], &fields[10],
                redis_field_labels[11], &fields[11],
                redis_field_labels[12], &fields[12],
                redis_field_labels[13], &fields[13],
                redis_field_labels[14], &fields[14],
                redis_field_labels[15], &fields[15],
                redis_field_labels[16], &fields[16],
                redis_field_labels[17], &fields[17],
                redis_field_labels[18], &fields[18],
                redis_field_labels[19], &fields[19],
                redis_field_labels[20], &fields[20],
                redis_field_labels[21], &fields[21],
                redis_field_labels[22], &fields[22],
                redis_field_labels[23], &fields[23],
                redis_field_labels[24], &fields[24],
                redis_field_labels[25], &fields[25],
                redis_field_labels[26], &fields[26],
                redis_field_labels[27], &fields[27],
                NULL) == REDISMODULE_ERR) {
                    continue;
            }
            RedisModule_ReplyWithArray(ctx, MAX_REDIS_FIELDS);
            int i = 0;
            for (; i < MAX_REDIS_FIELDS; ++i) {
                if (fields[i])
                    RedisModule_ReplyWithString(ctx, fields[i]);
                else
                    RedisModule_ReplyWithNull(ctx);
            }
            ++count;
        }
    }
    RedisModule_ReplySetArrayLength(ctx, count);
    return REDISMODULE_OK;
}
