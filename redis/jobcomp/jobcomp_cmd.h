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

#ifndef JOBCOMP_CMD_H
#define JOBCOMP_CMD_H

#include <redismodule.h>

#define JOBCOMP_CMD_INDEX "SLURMJC.INDEX"
#define JOBCOMP_CMD_MATCH "SLURMJC.MATCH"
#define JOBCOMP_CMD_FETCH "SLURMJC.FETCH"

int jobcomp_cmd_index(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
int jobcomp_cmd_match(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
int jobcomp_cmd_fetch(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

#endif /* JOBCOMP_CMD_H */
