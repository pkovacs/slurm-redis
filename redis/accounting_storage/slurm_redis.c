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

#include <redismodule.h>

#include "jobcomp_redis.h"

const char *module_name = "slurm_redis";
const int module_version = 1;

static RedisModuleType *jobcomp_type = 0;

int RedisModule_OnLoad(RedisModuleCtx *ctx)
{
    if (RedisModule_Init(ctx, module_name, module_version, REDISMODULE_APIVER_1)
        == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    RedisModuleTypeMethods jobcomp_type_methods = {
        .version = REDISMODULE_TYPE_METHOD_VERSION,
        .rdb_load = jobcomp_type_load,
        .rdb_save = jobcomp_type_save,
        .aof_rewrite = jobcomp_type_rewrite,
        .free = jobcomp_type_free
    };
    jobcomp_type = RedisModule_CreateDataType(ctx, JOBCOMP_TYPE_NAME,
                JOBCOMP_TYPE_ENCODING_VERSION, &jobcomp_type_methods);
    if (jobcomp_type == NULL) {
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}
