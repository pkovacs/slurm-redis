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

#include "common/stringto.h"

/*
 * The sscan_cursor object
 */
typedef struct sscan_cursor {
    RedisModuleCtx *ctx;
    RedisModuleCallReply *reply;
    RedisModuleCallReply *subreply_array;
    const RedisModuleString *set;
    RedisModuleString *err;
    long long count;
    long long value;
    size_t array_ix;
    size_t array_sz;
} *sscan_cursor_t;

static void call_sscan_internal(sscan_cursor_t cursor);

/*
 * Create an sscan_cursor object
 */
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
    cursor->value = -1;
    return cursor;
}

/*
 * Destroy an sscan_cursor object
 */
void destroy_sscan_cursor(sscan_cursor_t *cursor)
{
    if (!cursor) {
        return;
    }
    if ((*cursor)->reply) {
        // This recursively frees the subreplies too
        RedisModule_FreeCallReply((*cursor)->reply);
        (*cursor)->reply = NULL;
        (*cursor)->subreply_array = NULL;
    }
    if ((*cursor)->err) {
        RedisModule_FreeString((*cursor)->ctx, (*cursor)->err);
        (*cursor)->err = NULL;
    }
    RedisModule_Free(*cursor);
    *cursor = NULL;
}

/*
 * Return the last sscan_cursor error and error length byref
 */
int sscan_error(sscan_cursor_t cursor, const char **err, size_t *len)
{
    assert(cursor != NULL);
    assert(cursor->ctx != NULL);

    if (cursor->err && err) {
        *err = RedisModule_StringPtrLen(cursor->err, len);
        return SSCAN_ERR;
    }
    return SSCAN_OK;
}

/*
 * Fetch the next element from the sscan cursor.  We are hiding the necessary
 * repeated calls to SSCAN.  In order to complete a full iteration with SSCAN,
 * it is required that we keep calling SSCAN until the cursor value on reply[0]
 * is zero.  We read the array on reply[1] one at a time with each call to this
 * api, then we issue another SSCAN and repeat until all set members are read
 */
int sscan_next_element(sscan_cursor_t cursor, RedisModuleString **str)
{
    assert(cursor != NULL);
    assert(cursor->ctx != NULL);

    if (cursor->err) {
        RedisModule_FreeString(cursor->ctx, cursor->err);
        cursor->err = NULL;
    }

    // Initial SSCAN requires a cursor value of zero
    if (cursor->value == -1) {
        cursor->value = 0;
        call_sscan_internal(cursor);
    }

    // Do we need to issue another SSCAN?
    while (cursor->value != 0 &&
            ((cursor->subreply_array == NULL) ||
             (cursor->array_ix >= cursor->array_sz))) {
        call_sscan_internal(cursor);
        if (cursor->err) {
            return SSCAN_ERR;
        }
    }

    // Are we done?
    if (cursor->value == 0 &&
        ((cursor->subreply_array == NULL) ||
         (cursor->array_ix >= cursor->array_sz))) {
        return SSCAN_EOF;
    }

    // If we have an array on the sub-reply, consume it one at a time
    if (cursor->subreply_array &&
        (cursor->array_ix < cursor->array_sz)) {
        RedisModuleCallReply *subreply_element =
            RedisModule_CallReplyArrayElement(cursor->subreply_array,
                cursor->array_ix);
        if (subreply_element && str) {
            *str = RedisModule_CreateStringFromCallReply(subreply_element);
        }
        ++cursor->array_ix;
    }
    return SSCAN_OK;
}

/*
 * Helper function to issue the SSCAN command and load the returned cursor
 * value and array for reading by sscan_next_element
 */
static void call_sscan_internal(sscan_cursor_t cursor)
{
    assert(cursor != NULL);

    if (cursor->reply) {
        // This recursively frees the subreplies too
        RedisModule_FreeCallReply(cursor->reply);
        cursor->reply = NULL;
        cursor->subreply_array = NULL;
    }
    cursor->array_ix = 0;
    cursor->array_sz = 0;

    // Call SSCAN
    cursor->reply = RedisModule_Call(cursor->ctx, "SSCAN", "slcl", cursor->set,
        cursor->value, "COUNT", cursor->count);

    if ((RedisModule_CallReplyType(cursor->reply) != REDISMODULE_REPLY_ARRAY) ||
        (RedisModule_CallReplyLength(cursor->reply) != 2)) {
        cursor->err = RedisModule_CreateStringPrintf(cursor->ctx,
            REDISMODULE_ERRORMSG_WRONGTYPE);
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
    if (cursor->subreply_array) {
        if (RedisModule_CallReplyType(cursor->subreply_array)
            != REDISMODULE_REPLY_ARRAY) {
            cursor->err = RedisModule_CreateStringPrintf(cursor->ctx,
                REDISMODULE_ERRORMSG_WRONGTYPE);
            return;
        }
        cursor->array_sz = RedisModule_CallReplyLength(cursor->subreply_array);
    }

    // Convert the string cursor value to an integer
    if (sr_strtoll(RedisModule_CallReplyStringPtr(subreply_cursor, NULL),
        &cursor->value) < 0) {
        cursor->err = RedisModule_CreateStringPrintf(cursor->ctx,
            "invalid cursor");
        return;
    }
}
