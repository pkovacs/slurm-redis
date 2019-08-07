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

#include "iso8601_format.h"

#include <stddef.h>
#include <stdio.h>

char *mk_iso8601(time_t t, char *iso8601)
{
    if (!iso8601 || t < (time_t)0) {
        return NULL;
    }
    const size_t iso8601_sz = 21;
    struct tm tm_s;
    if (!gmtime_r(&t, &tm_s) ||
        !strftime(iso8601, iso8601_sz, "%FT%TZ", &tm_s)) {
        return NULL;
    }
    return iso8601;
}

time_t mk_time(const char *iso8601)
{
    int y, M, d, h, m, s;
    if (sscanf(iso8601, "%d-%d-%dT%d:%d:%dZ", &y, &M, &d, &h, &m, &s)
        != 6) {
        return (time_t)(-1);
    }
    struct tm tm_s = {
        .tm_year = y - 1900,
        .tm_mon = M - 1,
        .tm_mday = d,
        .tm_hour = h,
        .tm_min = m,
        .tm_sec = s,
        .tm_isdst = -1
    };
    return timegm(&tm_s);
}
