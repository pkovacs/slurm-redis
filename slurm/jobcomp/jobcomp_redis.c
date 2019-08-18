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

#include <stdlib.h>
#include <string.h>
#include <hiredis.h>
#include <uuid.h>

#include <slurm/slurm.h> /* SLURM_VERSION_NUMBER, slurm_list_next, ... */
#include <slurm/spank.h> /* slurm_debug, ... */
#include <src/common/xmalloc.h> /* xmalloc, ... */
#include <src/common/xstring.h> /* xstrdup, ... */
#include <src/slurmctld/slurmctld.h> /* struct job_record */

#include "common/redis_fields.h"
#include "jobcomp_redis_auto.h"
#include "jobcomp_redis_format.h"

const char plugin_name[] = "Job completion logging redis plugin";
const char plugin_type[] = "jobcomp/redis";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

#ifdef USE_ISO8601
const unsigned int _tmf = 1;
#else
const unsigned int _tmf = 0;
#endif

static const char *host = NULL;
static uint32_t port = 0;
static const char *pass = NULL;
static const char *prefix = NULL;
static redisContext *ctx = NULL;

static int redis_connect(void)
{
    if (ctx) {
        redisFree(ctx);
        ctx = NULL;
    }
    ctx = redisConnect(host, port);
    if (!ctx || ctx->err) {
        slurm_error("redis connect error: %s", ctx->errstr);
        return SLURM_ERROR;
    }
    if (pass) {
        AUTO_REPLY redisReply *reply = redisCommand(ctx, "AUTH %s", pass);
        if (reply && (reply->type == REDIS_REPLY_ERROR)) {
            slurm_debug("redis error: %s", reply->str);
            return SLURM_ERROR;
        }
    }
    return SLURM_SUCCESS;
}

static int redis_connected(void)
{
    if (!ctx) {
        return 0;
    }
    AUTO_REPLY redisReply *reply = redisCommand(ctx, "PING");
    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            slurm_debug("redis error: %s", reply->str);
            return 0;
        }
        if (reply->type == REDIS_REPLY_STATUS) {
            return strcmp(reply->str, "PONG") == 0;
        }
    }
    return 0;
}

static int redis_add_job_criteria(const char *key, const List list) {
    int pipeline = 0;
    const char *value;
    AUTO_LITER ListIterator it = slurm_list_iterator_create(list);
    while ((value = slurm_list_next(it))) {
        redisAppendCommand(ctx, "SADD %s %s", key, value);
        ++pipeline;
    }
    redisAppendCommand(ctx, "EXPIRE %s %u", key, QUERY_TTL);
    ++pipeline;
    return pipeline;
}

static int redis_add_job_steps(const char *key, const List list) {
    int pipeline = 0;
    char buf[32];
    const slurmdb_selected_step_t *step;
    AUTO_LITER ListIterator it = slurm_list_iterator_create(list);
    while ((step = slurm_list_next(it))) {
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf)-1, "%u", step->jobid);
        redisAppendCommand(ctx, "SADD %s %s", key, buf);
        ++pipeline;
    }
    redisAppendCommand(ctx, "EXPIRE %s %u", key, QUERY_TTL);
    ++pipeline;
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
    if (!host) {
        host = slurm_get_jobcomp_host();
        slurm_debug("redis host %s", host);
    }
    if (!port) {
        port = slurm_get_jobcomp_port();
        slurm_debug("redis port %u", port);
    }
    if (!pass) {
        pass = slurm_get_jobcomp_pass();
    }
    jobcomp_redis_format_init_t format_init = {
        .user_cache_sz = ID_CACHE_SIZE,
        .user_cache_ttl = ID_CACHE_TTL,
        .group_cache_sz = ID_CACHE_SIZE,
        .group_cache_ttl = ID_CACHE_TTL
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
    xfree(host);
    xfree(pass);
    xfree(prefix);
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
    if (prefix) {
        return SLURM_SUCCESS;
    }

    AUTO_STR char *loc = location ? xstrdup(location) : slurm_get_jobcomp_loc();
    if (!loc || strcmp(loc, DEFAULT_JOB_COMP_LOC) == 0) {
        prefix = xstrdup("job");
    } else if (loc) {
        prefix = xstrdup_printf("%s:job", loc);
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
    if (!redis_connected()) {
        return SLURM_ERROR;
    }

    AUTO_FIELDS redis_fields_t fields = {0};
    int rc = jobcomp_redis_format_fields(_tmf, job, &fields);
    if (rc != SLURM_SUCCESS) {
        return SLURM_ERROR;
    }

    int i, err = 0, pipeline = 0;

    // Start a multi-statement transaction to cover the creation of the job key
    // and creation/update of the index
    redisAppendCommand(ctx, "MULTI");
    ++pipeline;

    // Add the job's field-value pairs to a redis hash set
    for (i = 0; i < MAX_REDIS_FIELDS; ++i) {
        if (fields.value[i]) {
            redisAppendCommand(ctx, "HSET %s:%s %s %s", prefix,
                fields.value[kJobID], redis_field_labels[i],
                fields.value[i]);
            ++pipeline;
        }
    }
    if (TTL > 0) {
        redisAppendCommand(ctx, "EXPIRE %s:%s %lld", prefix,
            fields.value[kJobID], TTL);
        ++pipeline;
    }

    // Use SLURMJC.INDEX to index the job on the redis server
    redisAppendCommand(ctx, "SLURMJC.INDEX %s %s", prefix,
        fields.value[kJobID]);
    ++pipeline;

    // Pop the pipeline replies
    for (i = 0; i < pipeline; ++i) {
        AUTO_REPLY redisReply *reply = NULL;
        redisGetReply(ctx, (void **)&reply);
        if (reply && (reply->type == REDIS_REPLY_ERROR)) {
            if (!err) {
                slurm_debug("redis error: %s", reply->str);
            }
            err = 1;
        }
    }

    // Commit or rollback the transaction
    AUTO_REPLY redisReply *reply = NULL;
    if (err) {
        slurm_debug("discarding redis transaction for job %s",
            fields.value[kJobID]);
        reply = redisCommand(ctx, "DISCARD");
    } else {
        slurm_debug("committing redis transaction for job %s",
            fields.value[kJobID]);
        reply = redisCommand(ctx, "EXEC");
    }

    return SLURM_SUCCESS;
}

List slurm_jobcomp_get_jobs(slurmdb_job_cond_t *job_cond)
{
    if (!job_cond) {
        return NULL;
    }
    if (!redis_connected()) {
        redis_connect();
    }
    if (!redis_connected()) {
        return NULL;
    }

    int i, err = 0, pipeline = 0;
    char uuid_s[37];
    char key[128];
    uuid_t uuid;
    List job_list = slurm_list_create(jobcomp_destroy_job);

    // Generate a random uuid for the query keys
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_s);

    // Start a multi-statement transaction to cover the creation of the query
    // criteria keys needed later for the match request
    redisAppendCommand(ctx, "MULTI");
    ++pipeline;

    // Create redis hash set for the job criteria
    {
        AUTO_STR char *start = jobcomp_redis_format_time(_tmf,
            job_cond->usage_start);
        AUTO_STR char *end = jobcomp_redis_format_time(_tmf,
            job_cond->usage_end);
        redisAppendCommand(ctx, "HSET %s:qry:%s %s %u %s %u "
            "%s %s %s %s %sMin %u %sMax %u", prefix, uuid_s,
            redis_field_labels[kABI], SLURM_REDIS_ABI,
            redis_field_labels[kTimeFormat], _tmf,
            redis_field_labels[kStart], start,
            redis_field_labels[kEnd], end,
            redis_field_labels[kNNodes], job_cond->nodes_min,
            redis_field_labels[kNNodes], job_cond->nodes_max);
        redisAppendCommand(ctx, "EXPIRE %s:qry:%s %u", prefix, uuid_s,
            QUERY_TTL);
        pipeline += 2;
    }

    // Create redis set for gid list
    if ((job_cond->groupid_list) && slurm_list_count(job_cond->groupid_list)) {
        memset(key, 0, sizeof(key));
        snprintf(key, sizeof(key)-1, "%s:qry:%s:gid", prefix, uuid_s);
        pipeline += redis_add_job_criteria(key, job_cond->groupid_list);
    }

    // Create redis set for job list
    if ((job_cond->step_list) && slurm_list_count(job_cond->step_list)) {
        memset(key, 0, sizeof(key));
        snprintf(key, sizeof(key)-1, "%s:qry:%s:job", prefix, uuid_s);
        pipeline += redis_add_job_steps(key, job_cond->step_list);
    }

    // Create redis set for jobname list
    if ((job_cond->jobname_list) && slurm_list_count(job_cond->jobname_list)) {
        memset(key, 0, sizeof(key));
        snprintf(key, sizeof(key)-1, "%s:qry:%s:jnm", prefix, uuid_s);
        pipeline += redis_add_job_criteria(key, job_cond->jobname_list);
    }

    // Create redis set for partition list
    if ((job_cond->partition_list) &&
            slurm_list_count(job_cond->partition_list)) {
        memset(key, 0, sizeof(key));
        snprintf(key, sizeof(key)-1, "%s:qry:%s:prt", prefix, uuid_s);
        pipeline += redis_add_job_criteria(key, job_cond->partition_list);
    }

    // Create redis set for state list
    if ((job_cond->state_list) && slurm_list_count(job_cond->state_list)) {
        memset(key, 0, sizeof(key));
        snprintf(key, sizeof(key)-1, "%s:qry:%s:stt", prefix, uuid_s);
        pipeline += redis_add_job_criteria(key, job_cond->state_list);
    }

    // Create redis set for uid list
    if ((job_cond->userid_list) && slurm_list_count(job_cond->userid_list)) {
        memset(key, 0, sizeof(key));
        snprintf(key, sizeof(key)-1, "%s:qry:%s:uid", prefix, uuid_s);
        pipeline += redis_add_job_criteria(key, job_cond->userid_list);
    }

    // Pop the pipeline replies
    for (i = 0; i < pipeline; ++i) {
        AUTO_REPLY redisReply *reply = NULL;
        redisGetReply(ctx, (void **)&reply);
        if (reply && (reply->type == REDIS_REPLY_ERROR)) {
            if (!err) {
                slurm_debug("redis error: %s", reply->str);
            }
            err = 1;
        }
    }

    // Commit or rollback the transaction
    {
        AUTO_REPLY redisReply *reply = NULL;
        if (err) {
            slurm_debug("discarding redis transaction for uuid %s", uuid_s);
            reply = redisCommand(ctx, "DISCARD");
            return job_list;
        }
        slurm_debug("committing redis transaction for uuid %s", uuid_s);
        reply = redisCommand(ctx, "EXEC");
    }

    // Use SLURMJC.MATCH to create a match set for the job criteria
    {
        int have_matches = 0;
        AUTO_REPLY redisReply *reply = redisCommand(ctx, "SLURMJC.MATCH %s %s",
            prefix, uuid_s);
        if (reply && (reply->type == REDIS_REPLY_STRING)) {
            slurm_debug("redis job matches placed in %s", reply->str);
            have_matches = reply->len;
        } else {
            slurm_debug("redis job matches not found");
        }
        if (!have_matches) {
            return job_list;
        }
    }

    // Use SLURMJC.FETCH to pull down the matching jobs in chunks and build
    // the return jobcomp list for slurm
    do {
        redis_fields_t fields; // do not AUTO_FIELDS
        AUTO_REPLY redisReply *reply = redisCommand(ctx,
            "SLURMJC.FETCH %s %s %u", prefix, uuid_s, FETCH_COUNT);
        if (!reply || (reply->type == REDIS_REPLY_NIL) ||
            (reply->type != REDIS_REPLY_ARRAY) ||
            (reply->elements == 0)) {
            break;
        }
        size_t i = 0;
        for (; i < reply->elements; ++i) {
            size_t j = 0;
            const redisReply *subreply = reply->element[i]; // do not AUTO_REPLY
            for (; j < subreply->elements; ++j) {
                if (subreply->element[j]->type == REDIS_REPLY_STRING) {
                    fields.value[j] = subreply->element[j]->str;
                } else {
                    fields.value[j] = NULL;
                }
            }
            jobcomp_job_rec_t *job = xmalloc(sizeof(jobcomp_job_rec_t));
            if (jobcomp_redis_format_job(&fields, job) != SLURM_SUCCESS) {
                jobcomp_destroy_job(job);
                continue;
            }
            slurm_list_append(job_list, job);
        }
    } while (1);

    return job_list;
}

int slurm_jobcomp_archive(__attribute__((unused)) void *arch_cond)
{
    return SLURM_SUCCESS;
}

#if SLURM_VERSION_NUMBER < SLURM_VERSION_NUM(19,5,0)
int slurm_jobcomp_get_errno(void)
{
    return SLURM_SUCCESS;
}

char *slurm_jobcomp_strerror(__attribute__((unused)) int errnum)
{
    return NULL;
}
#endif
