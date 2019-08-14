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

#ifndef SSCAN_CURSOR_H
#define SSCAN_CURSOR_H

#include <stddef.h>
#include <redismodule.h>

/*
 * A wrapper for creating and iterating over redis set scan cursors
 * using the redis module api
 */

enum {
    SSCAN_ERR = -2,
    SSCAN_EOF = -1,
    SSCAN_OK = 0,
};

// Set scan cursor is an opaque pointer
typedef struct sscan_cursor *sscan_cursor_t;

// Set scan cursor initialization
typedef struct {
    RedisModuleCtx *ctx;
    RedisModuleString *set;
    long long count;
} sscan_cursor_init_t;

// Create a set scan cursor
sscan_cursor_t create_sscan_cursor(const sscan_cursor_init_t *init);

// Destroy a set scan cursor
void destroy_sscan_cursor(sscan_cursor_t *cursor);

// Return last error and error size byref, integer status (enum)
int sscan_error(sscan_cursor_t cursor, const char **err, size_t *len);

// Return copy of next element byref, integer status (enum)
int sscan_next_element(sscan_cursor_t cursor, RedisModuleString **str);

#endif /* SSCAN_CURSOR_H */
