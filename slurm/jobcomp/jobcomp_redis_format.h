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

/*
 * From slurm/src/common/slurm_jobcomp.h: the jobcomp_job_rec_t below
 * itemizes the data that a jobcomp plugin manages.
 *
typedef struct {
 *  uint32_t jobid;       // JobID      +
 *  char *partition;      // Partition  +
 *  char *start_time;     // Start      +
 *  char *end_time;       // End        +
 *  time_t elapsed_time;  // Elapsed    +
 *  uint32_t uid;         // UID        +
 *  char *uid_name;       // User       +
 *  uint32_t gid;         // GID        +
 *  char *gid_name;       // Group      +
 *  uint32_t node_cnt;    // NNodes     +
 *  uint32_t proc_cnt;    // NCPUs      +
 *  char *nodelist;       // NodeList   +
 *  char *jobname;        // JobName    +
 *  char *state;          // State      +
 *  char *timelimit;      // TimeLimit  +
    char *blockid;        // BlockID    +
    char *connection;     // ?? unused by sacct
    char *reboot;         // ?? unused by sacct
    char *rotate;         // ?? unused by sacct
    uint32_t max_procs;   // ?? unused by sacct
    char *geo;            // ?? unused by sacct
    char *bg_start_point; // ?? unused by sacct
 *  char *work_dir;       // WorkDir    +
 *  char *resv_name;      // Reservation+
 *  char *req_gres;       // ReqGRES    +
 *  char *account;        // Account    +
 *  char *qos_name;       // QOS        +
 *  char *wckey;          // WCKey      +
 *  char *cluster;        // Cluster    +
 *  char *submit_time;    // Submit     // sacct uses job_comp->start_time (bug)
 *  char *eligible_time;  // Eligible   +
 *  char *derived_ec;     // DerivedExitCode +
 *  char *exit_code;      // ExitCode   +
} jobcomp_job_rec_t;
 */

#define MAX_REDIS_FIELDS 27

// Value index of redis field within redis_fields_t
enum redis_field_value_index {
    kJobID = 0, kPartition = 1, kStart = 2, kEnd = 3, kElapsed = 4, kUID = 5,
    kUser = 6, kGID = 7, kGroup = 8, kNNodes = 9, kNCPUs = 10, kNodeList = 11,
    kJobName = 12, kState = 13, kTimeLimit = 14, kBlockID = 15, kWorkDir = 16,
    kReservation = 17, kReqGRES = 18, kAccount = 19, kQOS = 20, kWCKey = 21,
    kCluster = 22, kSubmit = 23, kEligible = 24, kDerivedExitCode = 25,
    kExitCode = 26
};

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
