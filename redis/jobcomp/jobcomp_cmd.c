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
#include "jobcomp_type.h"

static jobcomp_t *jobcomp_create(
    uint32_t jobid,
    uint32_t uid,
    uint32_t gid,
    uint32_t state,
    uint32_t node_cnt,
    uint32_t proc_cnt,
    uint32_t maxprocs,
    uint32_t time_limit,
    time_t start_time,
    time_t end_time,
    time_t submit_time,
    time_t eligible_time,
    const char *account,
    const char *cluster,
    const char *user_name,
    const char *group_name,
    const char *job_name,
    const char *partition,
    const char *node_list,
    const char *qos_name,
    const char *resv_name,
    const char *req_gres,
    const char *wckey,
    const char *derived_ec,
    const char *exit_code,
    const char *work_dir)
{
    jobcomp_t *jc = RedisModule_Calloc(1, sizeof(jobcomp_t));
    jc->jobid = jobid;
    jc->uid = uid;
    jc->gid = gid;
    jc->state = state;
    jc->node_cnt = node_cnt;
    jc->proc_cnt = proc_cnt;
    jc->maxprocs = maxprocs;
    jc->time_limit = time_limit;
    jc->start_time = start_time;
    jc->end_time = end_time;
    jc->submit_time = submit_time;
    jc->eligible_time = eligible_time;
    jc->account = RedisModule_Strdup(account);
    jc->cluster = RedisModule_Strdup(cluster);
    jc->user_name = RedisModule_Strdup(user_name);
    jc->group_name = RedisModule_Strdup(group_name);
    jc->job_name = RedisModule_Strdup(job_name);
    jc->partition = RedisModule_Strdup(partition);
    jc->node_list = RedisModule_Strdup(node_list);
    jc->qos_name = RedisModule_Strdup(qos_name);
    jc->resv_name = RedisModule_Strdup(resv_name);
    jc->req_gres = RedisModule_Strdup(req_gres);
    jc->wckey = RedisModule_Strdup(wckey);
    jc->derived_ec = RedisModule_Strdup(derived_ec);
    jc->exit_code = RedisModule_Strdup(exit_code);
    jc->work_dir = RedisModule_Strdup(work_dir);
    return jc;
}

static void jobcomp_destroy(jobcomp_t *jc)
{
    if (!jc) {
        return;
    }

    RedisModule_Free(jc->account);
    RedisModule_Free(jc->cluster);
    RedisModule_Free(jc->user_name);
    RedisModule_Free(jc->group_name);
    RedisModule_Free(jc->job_name);
    RedisModule_Free(jc->partition);
    RedisModule_Free(jc->node_list);
    RedisModule_Free(jc->qos_name);
    RedisModule_Free(jc->resv_name);
    RedisModule_Free(jc->req_gres);
    RedisModule_Free(jc->wckey);
    RedisModule_Free(jc->derived_ec);
    RedisModule_Free(jc->exit_code);
    RedisModule_Free(jc->work_dir);
    RedisModule_Free(jc);
}

static int jobcomp_get_key(RedisModuleCtx *ctx, RedisModuleString *key_name,
                          jobcomp_t **jc, int mode)
{
    RedisModuleKey *key = RedisModule_OpenKey(ctx, key_name, mode);
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_CloseKey(key);
        RedisModule_ReplyWithError(ctx, "ERR key does not exist");
        return REDISMODULE_ERR;
    } else if (RedisModule_ModuleTypeGetType(key) != jobcomp_redis_t) {
        RedisModule_CloseKey(key);
        RedisModule_ReplyWithError(ctx, "ERR wrong type");
        return REDISMODULE_ERR;
    }

    *jc = RedisModule_ModuleTypeGetValue(key);
    RedisModule_CloseKey(key);
    return REDISMODULE_OK;
}

int jobcomp_cmd_create(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    // Arity: SLURM.CREATE <key> <args...>
    if (argc != JOBCOMP_TYPE_ARITY + 2) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithError(ctx, "ERR key already exists");
        goto final;
    }

    long long jobid, uid, gid, state, node_cnt, proc_cnt, maxprocs;
    long long time_limit, start_time, end_time, submit_time, eligible_time;

    if ((RedisModule_StringToLongLong(argv[2], &jobid) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid jobid");
        return REDISMODULE_ERR;
    }
    if ((RedisModule_StringToLongLong(argv[3], &uid) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid uid");
        return REDISMODULE_ERR;
    }
    if ((RedisModule_StringToLongLong(argv[4], &gid) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid gid");
        return REDISMODULE_ERR;
    }
    if ((RedisModule_StringToLongLong(argv[5], &state) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid state");
        return REDISMODULE_ERR;
    }
    if ((RedisModule_StringToLongLong(argv[6], &node_cnt) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid node_cnt");
        return REDISMODULE_ERR;
    }
    if ((RedisModule_StringToLongLong(argv[7], &proc_cnt) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid proc_cnt");
        return REDISMODULE_ERR;
    }
    if ((RedisModule_StringToLongLong(argv[8], &maxprocs) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid maxprocs");
        return REDISMODULE_ERR;
    }
    if ((RedisModule_StringToLongLong(argv[9], &time_limit) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid time_limit");
        return REDISMODULE_ERR;
    }
    if ((RedisModule_StringToLongLong(argv[10], &start_time) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid start_time");
        return REDISMODULE_ERR;
    }
    if ((RedisModule_StringToLongLong(argv[11], &end_time) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid end_time");
        return REDISMODULE_ERR;
    }
    if ((RedisModule_StringToLongLong(argv[12], &submit_time) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid submit_time");
        return REDISMODULE_ERR;
    }
    if ((RedisModule_StringToLongLong(argv[13], &eligible_time) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid eligible_time");
        return REDISMODULE_ERR;
    }

    jobcomp_t *jc = jobcomp_create(
        jobid, uid, gid, state, node_cnt, proc_cnt, maxprocs, time_limit, start_time,
        end_time, submit_time, eligible_time,
        RedisModule_StringPtrLen(argv[14], NULL),
        RedisModule_StringPtrLen(argv[15], NULL),
        RedisModule_StringPtrLen(argv[16], NULL),
        RedisModule_StringPtrLen(argv[17], NULL),
        RedisModule_StringPtrLen(argv[18], NULL),
        RedisModule_StringPtrLen(argv[19], NULL),
        RedisModule_StringPtrLen(argv[20], NULL),
        RedisModule_StringPtrLen(argv[21], NULL),
        RedisModule_StringPtrLen(argv[22], NULL),
        RedisModule_StringPtrLen(argv[23], NULL),
        RedisModule_StringPtrLen(argv[24], NULL),
        RedisModule_StringPtrLen(argv[25], NULL),
        RedisModule_StringPtrLen(argv[26], NULL),
        RedisModule_StringPtrLen(argv[27], NULL));

    if (RedisModule_ModuleTypeSetValue(key, jobcomp_redis_t, jc) == REDISMODULE_ERR) {
        goto final;
    }

    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");

final:
    RedisModule_CloseKey(key);
    return REDISMODULE_OK;
}

int jobcomp_cmd_info(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    jobcomp_t *jc = NULL;
    if (jobcomp_get_key(ctx, argv[1], &jc, REDISMODULE_READ) != REDISMODULE_OK) {
        return REDISMODULE_OK;
    }

    // Reply with an array of labels and matching data
    //RedisModule_ReplyWithArray(ctx, JOBCOMP_TYPE_ARITY * 2);
    RedisModule_ReplyWithArray(ctx, 2);
    RedisModule_ReplyWithSimpleString(ctx, "jobid");
    RedisModule_ReplyWithLongLong(ctx, jc->jobid);

    return REDISMODULE_OK;
}
