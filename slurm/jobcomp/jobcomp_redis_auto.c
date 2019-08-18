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

#include <src/common/xmalloc.h> /* xfree, ... */

#include "jobcomp_redis_auto.h"

void destroy_string(char **str)
{
    if (str && *str) {
        xfree(*str);
        *str = NULL;
    }
}

void destroy_list_iterator(ListIterator *it)
{
    if (it && *it) {
        slurm_list_iterator_destroy(*it);
        *it = NULL;
    }
}

void destroy_redis_fields(redis_fields_t *fields)
{
    if (fields) {
        size_t i = 0;
        for (; i < MAX_REDIS_FIELDS; ++i) {
            if (fields->value[i]) {
                xfree(fields->value[i]);
            }
        }
    }
}

void destroy_redis_reply(redisReply **reply)
{
    if (reply && *reply) {
        freeReplyObject(*reply);
        *reply = NULL;
    }
}
