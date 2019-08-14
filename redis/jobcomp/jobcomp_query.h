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

#ifndef JOBCOMP_QUERY_H
#define JOBCOMP_QUERY_H

#include <stddef.h>
#include <redismodule.h>

/*
 * An abstract data type corresponding to slurm's slurmdb_job_cond_t
 */

enum {
    QUERY_ERR = -2,
    QUERY_NULL = -1,
    QUERY_OK = 0,
    QUERY_PASS = 1,
    QUERY_FAIL = 2
};

// A job query is an opaque pointer
typedef struct job_query *job_query_t;

// Job query initialization
typedef struct {
    RedisModuleCtx *ctx;
    const char *prefix;
    const char *uuid;
} job_query_init_t;

// Create a job query
job_query_t create_job_query(const job_query_init_t *init);

// Destroy a job query
void destroy_job_query(job_query_t *qry);

// Prepare the job query
int job_query_prepare(job_query_t qry);

// Return last error and error size byref, integer status (enum)
int job_query_error(job_query_t qry, const char **err, size_t *len);

// Check if a job matches job query criteria
int job_query_match_job(const job_query_t qry, long long job);

// Return integer start day for query
int job_query_start_day(const job_query_t qry, long long *start);

// Return integer end day for query
int job_query_end_day(const job_query_t qry, long long *end);

#endif /* JOBCOMP_QUERY_H */
