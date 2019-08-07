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

#include "ttl_hash.h"

#define _XOPEN_SOURCE 600
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include <src/common/xmalloc.h>
#include <src/common/xstring.h>

#define TTL_HASH_VALUE_SZ 32

typedef struct ttl_hash_bucket {
    size_t ttl;
    size_t key;
    char value[TTL_HASH_VALUE_SZ];
} *ttl_hash_bucket_t;

typedef struct ttl_hash {
    size_t hash_sz;
    size_t hash_ttl;
    ttl_hash_bucket_t buckets;
    pthread_rwlockattr_t rwlock_attr;
    pthread_rwlock_t rwlock;
} *ttl_hash_t;

static size_t hasher(size_t x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

static void unlock_rwlock(pthread_rwlock_t **rwlock)
{
    if (*rwlock) {
        pthread_rwlock_unlock(*rwlock);
    }
}

ttl_hash_t create_ttl_hash(const ttl_hash_init_t *init)
{
    assert(init != NULL);
    ttl_hash_t hash = xmalloc(sizeof(struct ttl_hash));
    hash->buckets = xmalloc(init->hash_sz * sizeof(struct ttl_hash_bucket));
    hash->hash_sz = init->hash_sz;
    hash->hash_ttl = init->hash_ttl;
    pthread_rwlockattr_init(&hash->rwlock_attr);
    pthread_rwlockattr_setkind_np(&hash->rwlock_attr,
        PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
    pthread_rwlock_init(&hash->rwlock, &hash->rwlock_attr);
    return hash;
}

void destroy_ttl_hash(ttl_hash_t *hash)
{
    if (!hash) {
        return;
    }
    pthread_rwlock_destroy(&(*hash)->rwlock);
    pthread_rwlockattr_destroy(&(*hash)->rwlock_attr);
    xfree((*hash)->buckets);
    xfree((*hash));
}

int ttl_hash_get(ttl_hash_t hash, size_t key, char **value)
{
    AUTO_PTR(unlock_rwlock) pthread_rwlock_t *lock = &hash->rwlock;
    if (pthread_rwlock_rdlock(&hash->rwlock)) {
        return HASH_BUSY;
    }
    size_t bkt = hasher(key) % hash->hash_sz;
    if (hash->buckets[bkt].key != key) {
        return HASH_NOT_FOUND;
    }
    if ((hash->buckets[bkt].ttl + hash->hash_ttl) < (size_t)time(NULL)) {
        return HASH_EXPIRED;
    }
    if (value) {
        *value = xstrdup(hash->buckets[bkt].value);
    }
    return HASH_OK;
}

int ttl_hash_set(ttl_hash_t hash, size_t key, const char *value)
{
    AUTO_PTR(unlock_rwlock) pthread_rwlock_t *lock = &hash->rwlock;
    if (pthread_rwlock_wrlock(&hash->rwlock)) {
        return HASH_BUSY;
    }
    size_t bkt = hasher(key) % hash->hash_sz;
    hash->buckets[bkt].key = key;
    memset(hash->buckets[bkt].value, 0, sizeof(hash->buckets[bkt].value));
    if (value) {
        // bucket string is always zero-terminated
        strncpy(hash->buckets[bkt].value, value,
                sizeof(hash->buckets[bkt].value)-1);
    }
    hash->buckets[bkt].ttl = hash->hash_ttl + (size_t)time(NULL);
    return HASH_OK;
}
