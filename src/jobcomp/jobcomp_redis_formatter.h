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

#ifndef JOBCOMP_REDIS_FORMATTER_H
#define JOBCOMP_REDIS_FORMATTER_H

#include <stdint.h>
#include <time.h>
#include <src/slurmctld/slurmctld.h>

/*
 * The job data we will store in redis
 */
typedef struct {
    uint32_t job_id;      // jbid
    uint32_t uid;         // usid
    uint32_t gid;         // grid
    uint32_t job_state;   // jbsc
    uint32_t node_cnt;    // ndct
    uint32_t proc_cnt;    // prct
    uint32_t maxprocs;    // prmx
    uint32_t time_limit;  // tmlm
    char *job_state_txt;  // jbst
    char *start_time;     // sttm
    char *end_time;       // entm
    char *submit_time;    // sbtm
    char *eligible_time;  // eltm
    char *account;        // acct
    char *cluster;        // clus
    char *user_name;      // usnm
    char *group_name;     // grnm
    char *job_name;       // jbnm
    char *partition;      // part
    char *node_list;      // ndls
    char *qos_name;       // qsnm
    char *resv_name;      // rvnm
    char *req_gres;       // rvgr
    char *wckey;          // wcky
    char *derived_ec;     // dxcd
    char *exit_code;      // excd
    char *work_dir;       // wkdr
} jobcomp_redis_t;

// Job formatter initialization
typedef struct jobcomp_redis_format_init {
    // Number of uid->user_name cache entries
    size_t usnm_cache_sz;
    // Time-to-live of uid->user_name cache entries
    size_t usnm_cache_ttl;
    // Number of gid->group_name cache entries
    size_t grnm_cache_sz;
    // Time-to-live of gid->group_name cache entries
    size_t grnm_cache_ttl;
    // malloc function
    void *(*malloc_fn)(size_t);
    // free function
    void (*free_fn)(void *);
    // strdup function
    char *(*strdup_fn)(const char *);
} jobcomp_redis_format_init_t;

// Initialize the formatter
void jobcomp_redis_format_init(const jobcomp_redis_format_init_t *init);

// De-initialize the formatter
void jobcomp_redis_format_fini();

// Allocate a jobcomp_redis_t
jobcomp_redis_t *jobcomp_redis_alloc();

// Free a jobcomp_redis_t
void jobcomp_redis_free(jobcomp_redis_t *job);

// Format a jobcomp_redis_t from a struct job_record
int jobcomp_redis_format_job(const struct job_record *job_in, jobcomp_redis_t *job_out);

#endif /* JOBCOMP_REDIS_FORMATTER_H */
