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

#include "src/common/ttl_hash.h"

static ttl_hash_t usnm_cache = NULL;
static ttl_hash_t grnm_cache = NULL;
static void *(*malloc_fn)(size_t) = NULL;
static void (*free_fn)(void *) = NULL;
static char *(*strdup_fn)(const char *) = NULL;

void format_iso8601(time_t t, char **out)
{
    if (!out) {
        return;
    }
    if (t <= (time_t)0) {
        *out = (*strdup_fn)("Unknown");
    }
    const size_t iso8601_sz = 20;
    struct tm stm;
    gmtime_r(&t, &stm);
    *out = (*malloc_fn)(iso8601_sz);
    memset(*out, 0, iso8601_sz);
    strftime(*out, iso8601_sz, "%FT%T", &stm);
}

void jobcomp_redis_format_init(const jobcomp_redis_format_init_t *init)
{
    assert(init != NULL);
    ttl_hash_init_t usnm_cache_init = {
        .sz = init->usnm_cache_sz,
        .ttl = init->usnm_cache_ttl,
        .malloc_fn = init->malloc_fn,
        .free_fn = init->free_fn,
        .strdup_fn = init->strdup_fn
    };
    ttl_hash_init_t grnm_cache_init = {
        .sz = init->grnm_cache_sz,
        .ttl = init->grnm_cache_ttl,
        .malloc_fn = init->malloc_fn,
        .free_fn = init->free_fn,
        .strdup_fn = init->strdup_fn
    };
    usnm_cache = create_ttl_hash(&usnm_cache_init);
    grnm_cache = create_ttl_hash(&grnm_cache_init);
    malloc_fn = init->malloc_fn;
    free_fn = init->free_fn;
    strdup_fn = init->strdup_fn;
}

void jobcomp_redis_format_fini()
{
    destroy_ttl_hash(usnm_cache);
    usnm_cache = NULL;
    destroy_ttl_hash(grnm_cache);
    grnm_cache = NULL;
}

int jobcomp_redis_format_fields(const struct job_record *job, redis_fields_t **fields)
{
    assert(fields != NULL);
    *fields = (*malloc_fn)(sizeof(redis_fields_t));
    memset(*fields, 0, sizeof(redis_fields_t));

    char buf[64];
    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf)-1, "%u", job->job_id);
    (*fields)->value[kJobID] = (*strdup_fn)(buf);

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf)-1, "%u", job->user_id);
    (*fields)->value[kUID] = (*strdup_fn)(buf);

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf)-1, "%u", job->group_id);
    (*fields)->value[kGID] = (*strdup_fn)(buf);

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf)-1, "%u", job->node_cnt);
    (*fields)->value[kNNodes] = (*strdup_fn)(buf);

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf)-1, "%u", job->total_cpus);
    (*fields)->value[kNCPUs] = (*strdup_fn)(buf);

    if (ttl_hash_get(usnm_cache, job->user_id, &(*fields)->value[kUser])
        != HASH_OK) {
        (*fields)->value[kUser] = uid_to_string(job->user_id);
        ttl_hash_set(usnm_cache, job->user_id, (*fields)->value[kUser]);
    }

    if (ttl_hash_get(grnm_cache, job->group_id, &(*fields)->value[kGroup])
        != HASH_OK) {
        (*fields)->value[kGroup] = gid_to_string(job->group_id);
        ttl_hash_set(grnm_cache, job->group_id, (*fields)->value[kGroup]);
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
    (*fields)->value[kState] = (*strdup_fn)(job_state_string(job_state));
    format_iso8601(start_time, &(*fields)->value[kStart]);
    format_iso8601(end_time, &(*fields)->value[kEnd]);

    if (job->details) {
        format_iso8601(job->details->submit_time, &(*fields)->value[kSubmit]);
        format_iso8601(job->details->begin_time, &(*fields)->value[kEligible]);
    }

    (*fields)->value[kPartition] = (*strdup_fn)(job->partition);
    (*fields)->value[kNodeList] = (*strdup_fn)(job->nodes);

    if (job->name && *job->name) {
        (*fields)->value[kJobName] = (*strdup_fn)(job->name);
    } else {
        (*fields)->value[kJobName] = (*strdup_fn)("allocation");
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
