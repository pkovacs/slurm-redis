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

#include "jobcomp_command.h"

const char *module_name = "slurm_jobcomp";
const int module_version = 1;

/*
 * Entry point for the redis module
 */
int RedisModule_OnLoad(RedisModuleCtx *ctx)
{
    // Register the module
    if (RedisModule_Init(ctx, module_name, module_version, REDISMODULE_APIVER_1)
        == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    // Register the SLURMJC.INDEX command
    if (RedisModule_CreateCommand(ctx, JOBCOMP_COMMAND_INDEX, jobcomp_cmd_index,
            "write", 1, 1, 1)
        == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    // Register the SLURMJC.MATCH command
    if (RedisModule_CreateCommand(ctx, JOBCOMP_COMMAND_MATCH, jobcomp_cmd_match,
            "write", 1, 1, 1)
        == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    // Register the SLURMJC.FETCH command
    if (RedisModule_CreateCommand(ctx, JOBCOMP_COMMAND_FETCH, jobcomp_cmd_fetch,
            "write", 1, 1, 1)
        == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
