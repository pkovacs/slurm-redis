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

#ifndef JOBCOMP_REDIS_FORMAT_H
#define JOBCOMP_REDIS_FORMAT_H

#include <stdint.h>
#include <time.h>

#include <src/common/slurm_jobcomp.h> /* jobcomp_job_rec_t */
#include <src/slurmctld/slurmctld.h> /* struct job_record */

#include "common/redis_fields.h"
#include "jobcomp_redis_auto.h"

// Job formatter initialization
typedef struct jobcomp_redis_format_init {
    // Number of uid->user_name cache entries
    size_t user_cache_sz;
    // Time-to-live of uid->user_name cache entries
    size_t user_cache_ttl;
    // Number of gid->group_name cache entries
    size_t group_cache_sz;
    // Time-to-live of gid->group_name cache entries
    size_t group_cache_ttl;
} jobcomp_redis_format_init_t;

// Initialize the formatter
void jobcomp_redis_format_init(const jobcomp_redis_format_init_t *init);

// De-initialize the formatter
void jobcomp_redis_format_fini();

// Format redis fields from a struct job_record (slurm to redis)
int jobcomp_redis_format_fields(unsigned int tmf, const struct job_record *job,
    redis_fields_t *fields);

// Create and format a jobcomp_job_rec_t from redis fields (redis to slurm)
int jobcomp_redis_format_job(const redis_fields_t *fields,
    jobcomp_job_rec_t *job);

// Format time_t into string for redis
char *jobcomp_redis_format_time(unsigned int tmf, time_t t);

#endif /* JOBCOMP_REDIS_FORMAT_H */
