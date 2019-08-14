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

#ifndef JOBCOMP_AUTO_H
#define JOBCOMP_AUTO_H

#include <redismodule.h>

#include "common/redis_fields.h"

// Use redis_module_string_t on the stack to hold and auto delete
// the contained redis module string using its context
typedef struct redis_module_string {
    RedisModuleCtx *ctx;
    RedisModuleString *str;
} redis_module_string_t;

// Use redis_module_fields_t on the stack to hold and auto delete
// the contained redis module strings using their context
typedef struct redis_module_fields {
    RedisModuleCtx *ctx;
    RedisModuleString *str[MAX_REDIS_FIELDS];
} redis_module_fields_t;

#define AUTO_RMKEY AUTO_PTR(close_redis_key)
#define AUTO_RMREPLY AUTO_PTR(destroy_redis_reply)
#define AUTO_RMSTR AUTO_PTR(destroy_redis_module_string)
#define AUTO_RMFIELDS AUTO_PTR(destroy_redis_module_fields)

void close_redis_key(RedisModuleKey **key);
void destroy_redis_reply(RedisModuleCallReply **reply);
void destroy_redis_module_string(redis_module_string_t *str);
void destroy_redis_module_fields(redis_module_fields_t *fields);

#endif /* JOBCOMP_AUTO_H */
