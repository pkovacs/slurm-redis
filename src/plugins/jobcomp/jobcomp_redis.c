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
#include <uuid.h>
// for List, SLURM_VERSION_NUMBER, SLURM_SUCCESS, etc.
#include <slurm/slurm.h>
// for slurm_verbose, slurm_debug, etc.
#include <slurm/spank.h>
// for xmalloc, xfree
#include <src/common/xmalloc.h>
// for xstrdup
#include <src/common/xstring.h>
// for struct job_record
#include <src/slurmctld/slurmctld.h>

#include "jobcomp_redis_format.h"

#define USER_CACHE_SZ 64
#define GROUP_CACHE_SZ 64
#define USER_CACHE_TTL 120
#define GROUP_CACHE_TTL 120

#define QUERY_KEY_TTL 300

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

// Field labels for redis fields
static const char *field_labels[] = {
    "JobID", "Partition", "Start", "End", "Elapsed", "UID", "User", "GID",
    "Group", "NNodes", "NCPUs", "NodeList", "JobName", "State", "TimeLimit",
    "BlockID", "WorkDir", "Reservation", "ReqGRES", "Account", "QOS", "WCKey",
    "Cluster", "Submit", "Eligible", "DerivedExitCode", "ExitCode"
};

static int redis_connect(void)
{
    if (ctx) {
        redisFree(ctx);
        ctx = NULL;
    }
    ctx = redisConnect(host, port);
    if (!ctx || ctx->err) {
        slurm_error("Connection error: %s\n", ctx->errstr);
        return SLURM_ERROR;
    }
    return SLURM_SUCCESS;
}

static int redis_connected(void)
{
    int rc = 0;
    if (!ctx) {
        return rc;
    }
    redisReply *reply;
    reply = redisCommand(ctx, "PING");
    if (ctx->err == 0) {
        freeReplyObject(reply);
        rc = 1;
    }
    return rc;
}

static int redis_sadd_list(const char *key, const List list) {
    char *value;
    int pipeline = 0;
    ListIterator it = list_iterator_create(list);
    while ((value = list_next(it))) {
        redisAppendCommand(ctx, "SADD %s %s", key, value);
        ++pipeline;
    }
    redisAppendCommand(ctx, "EXPIRE %s %u", key, QUERY_KEY_TTL);
    ++pipeline;
    list_iterator_destroy(it);
    return pipeline;
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
    if (ctx) {
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
    if (redis_connected()) {
        return SLURM_SUCCESS;
    }

    if (redis_connect() != SLURM_SUCCESS) {
        return SLURM_ERROR;
    }

    if (location) {
        keytag = xstrdup(location);
    } else {
        if (!(keytag = slurm_get_jobcomp_loc())) {
            keytag = xstrdup("job");
        }
    }

    return SLURM_SUCCESS;
}

int slurm_jobcomp_log_record(struct job_record *job)
{
    if (!job) {
        return SLURM_SUCCESS;
    }

    if (!redis_connected()) {
        redis_connect();
    }

    redis_fields_t *fields = NULL;
    int rc = jobcomp_redis_format_fields(job, &fields);
    if (rc != SLURM_SUCCESS) {
        // TODO: e.g. we MUST have fields->value[kJobID]
    }

    // Add the job's field-value pairs to a new redis key
    int i = 0, pipeline = 0;
    for (; i < MAX_REDIS_FIELDS; ++i) {
        if (fields->value[i]) {
            redisAppendCommand(ctx, "HSET %s:%s %s %s",
                keytag, fields->value[kJobID],
                field_labels[i], fields->value[i]);
            ++pipeline;
        }
    }

    // Index the job on the redis server
    redisAppendCommand(ctx, "SLURMJC.INDEX %s %s",
        keytag, fields->value[kJobID]);
    ++pipeline;

    // TODO: error checking
    // Pop off the pipelined replies
    redisReply *reply;
    for (i = 0; i < pipeline; ++i) {
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

#if 0
typedef struct {
    List acct_list;     /* list of char * */
    List associd_list;  /* list of char */
    List cluster_list;  /* list of char * */
    uint32_t cpus_max;  /* number of cpus high range */
    uint32_t cpus_min;  /* number of cpus low range */
    int32_t exitcode;   /* exit code of job */
    uint32_t flags;     /* Reporting flags*/
    List format_list;   /* list of char * */
    List groupid_list;  /* list of char * */
    List jobname_list;  /* list of char * */
    uint32_t nodes_max; /* number of nodes high range */
    uint32_t nodes_min; /* number of nodes low range */
    List partition_list;/* list of char * */
    List qos_list;      /* list of char * */
    List resv_list;     /* list of char * */
    List resvid_list;   /* list of char * */
    List state_list;    /* list of char * */
    List step_list;     /* list of slurmdb_selected_step_t */
    uint32_t timelimit_max; /* max timelimit */
    uint32_t timelimit_min; /* min timelimit */
    time_t usage_end;
    time_t usage_start;
    char *used_nodes;   /* a ranged node string where jobs ran */
    List userid_list;   /* list of char * */
    List wckey_list;    /* list of char * */
} slurmdb_job_cond_t;
#endif


List slurm_jobcomp_get_jobs(slurmdb_job_cond_t *job_cond)
{
    if (!job_cond) {
        return SLURM_SUCCESS;
    }

    if (!redis_connected()) {
        redis_connect();
    }

    int i, pipeline = 0;
    char uuid_s[37];
    char key[128];
    uuid_t uuid;

    // Generate a random uuid for the request keys
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_s);

    // Create redis hash set for the scalar condition values
    redisAppendCommand(ctx, "HSET %s:qry:%s Start %ld End %ld", keytag,
        uuid_s, job_cond->usage_start, job_cond->usage_end);
    redisAppendCommand(ctx, "EXPIRE %s:qry:%s %u", keytag, uuid_s,
        QUERY_KEY_TTL);
    pipeline += 2;

    // Create redis set for userid_list
    if ((job_cond->userid_list) && list_count(job_cond->userid_list)) {
        memset(key, 0, sizeof(key));
        snprintf(key, sizeof(key)-1, "%s:qry:%s:uid", keytag, uuid_s);
        pipeline += redis_sadd_list(key, job_cond->userid_list);
    }

    // Create redis set for groupid_list
    if ((job_cond->groupid_list) && list_count(job_cond->groupid_list)) {
        memset(key, 0, sizeof(key));
        snprintf(key, sizeof(key)-1, "%s:qry:%s:gid", keytag, uuid_s);
        pipeline += redis_sadd_list(key, job_cond->groupid_list);
    }

    // Create redis set for jobname_list
    if ((job_cond->jobname_list) && list_count(job_cond->jobname_list)) {
        memset(key, 0, sizeof(key));
        snprintf(key, sizeof(key)-1, "%s:qry:%s:jobname", keytag, uuid_s);
        pipeline += redis_sadd_list(key, job_cond->jobname_list);
    }

    // Create redis set for partition_list
    if ((job_cond->partition_list) && list_count(job_cond->partition_list)) {
        memset(key, 0, sizeof(key));
        snprintf(key, sizeof(key)-1, "%s:qry:%s:partition", keytag, uuid_s);
        pipeline += redis_sadd_list(key, job_cond->partition_list);
    }

    // Ask the redis server for matches to the criteria we sent
    redisAppendCommand(ctx, "SLURMJC.MATCH %s %s", keytag, uuid_s);
    ++pipeline;

    // TODO: error checking
    // Pop off the pipelined replies
    redisReply *reply;
    for (i = 0; i < pipeline; ++i) {
        redisGetReply(ctx, (void **)&reply);
        freeReplyObject(reply);
        reply = NULL;
    }

    // TODO: send deletes to redis for the qry and result set
    return NULL;
}

int slurm_jobcomp_get_errno(void)
{
    return SLURM_SUCCESS;
}

char *slurm_jobcomp_strerror(__attribute__ ((unused)) int errnum)
{
    return NULL;
}

int slurm_jobcomp_archive(__attribute__ ((unused)) void *arch_cond)
{
    return SLURM_SUCCESS;
}
