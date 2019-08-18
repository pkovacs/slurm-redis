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

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "stringto.h"

int sr_strtol(const char *str, long int *ret)
{
    assert(str != NULL);
    long int i = strtol(str, NULL, 10);
    if ((errno == ERANGE &&
            ((i == LONG_MAX) || (i == LONG_MIN)))
        || (errno != 0 && i == 0)) {
        return -1;
    }
    if (ret) {
        *ret = i;
    }
    return 0;
}

int sr_strtoll(const char *str, long long int *ret)
{
    assert(str != NULL);
    long long int i = strtoll(str, NULL, 10);
    if ((errno == ERANGE &&
            ((i == LLONG_MAX) || (i == LLONG_MIN)))
        || (errno != 0 && i == 0)) {
        return -1;
    }
    if (ret) {
        *ret = i;
    }
    return 0;
}

int sr_strtoul(const char *str, long unsigned int *ret)
{
    assert(str != NULL);
    long unsigned int i = strtoul(str, NULL, 10);
    if ((errno == ERANGE && (i == ULONG_MAX))
        || (errno != 0 && i == 0)) {
        return -1;
    }
    if (ret) {
        *ret = i;
    }
    return 0;
}

int sr_strtoull(const char *str, long long unsigned int *ret)
{
    assert(str != NULL);
    long long unsigned int i = strtoull(str, NULL, 10);
    if ((errno == ERANGE && (i == ULLONG_MAX))
        || (errno != 0 && i == 0)) {
        return -1;
    }
    if (ret) {
        *ret = i;
    }
    return 0;
}
