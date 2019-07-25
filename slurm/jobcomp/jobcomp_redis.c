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

#include <hiredis.h>
#include <slurm/slurm.h>
#include <slurm/spank.h>
#include <src/common/xmalloc.h>
#include <src/common/xstring.h>
#include <src/slurmctld/slurmctld.h>

#include "jobcomp_redis_formatter.h"

#define USNM_CACHE_SZ 64
#define GRNM_CACHE_SZ 64
#define USNM_CACHE_TTL 120
#define GRNM_CACHE_TTL 120

const char plugin_name[] = "Job completion logging redis plugin";
const char plugin_type[] = "jobcomp/redis";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

/*
 * Could use: mysql_db_info_t *create_mysql_db_info() to get host/port/pass
 */

static const char *host = "127.0.0.1";
static const int port = 6379;
static redisContext *ctx = NULL;
static char *keytag = NULL;

/*
 * Shims for the slurm allocators which are macros whose address we cannot
 * take directly.
 */
static void *malloc_shim(size_t sz) {
    return xmalloc(sz);
}

static void free_shim(void *mem) {
    xfree(mem);
}

static char *strdup_shim(const char *s) {
    return xstrdup(s);
}

int init(void)
{
    static int once = 0;
    if (!once) {
        slurm_verbose("%s loaded", plugin_name);
        once = 1;
    } else {
        slurm_debug("%s loaded", plugin_name);
    }
    jobcomp_redis_format_init_t format_init = {
        .usnm_cache_sz = USNM_CACHE_SZ,
        .usnm_cache_ttl = USNM_CACHE_TTL,
        .grnm_cache_sz = GRNM_CACHE_SZ,
        .grnm_cache_ttl = GRNM_CACHE_TTL,
        .malloc_fn = malloc_shim,
        .free_fn = free_shim,
        .strdup_fn = strdup_shim
    };
    jobcomp_redis_format_init(&format_init);
    return SLURM_SUCCESS;
}

int fini(void)
{
    if (ctx != NULL) {
        slurm_debug("%s finished", plugin_name);
        redisFree(ctx);
        ctx = NULL;
    }
    if (keytag) {
        xfree(keytag);
        keytag = NULL;
    }
    jobcomp_redis_format_fini();
    return SLURM_SUCCESS;
}

int slurm_jobcomp_set_location(char *location)
{
    // The location is an optional tag that will be prepended to all redis keys
    // This can help segregate slurm keys from other keys in redis
    ctx = redisConnect(host, port);
    if (ctx == NULL || ctx->err) {
        slurm_error("Connection error: %s\n", ctx->errstr);
        return SLURM_ERROR;
    }
    if (location) {
        slurm_debug("%s: location %s", plugin_type, location);
        keytag = xstrdup_printf("%s:", location);
    } else {
        keytag = xstrdup("");
    }
    return SLURM_SUCCESS;
}

int slurm_jobcomp_log_record(struct job_record *in)
{
    if (!in) {
        return SLURM_SUCCESS;
    }
    jobcomp_redis_t *out = jobcomp_redis_alloc();
    int rc = jobcomp_redis_format_job(in, out);
    if (rc != SLURM_SUCCESS) {
        jobcomp_redis_free(out);
        return rc;
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
    // MULTI/EXEC for hash set + indices
    redisReply *reply;
    reply = redisCommand(ctx,
            "HMSET %s%u "
            "jbid %u usid %u grid %u "
            "jbsc %u ndct %u prct %u "
            "prmx %u tmlm %u "
            "jbst %s "
            "sttm %s entm %s "
            "sbtm %s eltm %s "
            "usnm %s grnm %s jbnm %s "
            "part %s ndls %s"
            ,
            keytag, out->job_id,
            out->job_id, out->uid, out->gid,
            out->job_state, out->node_cnt, out->proc_cnt,
            out->maxprocs, out->time_limit,
            out->job_state_txt,
            out->start_time, out->end_time,
            out->submit_time, out->eligible_time,
            out->user_name, out->group_name, out->job_name,
            out->partition, out->node_list);

    freeReplyObject(reply);
    return SLURM_SUCCESS;
}

int slurm_jobcomp_get_errno(void)
{
    return SLURM_SUCCESS;
}

char *slurm_jobcomp_strerror(int errnum)
{
    if (errnum) {}
    return NULL;
}

List slurm_jobcomp_get_jobs(void *job_cond)
{
    if (job_cond) {}
    return NULL;
}

int slurm_jobcomp_archive(void *arch_cond)
{
    if (arch_cond) {}
    return SLURM_SUCCESS;
}
