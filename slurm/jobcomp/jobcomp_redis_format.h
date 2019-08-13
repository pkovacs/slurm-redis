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

#include <common/redis_fields.h>

/*
 * From slurm/src/common/slurm_jobcomp.h: the jobcomp_job_rec_t below
 * itemizes the data that a jobcomp plugin manages.
 *
typedef struct {
 x  uint32_t jobid;       // JobID      +
 x  char *partition;      // Partition  +
 x  char *start_time;     // Start      +
 x  char *end_time;       // End        +
 x  time_t elapsed_time;  // Elapsed    +
 x  uint32_t uid;         // UID        +
 x  char *uid_name;       // User       +
 x  uint32_t gid;         // GID        +
 x  char *gid_name;       // Group      +
 x  uint32_t node_cnt;    // NNodes     +
 x  uint32_t proc_cnt;    // NCPUs      +
 x  char *nodelist;       // NodeList   +
 x  char *jobname;        // JobName    +
 x  char *state;          // State      +
 x  char *timelimit;      // TimeLimit  +
    char *blockid;        // ?? bluegene only -- sourced from job comment ?? +
    char *connection;     // ?? unused by sacct
    char *reboot;         // ?? unused by sacct
    char *rotate;         // ?? unused by sacct
    uint32_t max_procs;   // ?? unused by sacct
    char *geo;            // ?? unused by sacct
    char *bg_start_point; // ?? unused by sacct
 x  char *work_dir;       // WorkDir    +
 x  char *resv_name;      // Reservation+
 x  char *req_gres;       // ReqGRES    +
 x  char *account;        // Account    +
 x  char *qos_name;       // QOS        +
 x  char *wckey;          // WCKey      +
 x  char *cluster;        // Cluster    +
 x  char *submit_time;    // Submit     // sacct uses job_comp->start_time (bug)
 x  char *eligible_time;  // Eligible   +
 x  char *derived_ec;     // DerivedExitCode +
 x  char *exit_code;      // ExitCode   +
} jobcomp_job_rec_t;
 */

// Array of redis field values
typedef struct redis_fields {
    char *value[MAX_REDIS_FIELDS];
} redis_fields_t;

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
int jobcomp_redis_format_fields(const struct job_record *job,
    redis_fields_t **fields);

// Format a jobcomp_job_rec_t from redis fields (redis to slurm)
int jobcomp_redis_format_job(const redis_fields_t *fields,
    jobcomp_job_rec_t **job);

// Format time_t into string (-DISO8601_DATES=ON/OFF option)
char *jobcomp_redis_format_time(time_t t);

#endif /* JOBCOMP_REDIS_FORMAT_H */
