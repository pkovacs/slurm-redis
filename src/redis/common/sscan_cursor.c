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

#include "sscan_cursor.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct sscan_cursor {
    RedisModuleCtx *ctx;
    RedisModuleString *set;
    RedisModuleString *err;
    RedisModuleCallReply *reply;
    RedisModuleCallReply *subreply_array;
    long long count;
    long long cursor;
    size_t next_element;
    size_t total_elements;
} *sscan_cursor_t;

static void call_sscan_internal(sscan_cursor_t cursor)
{
    if (cursor->reply) {
        // This recursively frees the subreplies too
        RedisModule_FreeCallReply(cursor->reply);
        cursor->reply = NULL;
        cursor->subreply_array = NULL;
    }
    cursor->next_element = 0;
    cursor->total_elements = 0;

    // Call SSCAN
    cursor->reply = RedisModule_Call(cursor->ctx, "SSCAN", "slcl", cursor->set,
        cursor->cursor, "COUNT", cursor->count);

    if ((RedisModule_CallReplyType(cursor->reply) != REDISMODULE_REPLY_ARRAY) ||
        (RedisModule_CallReplyLength(cursor->reply) != 2)) {
        cursor->err = RedisModule_CreateString(cursor->ctx,
            REDISMODULE_ERRORMSG_WRONGTYPE,
            strlen(REDISMODULE_ERRORMSG_WRONGTYPE));
        return;
    }

    // Fetch the cursor value on the first element; zero means last iteration
    RedisModuleCallReply *subreply_cursor = RedisModule_CallReplyArrayElement(
        cursor->reply, 0);
    if ((RedisModule_CallReplyType(subreply_cursor)
        != REDISMODULE_REPLY_STRING)) {
        cursor->err = RedisModule_CreateStringPrintf(cursor->ctx,
            REDISMODULE_ERRORMSG_WRONGTYPE);
        return;
    }

    // Fetch the array of values on the second element; can be NULL
    cursor->subreply_array = RedisModule_CallReplyArrayElement(cursor->reply,
        1);
    if (cursor->subreply_array &&
            (RedisModule_CallReplyType(cursor->subreply_array)
        != REDISMODULE_REPLY_ARRAY)) {
        cursor->err = RedisModule_CreateStringPrintf(cursor->ctx,
            REDISMODULE_ERRORMSG_WRONGTYPE);
        return;
    } else if (cursor->subreply_array) {
        cursor->total_elements =
            RedisModule_CallReplyLength(cursor->subreply_array);
    }

    // Convert the string cursor value to an integer
    cursor->cursor = strtoll(RedisModule_CallReplyStringPtr(subreply_cursor,
        NULL), NULL, 10);
    if ((errno == ERANGE &&
            (cursor->cursor == LLONG_MAX || cursor->cursor == LLONG_MIN))
        || (errno != 0 && cursor->cursor == 0)) {
        cursor->err = RedisModule_CreateStringPrintf(cursor->ctx,
            "invalid cursor"); 
        return;
    }
}

sscan_cursor_t create_sscan_cursor(const sscan_cursor_init_t *init)
{
    assert(init != NULL);
    assert(init->ctx != NULL);
    assert(init->set != NULL);
    assert(init->count > 0);

    sscan_cursor_t cursor = RedisModule_Calloc(1, sizeof(struct sscan_cursor));
    cursor->ctx = init->ctx;
    cursor->set = init->set;
    cursor->count = init->count;
    cursor->cursor = -1;
    return cursor;
}

const char *sscan_error(sscan_cursor_t cursor, size_t *len)
{
    assert(cursor != NULL);
    assert(cursor->ctx != NULL);

    if (cursor->err) {
        return RedisModule_StringPtrLen(cursor->err, len);
    }
    return NULL;
}

const char *sscan_next_element(sscan_cursor_t cursor, size_t *len)
{
    assert(cursor != NULL);
    assert(cursor->ctx != NULL);

    const char *element = NULL;
    size_t element_sz = 0;

    if (cursor->err) {
        RedisModule_FreeString(cursor->ctx, cursor->err);
        cursor->err = NULL;
    }
    // We are hiding the necessary repeated calls to SSCAN with this api.
    // In order to complete a full iteration with SSCAN, it is required that
    // we keep calling it while the cursor is non-zero.  We consume the array
    // with each call until the SSCAN cursor value hits zero.  The returned
    // array can legally come back NULL in which case we keep calling SSCAN
    // until values appear on the array OR the cursor comes back zero.
    while (cursor->cursor != 0 && cursor->subreply_array == NULL) {
        call_sscan_internal(cursor);
        if (cursor->err) {
            return RedisModule_StringPtrLen(cursor->err, len);
        }
    }
    if (cursor->subreply_array &&
        (cursor->next_element < cursor->total_elements)) {
        RedisModuleCallReply *subreply_element =
            RedisModule_CallReplyArrayElement(cursor->subreply_array,
                cursor->next_element);
        ++cursor->next_element;
        if (subreply_element) {
            element = RedisModule_CallReplyStringPtr(subreply_element,
                &element_sz);
        }
    }
    if (len) {
        *len = element_sz;
    }
    return element;
}

void destroy_sscan_cursor(sscan_cursor_t cursor)
{
    assert(cursor != NULL);
    assert(cursor->ctx != NULL);

    if (cursor->err) {
        RedisModule_FreeString(cursor->ctx, cursor->err);
        cursor->err = NULL;
    }
    if (cursor->set) {
        RedisModule_FreeString(cursor->ctx, cursor->set);
        cursor->set = NULL;
    }
    if (cursor->reply) {
        // This recursively frees the subreplies too
        RedisModule_FreeCallReply(cursor->reply);
        cursor->reply = NULL;
        cursor->subreply_array = NULL;
    }
    RedisModule_Free(cursor);
}
