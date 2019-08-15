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

#include "jobcomp_command.h"

#include <string.h>
#include <time.h>

#include "common/iso8601_format.h"
#include "common/redis_fields.h"
#include "common/sscan_cursor.h"
#include "jobcomp_auto.h"
#include "jobcomp_query.h"

#define FETCH_MAX_COUNT 500

/*
 * SLURMJC.INDEX <prefix> <jobid>
 */
int jobcomp_cmd_index(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    RedisModule_AutoMemory(ctx);
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    const char *prefix = RedisModule_StringPtrLen(argv[1], NULL);
    const char *jobid = RedisModule_StringPtrLen(argv[2], NULL);
    AUTO_RMSTR redis_module_string_t job_keyname = {
        .ctx = ctx,
        .str = RedisModule_CreateStringPrintf(ctx, "%s:%s", prefix, jobid)
    };

    // Open the job key
    AUTO_RMKEY RedisModuleKey *key = RedisModule_OpenKey(ctx,
        job_keyname.str, REDISMODULE_READ);
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }
    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_HASH) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    // Fetch the needed job data
    AUTO_RMSTR redis_module_string_t abi = { .ctx = ctx };
    AUTO_RMSTR redis_module_string_t tmf = { .ctx = ctx };
    AUTO_RMSTR redis_module_string_t end = { .ctx = ctx };
    if (RedisModule_HashGet(key, REDISMODULE_HASH_CFIELDS,
        redis_field_labels[kABI], &abi.str,
        redis_field_labels[kTimeFormat], &tmf.str,
        redis_field_labels[kEnd], &end.str,
        NULL) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "expected field(s) missing");
        return REDISMODULE_ERR;
    }

    long long _tmf;
    if (RedisModule_StringToLongLong(tmf.str, &_tmf) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "invalid _tmf");
        return REDISMODULE_ERR;
    }

    long long end_time;
    if (_tmf == 1) {
        end_time = mk_time(RedisModule_StringPtrLen(end.str, NULL));
        if (end_time == (-1)) {
            RedisModule_ReplyWithError(ctx, "invalid iso8601 end date/time");
            return REDISMODULE_ERR;
        }
    } else {
        if (RedisModule_StringToLongLong(end.str, &end_time)
            == REDISMODULE_ERR) {
            RedisModule_ReplyWithError(ctx, "invalid end date/time");
            return REDISMODULE_ERR;
        }
    }

    // Create or update the index
    long long end_days = end_time / SECONDS_PER_DAY;
    AUTO_RMSTR redis_module_string_t idx = {
        .ctx = ctx,
        .str = RedisModule_CreateStringPrintf(ctx, "%s:idx:end:%lld",
            prefix, end_days)
    };
    AUTO_RMREPLY RedisModuleCallReply *reply = RedisModule_Call(ctx, "SADD",
        "sc", idx.str, jobid);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
        RedisModule_ReplyWithCallReply(ctx, reply);
        return REDISMODULE_ERR;
    }
    if (TTL > 0) {
        AUTO_RMREPLY RedisModuleCallReply *reply = RedisModule_Call(ctx,
            "EXPIRE", "sl", idx.str, TTL);
        if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
            RedisModule_ReplyWithCallReply(ctx, reply);
            return REDISMODULE_ERR;
        }
    }

    RedisModule_ReplyWithString(ctx, idx.str);
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

    AUTO_RMSTR redis_module_string_t matchset = {
        .ctx = ctx,
        .str = RedisModule_CreateStringPrintf(ctx, "%s:mat:%s", prefix, uuid)
    };

    // The range of index keys we will inspect
    long long start_day, end_day, day;
    job_query_start_day(qry, &start_day);
    job_query_end_day(qry, &end_day);

    // Inspect each index for jobs that match
    for (day = start_day; day <= end_day; ++day) {

        AUTO_RMSTR redis_module_string_t idx = {
            .ctx = ctx,
            .str = RedisModule_CreateStringPrintf(ctx, "%s:idx:end:%lld",
                prefix, day)
        };
        sscan_cursor_init_t init = {
            .ctx = ctx,
            .set = idx.str,
            .count = 500
        };
        AUTO_PTR(destroy_sscan_cursor) sscan_cursor_t cursor =
            create_sscan_cursor(&init);
        if (sscan_error(cursor, &err, NULL) == SSCAN_ERR) {
            RedisModule_ReplyWithError(ctx, err);
            return REDISMODULE_ERR;
        }
        do {
            AUTO_RMSTR redis_module_string_t job = { .ctx = ctx };
            rc = sscan_next_element(cursor, &job.str);
            if (rc == SSCAN_ERR) {
                sscan_error(cursor, &err, NULL);
                RedisModule_ReplyWithError(ctx, err);
                return REDISMODULE_ERR;
            }
            if ((rc == SSCAN_OK) && job.str) {
                long long jobid;
                if (RedisModule_StringToLongLong(job.str, &jobid)
                    == REDISMODULE_ERR) {
                    RedisModule_ReplyWithError(ctx, "invalid job id");
                    return REDISMODULE_ERR;
                }
                int job_match = job_query_match_job(qry, jobid);
                if (job_match == QUERY_ERR) {
                    job_query_error(qry, &err, NULL);
                    RedisModule_ReplyWithError(ctx, err);
                    return REDISMODULE_ERR;
                }
                if (job_match == QUERY_PASS) {
                    // Add job to sorted set with score = job so that we can
                    // pop them off in SLURMFC.FETCH in job sorted order
                    AUTO_RMREPLY RedisModuleCallReply *reply =
                        RedisModule_Call(ctx, "ZADD", "sll", matchset.str,
                        jobid, jobid);
                }
            }
        } while (rc != SSCAN_EOF);
    }

    AUTO_RMKEY RedisModuleKey *matchset_key = RedisModule_OpenKey(ctx,
        matchset.str, REDISMODULE_WRITE);
    if (RedisModule_KeyType(matchset_key) == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }
    if (RedisModule_KeyType(matchset_key) != REDISMODULE_KEYTYPE_ZSET) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetExpire(matchset_key, QUERY_TTL * 1000)
        == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "failed to set ttl on match set");
        return REDISMODULE_ERR;
    }

    RedisModule_ReplyWithString(ctx, matchset.str);
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

    long long max_count, count = 0;
    const char *prefix = RedisModule_StringPtrLen(argv[1], NULL);
    const char *uuid = RedisModule_StringPtrLen(argv[2], NULL);
    AUTO_RMSTR redis_module_string_t matchset;
        matchset.ctx = ctx;
        matchset.str = RedisModule_CreateStringPrintf(ctx, "%s:mat:%s",
            prefix, uuid);

    if (RedisModule_StringToLongLong(argv[3], &max_count) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, "invalid max count");
        return REDISMODULE_ERR;
    }
    if (max_count > FETCH_MAX_COUNT) {
        max_count = FETCH_MAX_COUNT;
    }
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    count = 0;
    while (count < max_count) {
        size_t s = 0;
        AUTO_RMFIELDS redis_module_fields_t fields = { .ctx = ctx };
        AUTO_RMREPLY RedisModuleCallReply *reply = RedisModule_Call(ctx,
            "ZPOPMIN", "sl", matchset.str, 100);
        if ((RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_NULL) ||
            (RedisModule_CallReplyType(reply) != REDISMODULE_REPLY_ARRAY) ||
            (RedisModule_CallReplyLength(reply)) == 0) {
            break;
        }
        for (s = 0; (s < RedisModule_CallReplyLength(reply)) &&
                (count < max_count); s += 2) {
            RedisModuleCallReply *subreply = // no AUTO_RMREPLY
                RedisModule_CallReplyArrayElement(reply, s);
            AUTO_RMSTR redis_module_string_t job = {
                .ctx = ctx,
                .str = RedisModule_CreateStringFromCallReply(subreply)
            };
            long long jobid;
            if (RedisModule_StringToLongLong(job.str, &jobid)
                == REDISMODULE_ERR) {
                continue;
            }
            AUTO_RMSTR redis_module_string_t job_keyname = {
                .ctx = ctx,
                .str = RedisModule_CreateStringPrintf(ctx, "%s:%lld",
                    prefix, jobid)
            };
            AUTO_RMKEY RedisModuleKey *job_key = RedisModule_OpenKey(ctx,
                job_keyname.str, REDISMODULE_READ);
            if (RedisModule_KeyType(job_key) == REDISMODULE_KEYTYPE_EMPTY) {
                continue;
            }
            if (RedisModule_HashGet(job_key, REDISMODULE_HASH_CFIELDS,
                redis_field_labels[0], &fields.str[0],
                redis_field_labels[1], &fields.str[1],
                redis_field_labels[2], &fields.str[2],
                redis_field_labels[3], &fields.str[3],
                redis_field_labels[4], &fields.str[4],
                redis_field_labels[5], &fields.str[5],
                redis_field_labels[6], &fields.str[6],
                redis_field_labels[7], &fields.str[7],
                redis_field_labels[8], &fields.str[8],
                redis_field_labels[9], &fields.str[9],
                redis_field_labels[10], &fields.str[10],
                redis_field_labels[11], &fields.str[11],
                redis_field_labels[12], &fields.str[12],
                redis_field_labels[13], &fields.str[13],
                redis_field_labels[14], &fields.str[14],
                redis_field_labels[15], &fields.str[15],
                redis_field_labels[16], &fields.str[16],
                redis_field_labels[17], &fields.str[17],
                redis_field_labels[18], &fields.str[18],
                redis_field_labels[19], &fields.str[19],
                redis_field_labels[20], &fields.str[20],
                redis_field_labels[21], &fields.str[21],
                redis_field_labels[22], &fields.str[22],
                redis_field_labels[23], &fields.str[23],
                redis_field_labels[24], &fields.str[24],
                redis_field_labels[25], &fields.str[25],
                redis_field_labels[26], &fields.str[26],
                redis_field_labels[27], &fields.str[27],
                NULL) == REDISMODULE_ERR) {
                    continue;
            }
            RedisModule_ReplyWithArray(ctx, MAX_REDIS_FIELDS);
            int i = 0;
            for (; i < MAX_REDIS_FIELDS; ++i) {
                if (fields.str[i]) {
                    RedisModule_ReplyWithString(ctx, fields.str[i]);
                } else {
                    RedisModule_ReplyWithNull(ctx);
                }
            }
            ++count;
        }
    }
    RedisModule_ReplySetArrayLength(ctx, count);
    return REDISMODULE_OK;
}
