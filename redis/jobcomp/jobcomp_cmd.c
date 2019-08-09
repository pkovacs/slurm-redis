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

#include "common/iso8601_format.h"
#include "redis/common/sscan_cursor.h"
#include "jobcomp_qry.h"

int jobcomp_cmd_index(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    RedisModule_AutoMemory(ctx);
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    const char *keytag = RedisModule_StringPtrLen(argv[1], NULL);
    const char *jobid = RedisModule_StringPtrLen(argv[2], NULL);
    RedisModuleString *keyname = RedisModule_CreateStringPrintf(ctx,
        "%s:%s", keytag, jobid);
    RedisModuleCallReply *reply = NULL;
    long long end_time;

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

#ifdef ISO8601_DATES
    end_time = mk_time(RedisModule_StringPtrLen(end, NULL));
    if (end_time == (-1)) {
        RedisModule_ReplyWithError(ctx, "invalid iso8601 end date/time");
        return REDISMODULE_ERR;
    }
#else
    end_time = strtoll(RedisModule_StringPtrLen(end, NULL), NULL, 10);
    if ((errno == ERANGE &&
            (end_time == LLONG_MAX || end_time == LLONG_MIN))
        || (errno != 0 && end_time == 0)) {
        RedisModule_ReplyWithError(ctx, "invalid end date/time");
        return REDISMODULE_ERR;
    }
#endif

    // Create or update the end index
    long long end_days = end_time / SECONDS_PER_DAY;
    RedisModuleString *idx_end = RedisModule_CreateStringPrintf(ctx,
        "%s:idx:%lld:end", keytag, end_days);
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

    job_query_init_t init = {
        .ctx = ctx,
        .keytag = keytag,
        .uuid = uuid,
    };
    AUTO_PTR(destroy_job_query) job_query_t qry = create_job_query(&init);
    if (!qry) {
        return REDISMODULE_ERR;
    }
    RedisModuleString *match = RedisModule_CreateStringPrintf(ctx,
        "%s:mat:%s", keytag, uuid);

    // The range of index keys we will inspect
    long long start_day = job_query_start_day(qry);
    long long end_day = job_query_end_day(qry);
    long long day = start_day;

    // Inspect each index for jobs that match
    for (; day <= end_day; ++day) {

        int rc;
        const char *element, *err;
        size_t element_sz, err_sz;
        RedisModuleString *idx = RedisModule_CreateStringPrintf(ctx,
            "%s:idx:%lld:end", keytag, day);

        sscan_cursor_init_t init = {
            .ctx = ctx,
            .set = RedisModule_StringPtrLen(idx, NULL),
            .count = 500
        };
        AUTO_PTR(destroy_sscan_cursor) sscan_cursor_t cursor =
            create_sscan_cursor(&init);
        if (sscan_error(cursor, &err, &err_sz) == SSCAN_ERR) {
            RedisModule_ReplyWithError(ctx, err);
            return REDISMODULE_ERR;
        }
        do {
            rc = sscan_next_element(cursor, &element, &element_sz);
            if (rc == SSCAN_ERR) {
                sscan_error(cursor, &err, &err_sz);
                RedisModule_ReplyWithError(ctx, err);
                return REDISMODULE_ERR;
            }
            if ((rc == SSCAN_OK) && element) {
                long long job = strtoll(element, NULL, 10);
                if ((errno == ERANGE &&
                        (job == LLONG_MAX || job == LLONG_MIN))
                    || (errno != 0 && job == 0)) {
                    RedisModule_ReplyWithError(ctx, "invalid job id");
                    return REDISMODULE_ERR;
                }

                if (job_query_match_job(qry, job)) {
                    RedisModule_Call(ctx, "SADD", "sl", match, job);
                }
            }
        } while (rc != SSCAN_EOF);
        RedisModule_FreeString(ctx, idx);
        idx = NULL;
    }

    RedisModuleKey *mkey = RedisModule_OpenKey(ctx, match, REDISMODULE_WRITE);
    if ((RedisModule_KeyType(mkey) != REDISMODULE_KEYTYPE_EMPTY) &&
        (RedisModule_KeyType(mkey) != REDISMODULE_KEYTYPE_SET)) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }
    if (RedisModule_KeyType(mkey) == REDISMODULE_KEYTYPE_SET &&
            (RedisModule_SetExpire(mkey, QUERY_KEY_TTL * 1000)
                == REDISMODULE_ERR)) {
        RedisModule_ReplyWithError(ctx, "failed to set ttl on match set");
        return REDISMODULE_ERR;
    }

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}
