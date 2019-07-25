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

#ifndef JOBCOMP_TYPE_H
#define JOBCOMP_TYPE_H

#include <stdint.h>
#include <time.h>

#include <redismodule.h>

#define JOBCOMP_TYPE_ARITY 26
#define JOBCOMP_TYPE_NAME "SLURMJC--"
#define JOBCOMP_TYPE_ENCODING_VERSION 0

extern RedisModuleType *jobcomp_redis_t;

/* 
 * The slurm jobcomp plugins each support a slightly different set of job
 * completion fields, so I've chosen the fields below as a starting point.
 */
typedef struct {
    uint32_t jobid;
    uint32_t uid;
    uint32_t gid;
    uint32_t state;
    uint32_t node_cnt;
    uint32_t proc_cnt;
    uint32_t maxprocs;
    uint32_t time_limit;
    time_t start_time;
    time_t end_time;
    time_t submit_time;
    time_t eligible_time;
    char *account;
    char *cluster;
    char *user_name;
    char *group_name;
    char *job_name;
    char *partition;
    char *node_list;
    char *qos_name;
    char *resv_name;
    char *req_gres;
    char *wckey;
    char *derived_ec;
    char *exit_code;
    char *work_dir;
} jobcomp_t;

void *jobcomp_type_load(RedisModuleIO *rdb, int encver);
void jobcomp_type_save(RedisModuleIO *rdb, void *value);
void jobcomp_type_rewrite(RedisModuleIO *aof, RedisModuleString *key, void *value);
void jobcomp_type_free(void *value);

#endif /* JOBCOMP_TYPE_H */
