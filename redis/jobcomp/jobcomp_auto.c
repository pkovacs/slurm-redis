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

#include "jobcomp_auto.h"

void close_redis_key(RedisModuleKey **key)
{
    if (key) {
        RedisModule_CloseKey(*key);
    }
}

void destroy_redis_reply(RedisModuleCallReply **reply)
{
    if (reply) {
        RedisModule_FreeCallReply(*reply);
        *reply = NULL;
    }
}

void destroy_redis_module_string(redis_module_string_t *str)
{
    if (str) {
        RedisModule_FreeString(str->ctx, str->str);
        str->str = NULL;
    }
}

void destroy_redis_module_fields(redis_module_fields_t *fields)
{
    if (fields) {
        int i = 0;
        for (; i < MAX_REDIS_FIELDS; ++i) {
            if (fields->str[i]) {
                RedisModule_FreeString(fields->ctx, fields->str[i]);
                fields->str[i] = NULL;
            }
        }
    }
}
