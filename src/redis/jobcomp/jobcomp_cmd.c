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

#include <stdio.h>
#include <string.h>
#include <time.h>

static int days_since_unix_epoch(const char *iso8601_date, long long *days)
{
    struct tm tm_date;
    time_t start_time;
    int y, M, d;
    if (sscanf(iso8601_date, "%d-%d-%d", &y, &M, &d) != 3) {
        return -1;
    }
    memset(&tm_date, 0, sizeof(tm_date));
    tm_date.tm_year = y - 1900;
    tm_date.tm_mon = M - 1;
    tm_date.tm_mday = d;
    start_time = mktime(&tm_date);
    if (days) {
        *days = start_time / 86400;
    }
    return 0;
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

    // Open the jobid key that was created in the slurm jobcomp plugin
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyname, REDISMODULE_READ);
    if ((RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) ||
        (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_HASH)) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        goto error;
    }

    // Create or update the uid index
    RedisModuleString *uid = NULL;
    if (RedisModule_HashGet(key, REDISMODULE_HASH_CFIELDS, "UID", &uid, NULL)
        == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "UID key missing");
        goto error;
    }
    const char *uid_c = RedisModule_StringPtrLen(uid, NULL);
    RedisModuleString *idxname_uid =
        RedisModule_CreateStringPrintf(ctx, "%s:idx:uid:%s", keytag, uid_c);
    reply = RedisModule_Call(ctx, "SADD", "sc", idxname_uid, jobid);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
        RedisModule_ReplyWithCallReply(ctx, reply);
        goto error;
    }

    // Create or update the partition index
    RedisModuleString *partition = NULL;
    if (RedisModule_HashGet(key, REDISMODULE_HASH_CFIELDS, "Partition",
        &partition, NULL) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "Partition key missing");
        goto error;
    }
    const char *partition_c = RedisModule_StringPtrLen(partition, NULL);
    RedisModuleString *idxname_partition =
        RedisModule_CreateStringPrintf(ctx, "%s:idx:partition:%s", keytag,
            partition_c);
    reply = RedisModule_Call(ctx, "SADD", "sc", idxname_partition, jobid);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
        RedisModule_ReplyWithCallReply(ctx, reply);
        goto error;
    }

    // Create or update the jobname index
    RedisModuleString *jobname = NULL;
    if (RedisModule_HashGet(key, REDISMODULE_HASH_CFIELDS, "JobName",
        &jobname, NULL) == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "JobName key missing");
        goto error;
    }
    const char *jobname_c = RedisModule_StringPtrLen(jobname, NULL);
    RedisModuleString *idxname_jobname =
        RedisModule_CreateStringPrintf(ctx, "%s:idx:jobname:%s", keytag,
            jobname_c);
    reply = RedisModule_Call(ctx, "SADD", "sc", idxname_jobname, jobid);
    if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
        RedisModule_ReplyWithCallReply(ctx, reply);
        goto error;
    }

    // Create or update the start index (daily bins)
    RedisModuleString *start = NULL;
    if (RedisModule_HashGet(key, REDISMODULE_HASH_CFIELDS, "Start", &start, NULL)
        == REDISMODULE_ERR) {
        RedisModule_ReplyWithError(ctx, "Start key missing");
        goto error;
    }
    long long days = 0;
    if (days_since_unix_epoch(RedisModule_StringPtrLen(start, NULL), &days) < 0) {
        RedisModule_ReplyWithError(ctx, "Failed to parse start date");
        goto error;
    }
    RedisModuleString *idxname_start =
        RedisModule_CreateStringPrintf(ctx, "%s:idx:start:%ld", keytag, days);
    reply = RedisModule_Call(ctx, "SADD", "sc", idxname_start, jobid);
    //if (RedisModule_CallReplyType(reply) == REDISMODULE_REPLY_ERROR) {
        RedisModule_ReplyWithCallReply(ctx, reply);
    //    goto error;
    //}

    RedisModule_CloseKey(key);
    return REDISMODULE_OK;

error:
    RedisModule_CloseKey(key);
    return REDISMODULE_ERR;
}

int jobcomp_cmd_jobs(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (ctx || argv || argc) {}
    return REDISMODULE_OK;
}
