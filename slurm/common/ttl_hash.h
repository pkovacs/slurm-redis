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

#ifndef TTL_HASH_H
#define TTL_HASH_H

#include <stddef.h>

/*
 * A thread-safe hash for storing key/values with time-to-live (ttl)
 */

// Hash return codes
enum {
    HASH_BUSY = -1,
    HASH_OK = 0,
    HASH_NOT_FOUND = 2,
    HASH_EXPIRED = 3
};

// Hash is an opaque pointer
typedef struct ttl_hash *ttl_hash_t;

// Hash initialization
typedef struct {
    // Number of hash entries
    size_t hash_sz;
    // Time-to-live in seconds of hash entries
    size_t hash_ttl;
} ttl_hash_init_t;

// Create a ttl hash
ttl_hash_t create_ttl_hash(const ttl_hash_init_t *init);

// Destroy a ttl hash
void destroy_ttl_hash(ttl_hash_t *hash);

// Test for a key in the hash, optionally returning a dup of its value
int ttl_hash_get(ttl_hash_t hash, size_t key, char **value);

// Set a key/value in the hash with the value valid for ttl seconds
int ttl_hash_set(ttl_hash_t hash, size_t key, const char *value);

#endif /* TTL_HASH_H */
