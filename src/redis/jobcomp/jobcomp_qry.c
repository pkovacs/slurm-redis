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

#include "jobcomp_qry.h"

#include <assert.h>
#include <time.h>

typedef struct job_query {
    time_t start_time, end_time;
} *job_query_t;

// Create a job query
job_query_t create_job_query(const job_query_init_t *init)
{
    job_query_t qry = RedisModule_Calloc(1, sizeof(struct job_query));
    return qry;
}

// Destroy a job query
void destroy_job_query(job_query_t *qry)
{
    if (!qry) {
        return;
    }
    RedisModule_Free(*qry);
    *qry = NULL;
}

// Check if a job matches job query criteria
int job_query_match_job(const job_query_t qry, long long job)
{
    assert(qry != NULL);
    assert (job > 0);
    return 0;
}
