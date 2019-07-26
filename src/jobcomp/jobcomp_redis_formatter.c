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

#include "jobcomp_redis_formatter.h"

#include <assert.h>
#include <string.h>
#include <time.h>
#include <src/common/uid.h>
#include <src/common/xmalloc.h>
#include <src/common/xstring.h>

#include "src/common/ttl_hash.h"

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
    destroy_ttl_hash(user_cache);
    user_cache = NULL;
    destroy_ttl_hash(group_cache);
    group_cache = NULL;
}

int jobcomp_redis_format_fields(const struct job_record *job, redis_fields_t **fields)
{
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

    if (job->details) {
        format_iso8601(job->details->submit_time, &(*fields)->value[kSubmit]);
        format_iso8601(job->details->begin_time, &(*fields)->value[kEligible]);
    }

    (*fields)->value[kPartition] = xstrdup(job->partition);
    (*fields)->value[kNodeList] = xstrdup(job->nodes);

    if (job->name && *job->name) {
        (*fields)->value[kJobName] = xstrdup(job->name);
    } else {
        (*fields)->value[kJobName] = xstrdup("allocation");
    }

    return SLURM_SUCCESS;
}

#if 0
int jobcomp_redis_format_job(const struct job_record *in, jobcomp_redis_t *out)
{

    out->maxprocs = in->details ? in->details->max_cpus : NO_VAL;

    if ((in->time_limit == NO_VAL) && in->part_ptr) {
        out->time_limit = in->part_ptr->max_time;
    } else {
        out->time_limit = in->time_limit;
    }

    return SLURM_SUCCESS;
}
#endif
