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

#include <src/common/xmalloc.h> /* xmalloc, ... */
#include <src/common/xstring.h> /* xstrdup, ... */

#define REPLY_ERRORMSG_WRONGTYPE "WRONGTYPE reply is of the wrong type"

typedef struct sscan_cursor {
    redisContext *ctx;
    redisReply *reply;
    redisReply *subreply_array;
    char *set;
    char *err;
    long long count;
    long long value;
    size_t array_ix;
} *sscan_cursor_t;

static void call_sscan_internal(sscan_cursor_t cursor)
{
    if (cursor->reply) {
        // This recursively frees the subreplies too
        freeReplyObject(cursor->reply);
        cursor->reply = NULL;
        cursor->subreply_array = NULL;
    }
    cursor->array_ix = 0;

    // Call SSCAN
    cursor->reply = redisCommand(cursor->ctx, "SSCAN %s %lld COUNT %lld",
        cursor->set, cursor->value, cursor->count);
    if ((cursor->reply->type != REDIS_REPLY_ARRAY) ||
        (cursor->reply->elements != 2)) {
        cursor->err = xstrdup(REPLY_ERRORMSG_WRONGTYPE);
        return;
    }

    // Fetch the cursor value on the first element; zero means last iteration
    redisReply *subreply_cursor = cursor->reply->element[0];
    if (subreply_cursor->type != REDIS_REPLY_STRING) {
        cursor->err = xstrdup(REPLY_ERRORMSG_WRONGTYPE);
        return;
    }

    // Fetch the array of values on the second element; can be NULL
    cursor->subreply_array = cursor->reply->element[1];
    if (cursor->subreply_array &&
            (cursor->subreply_array->type != REDIS_REPLY_ARRAY)) {
        cursor->err = xstrdup(REPLY_ERRORMSG_WRONGTYPE);
        return;
    }

    // Convert the string cursor value to an integer
    cursor->value = strtoll(subreply_cursor->str, NULL, 10);
    if ((errno == ERANGE &&
            (cursor->value == LLONG_MAX || cursor->value == LLONG_MIN))
        || (errno != 0 && cursor->value == 0)) {
        cursor->err = xstrdup("invalid cursor");
        return;
    }
}

sscan_cursor_t create_sscan_cursor(const sscan_cursor_init_t *init)
{
    assert(init != NULL);
    assert(init->ctx != NULL);
    assert(init->set != NULL);
    assert(init->count > 0);

    sscan_cursor_t cursor = xmalloc(sizeof(struct sscan_cursor));
    cursor->ctx = init->ctx;
    cursor->set = init->set;
    cursor->count = init->count;
    cursor->value = -1;
    return cursor;
}

void destroy_sscan_cursor(sscan_cursor_t *cursor)
{
    if (!cursor) {
        return;
    }
    if ((*cursor)->reply) {
        // This recursively frees the subreplies too
        freeReplyObject((*cursor)->reply);
        (*cursor)->reply = NULL;
        (*cursor)->subreply_array = NULL;
    }
    xfree((*cursor)->set);
    xfree((*cursor)->err);
    xfree(*cursor);
}

int sscan_error(sscan_cursor_t cursor, const char **err, size_t *len)
{
    assert(cursor != NULL);
    assert(cursor->ctx != NULL);
    if (cursor->err && err) {
        *err = cursor->err;
        if (len) {
            *len = strlen(cursor->err);
        }
        return SSCAN_ERR;
    }
    return SSCAN_OK;
}

int sscan_next_element(sscan_cursor_t cursor, const char **ret, size_t *len)
{
    assert(cursor != NULL);
    assert(cursor->ctx != NULL);

    xfree(cursor->err);

    // We are hiding the repeated calls to SSCAN.  In order to complete
    // a full iteration with SSCAN, it is required that we keep calling
    // SSCAN until the cursor value on reply[0] is zero. We consume the
    // array on reply[1] one at a time with each sscan_next_element.
    while (cursor->value != 0 &&
            ((cursor->subreply_array == NULL) ||
             (cursor->array_ix >= cursor->subreply_array->elements))) {
        call_sscan_internal(cursor);
        if (cursor->err) {
            return SSCAN_ERR;
        }
    }

    // Are we done?
    if (cursor->value == 0 &&
        ((cursor->subreply_array == NULL) ||
         (cursor->array_ix >= cursor->subreply_array->elements))) {
        return SSCAN_EOF;
    }

    // If we have an array on the sub-reply, consume it one at a time
    if (cursor->subreply_array &&
        (cursor->array_ix < cursor->subreply_array->elements)) {
        if (cursor->subreply_array->element[cursor->array_ix] && ret) {
            *ret = cursor->subreply_array->element[cursor->array_ix]->str;
            if (len) {
                *len = cursor->subreply_array->element[cursor->array_ix]->len;
            }
        }
        ++cursor->array_ix;
    }
    return SSCAN_OK;
}
