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

#ifndef JOBCOMP_REDIS_AUTO_H
#define JOBCOMP_REDIS_AUTO_H

#include <hiredis.h>

#include <src/common/list.h> /* ListIterator, ... */

#include "jobcomp_redis_format.h"

#define AUTO_STR AUTO_PTR(destroy_string)
#define AUTO_LITER AUTO_PTR(destroy_list_iterator)
#define AUTO_FIELDS AUTO_PTR(destroy_redis_fields)
#define AUTO_REPLY AUTO_PTR(destroy_redis_reply)

void destroy_string(char **str);
void destroy_list_iterator(ListIterator *it);
void destroy_redis_fields(redis_fields_t **fields);
void destroy_redis_reply(redisReply **reply);

#endif /* JOBCOMP_REDIS_AUTO_H */
