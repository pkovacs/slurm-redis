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

#include <string.h>
#include <hiredis.h>
#include <slurm/slurm.h>
#include <slurm/spank.h>
#include <src/common/xmalloc.h>
#include <src/common/xstring.h>
#include <src/slurmctld/slurmctld.h>

#include "jobcomp_redis_formatter.h"

#define USER_CACHE_SZ 64
#define GROUP_CACHE_SZ 64
#define USER_CACHE_TTL 120
#define GROUP_CACHE_TTL 120

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

// Field labels for all supported redis fields
static const char *field_label[] = {
    "JobID", "Partition", "Start", "End", "Elapsed", "UID", "User", "GID",
    "Group", "NNodes", "NCPUs", "NodeList", "JobName", "State", "TimeLimit",
    "BlockID", "WorkDir", "Reservation", "ReqGRES", "Account", "QOS", "WCKey",
    "Cluster", "Submit", "Eligible", "DerivedExitCode", "ExitCode"
};

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
        .user_cache_sz = USER_CACHE_SZ,
        .user_cache_ttl = USER_CACHE_TTL,
        .group_cache_sz = GROUP_CACHE_SZ,
        .group_cache_ttl = GROUP_CACHE_TTL
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
        keytag = xstrdup(location);
    } else {
        keytag = xstrdup("");
    }
    return SLURM_SUCCESS;
}

int slurm_jobcomp_log_record(struct job_record *job)
{
    if (!job) {
        return SLURM_SUCCESS;
    }
    redisReply *reply;
    redis_fields_t *fields = NULL;
    int rc = jobcomp_redis_format_fields(job, &fields);
    if (rc != SLURM_SUCCESS) {
        // todo, e.g. we MUST have fields->value[kJobID]
    }

    // Pipeline redis commands
    int i = 0, val_count = 0;
    for (; i < MAX_REDIS_FIELDS; ++i) {
        if (fields->value[i]) {
            redisAppendCommand(ctx, "HSET %s:%s %s %s",
                keytag, fields->value[kJobID],
                field_label[i], fields->value[i]);
            ++val_count;
        }
    }

    // Pop off the pipelined replies
    for (i = 0; i < val_count; ++i) {
        redisGetReply(ctx, (void **)&reply);
        freeReplyObject(reply);
        reply = NULL;
    }

    // Free Mars
    for (i = 0; i < MAX_REDIS_FIELDS; ++i) {
        if (fields->value[i]) {
            xfree(fields->value[i]);
            fields->value[i] = NULL;
        }
    }
    xfree(fields);
    fields = NULL;

    return SLURM_SUCCESS;
}

int slurm_jobcomp_get_errno(void)
{
    return SLURM_SUCCESS;
}

char *slurm_jobcomp_strerror(__attribute__ ((unused)) int errnum)
{
    return NULL;
}

List slurm_jobcomp_get_jobs(__attribute__ ((unused)) void *job_cond)
{
    return NULL;
}

int slurm_jobcomp_archive(__attribute__ ((unused)) void *arch_cond)
{
    return SLURM_SUCCESS;
}
