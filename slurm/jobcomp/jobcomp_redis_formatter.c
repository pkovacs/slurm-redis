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
#include <time.h>
#include <src/common/uid.h>

#include "slurm/common/ttl_hash.h"

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

jobcomp_redis_t *jobcomp_redis_alloc()
{
    return (*malloc_fn)(sizeof(jobcomp_redis_t));
}

void jobcomp_redis_free(jobcomp_redis_t *job)
{
    if (!job) {
        return;
    }
    (*free_fn)(job->job_state_txt);
    (*free_fn)(job->start_time);
    (*free_fn)(job->end_time);
    (*free_fn)(job->submit_time);
    (*free_fn)(job->eligible_time);
    (*free_fn)(job->account);
    (*free_fn)(job->cluster);
    (*free_fn)(job->user_name);
    (*free_fn)(job->group_name);
    (*free_fn)(job->job_name);
    (*free_fn)(job->partition);
    (*free_fn)(job->node_list);
    (*free_fn)(job->qos_name);
    (*free_fn)(job->resv_name);
    (*free_fn)(job->req_gres);
    (*free_fn)(job->wckey);
    (*free_fn)(job->derived_ec);
    (*free_fn)(job->exit_code);
    (*free_fn)(job->work_dir);
    (*free_fn)(job);
}
/*
 * uint32_t job_id;      // jbid
 * uint32_t uid;         // usid
 * uint32_t gid;         // grid
 * uint32_t job_state;   // jbsc
 * uint32_t node_cnt;    // ndct
 * uint32_t proc_cnt;    // prct
 * uint32_t maxprocs;    // prmx
 * uint32_t time_limit;  // tmlm
 * char *job_state_text; // jbst
 * char *start_time;     // sttm
 * char *end_time;       // entm
 * char *submit_time;    // sbtm
 * char *eligible_time;  // eltm
    char *account;        // acct
    char *cluster;        // clus
 * char *user_name;      // usnm
 * char *group_name;     // grnm
 * char *job_name;       // jbnm
 * char *partition;      // part
 * char *node_list;      // ndls
    char *qos_name;       // qsnm
    char *resv_name;      // rvnm
    char *req_gres;       // rvgr
    char *wckey;          // wcky
    char *derived_ec;     // dxcd
    char *exit_code;      // excd
    char *work_dir;       // wkdr

 */
int jobcomp_redis_format_job(const struct job_record *in, jobcomp_redis_t *out)
{
    if (!in || !out) {
        return SLURM_ERROR;
    }
    out->job_id = in->job_id;
    out->uid = in->user_id;
    out->gid = in->group_id;

    time_t start_time, end_time;
    if (IS_JOB_RESIZING(in)) {
        out->job_state = JOB_RESIZING;
        start_time = in->resize_time ? in->resize_time : in->start_time;
        end_time = time(NULL);
    } else {
        out->job_state = in->job_state & JOB_STATE_BASE;
        if (in->resize_time) {
            start_time = in->resize_time;
        } else if (in->start_time > in->end_time) {
            start_time = (time_t)0;
        } else {
            start_time = in->start_time;
        }
        end_time = in->end_time;
    }
    format_iso8601(start_time, &out->start_time);
    format_iso8601(end_time, &out->end_time);

    if (in->details) {
        format_iso8601(in->details->submit_time, &out->submit_time);
        format_iso8601(in->details->begin_time, &out->eligible_time);
    }

    out->node_cnt = in->node_cnt;
    out->proc_cnt = in->total_cpus;
    out->maxprocs = in->details ? in->details->max_cpus : NO_VAL;

    if ((in->time_limit == NO_VAL) && in->part_ptr) {
        out->time_limit = in->part_ptr->max_time;
    } else {
        out->time_limit = in->time_limit;
    }

    out->job_state_txt = (*strdup_fn)(job_state_string(out->job_state));

    if (ttl_hash_get(usnm_cache, out->uid, &out->user_name) != HASH_OK) {
        out->user_name = uid_to_string(out->uid);
        ttl_hash_set(usnm_cache, out->uid, out->user_name);
    }

    if (ttl_hash_get(grnm_cache, out->gid, &out->group_name) != HASH_OK) {
        out->group_name = gid_to_string(out->gid);
        ttl_hash_set(grnm_cache, out->gid, out->group_name);
    }

    if (in->name && *in->name) {
        out->job_name = slurm_add_slash_to_quotes(in->name);
    } else {
        out->job_name = (*strdup_fn)("allocation");
    }

    out->partition = (*strdup_fn)(in->partition);
    out->node_list = (*strdup_fn)(in->nodes);

    return SLURM_SUCCESS;
}
