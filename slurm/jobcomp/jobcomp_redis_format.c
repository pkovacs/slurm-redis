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

#include "jobcomp_redis_format.h"

#include <assert.h>
#include <string.h>
#include <time.h>
// for uid_to_string, gid_to_string
#include <src/common/uid.h>
// for xmalloc, xfree
#include <src/common/xmalloc.h>
// for xstrdup
#include <src/common/xstring.h>

#include "slurm/common/ttl_hash.h"

static ttl_hash_t user_cache = NULL;
static ttl_hash_t group_cache = NULL;

void format_iso8601(time_t t, char **out)
{
    if (!out) {
        return;
    }
    if (t <= (time_t)0) {
        *out = xstrdup("Unknown");
    }
    const size_t iso8601_sz = 20;
    struct tm stm;
    gmtime_r(&t, &stm);
    *out = xmalloc(iso8601_sz);
    memset(*out, 0, iso8601_sz);
    strftime(*out, iso8601_sz, "%FT%T", &stm);
}

void jobcomp_redis_format_init(const jobcomp_redis_format_init_t *init)
{
    assert(init != NULL);
    ttl_hash_init_t user_cache_init = {
        .hash_sz = init->user_cache_sz,
        .hash_ttl = init->user_cache_ttl
    };
    ttl_hash_init_t group_cache_init = {
        .hash_sz = init->group_cache_sz,
        .hash_ttl = init->group_cache_ttl
    };
    user_cache = create_ttl_hash(&user_cache_init);
    group_cache = create_ttl_hash(&group_cache_init);
}

void jobcomp_redis_format_fini()
{
    destroy_ttl_hash(&user_cache);
    destroy_ttl_hash(&group_cache);
}

int jobcomp_redis_format_fields(const struct job_record *job, redis_fields_t **fields)
{
    assert(job != NULL);
    assert(fields != NULL);
    *fields = xmalloc(sizeof(redis_fields_t));
    memset(*fields, 0, sizeof(redis_fields_t));

    char buf[64];
    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf)-1, "%u", job->job_id);
    (*fields)->value[kJobID] = xstrdup(buf);

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf)-1, "%u", job->user_id);
    (*fields)->value[kUID] = xstrdup(buf);

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf)-1, "%u", job->group_id);
    (*fields)->value[kGID] = xstrdup(buf);

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf)-1, "%u", job->node_cnt);
    (*fields)->value[kNNodes] = xstrdup(buf);

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf)-1, "%u", job->total_cpus);
    (*fields)->value[kNCPUs] = xstrdup(buf);

    if (ttl_hash_get(user_cache, job->user_id, &(*fields)->value[kUser])
        != HASH_OK) {
        (*fields)->value[kUser] = uid_to_string(job->user_id);
        ttl_hash_set(user_cache, job->user_id, (*fields)->value[kUser]);
    }

    if (ttl_hash_get(group_cache, job->group_id, &(*fields)->value[kGroup])
        != HASH_OK) {
        (*fields)->value[kGroup] = gid_to_string(job->group_id);
        ttl_hash_set(group_cache, job->group_id, (*fields)->value[kGroup]);
    }

    uint32_t job_state;
    time_t start_time, end_time;
    if (IS_JOB_RESIZING(job)) {
        job_state = JOB_RESIZING;
        start_time = job->resize_time ? job->resize_time : job->start_time;
        end_time = time(NULL);
    } else {
        job_state = job->job_state & JOB_STATE_BASE;
        if (job->resize_time) {
            start_time = job->resize_time;
        } else if (job->start_time > job->end_time) {
            start_time = (time_t)0;
        } else {
            start_time = job->start_time;
        }
        end_time = job->end_time;
    }
    (*fields)->value[kState] = xstrdup(job_state_string(job_state));
    format_iso8601(start_time, &(*fields)->value[kStart]);
    format_iso8601(end_time, &(*fields)->value[kEnd]);

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf)-1, "%ld", end_time - start_time);
    (*fields)->value[kElapsed] = xstrdup(buf);

    (*fields)->value[kPartition] = xstrdup(job->partition);
    (*fields)->value[kNodeList] = xstrdup(job->nodes);

    if (job->name && *job->name) {
        (*fields)->value[kJobName] = xstrdup(job->name);
    } else {
        (*fields)->value[kJobName] = xstrdup("allocation");
    }

    if (job->time_limit == INFINITE) {
        (*fields)->value[kTimeLimit] = xstrdup("UNLIMITED");
    } else if (job->time_limit == NO_VAL) {
        (*fields)->value[kTimeLimit] = xstrdup("Partition_Limit");
    } else {
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf)-1, "%u", job->time_limit);
        (*fields)->value[kUID] = xstrdup(buf);
    }

    // Below and all the way to the bottom of the function are data for which
    // the absence of a value on the job record means we store no hash value
    // in redis at all, thus saving memory.

    if (job->details) {
        if (job->details->submit_time) {
            format_iso8601(job->details->submit_time, &(*fields)->value[kSubmit]);
        }
        if (job->details->begin_time) {
            format_iso8601(job->details->begin_time, &(*fields)->value[kEligible]);
        }
        if (job->details->work_dir && *job->details->work_dir) {
            (*fields)->value[kWorkDir] = xstrdup(job->details->work_dir);
        }
    }

    if (job->resv_name && *job->resv_name) {
        (*fields)->value[kReservation] = strdup(job->resv_name);
    }

    if (job->gres_req && *job->gres_req) {
        (*fields)->value[kReqGRES] = strdup(job->gres_req);
    }

    if (job->account && *job->account) {
        (*fields)->value[kAccount] = strdup(job->account);
    }

    if (job->qos_ptr && job->qos_ptr->name && *job->qos_ptr->name) {
        (*fields)->value[kQOS] = strdup(job->qos_ptr->name);
    }

    if (job->wckey && *job->wckey) {
        (*fields)->value[kWCKey] = strdup(job->wckey);
    }

    if (job->assoc_ptr && job->assoc_ptr->cluster
        && *job->assoc_ptr->cluster) {
        (*fields)->value[kCluster] = strdup(job->assoc_ptr->cluster);
    }

    int ec1 = 0, ec2 = 0;
    if (job->derived_ec == NO_VAL) {
    } else if (WIFSIGNALED(job->derived_ec)) {
        ec2 = WTERMSIG(job->derived_ec);
    } else if (WIFEXITED(job->derived_ec)) {
        ec1 = WEXITSTATUS(job->derived_ec);
    }
    if (ec1 || ec2) {
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf)-1, "%d:%d", ec1, ec2);
        (*fields)->value[kDerivedExitCode] = xstrdup(buf);
    }

    ec1 = ec2 = 0;
    if (job->exit_code == NO_VAL) {
    } else if (WIFSIGNALED(job->exit_code)) {
        ec2 = WTERMSIG(job->exit_code);
    } else if (WIFEXITED(job->exit_code)) {
        ec1 = WEXITSTATUS(job->exit_code);
    }
    if (ec1 || ec2) {
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf)-1, "%d:%d", ec1, ec2);
        (*fields)->value[kExitCode] = xstrdup(buf);
    }

    return SLURM_SUCCESS;
}

int jobcomp_redis_format_job(const redis_fields_t *fields, jobcomp_job_rec_t **job)
{
    assert(fields != NULL);
    assert(job != NULL);
    return SLURM_SUCCESS;
}
