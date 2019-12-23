## Slurm Job Completion Plugin for Redis <!-- omit in toc -->

- [Purpose and Design](#purpose-and-design)
- [Requirements](#requirements)
  - [Build slurm-redis using provided slurm patch set](#build-slurm-redis-using-provided-slurm-patch-set)
  - [Build slurm-redis using native build system](#build-slurm-redis-using-native-build-system)
  - [Advanced configuration](#advanced-configuration)
- [Slurm Configuration](#slurm-configuration)
- [Redis Configuration](#redis-configuration)
  - [Redis System Config](#redis-system-config)
- [Usage](#usage)
- [FAQ](#faq)

___

### Purpose and Design

I wanted a fast, lightweight job completion plugin for slurm that has good support for client-side filtering of job completion criteria.  Redis fits this need very nicely because it is memory-based and very fast indeed.  This jobcomp_redis plugin can  produce permanent redis keys or, using redis key expiry, it can produce keys which live only for a duration that you configure.  The jobcomp_redis plugin can be a good complement to accounting storage plugins, e.g. mysql/mariadb.  For example, you could configure the jobcomp_redis plugin so that keys live only for a week, thus implementing a super-fast, memory-based cache of a rolling week's worth of jobs.  If you save the keys permanently (the default), you can configure redis persistence to suit your needs, write scripts to manage your redis job data, etc.

In terms of design, the jobcomp_redis slurm plugin works with a partner plugin that I also wrote for this project, slurm_jobcomp, which is loaded into redis and implements specialized commands that are invoked by the slurm-side plugin when jobs complete or when clients such as `sacct` request job data.  This provides nice separation of concerns and minimizes network traffic.  To elaborate on that: the slurm-side plugin issues the custom redis command `SLURMJC.INDEX` after it sends job data to redis, but the indexing scheme itself is completely opaque to slurm and fully the responsiblility of the redis-side partner.

When job data is requested from slurm, jobcomp_redis sends job criteria to redis and then issues the command `SLURMJC.MATCH` to ask redis to perform the job matching.  In this way, we avoid pulling job candidates across the wire just to test if they match which can waste network bandwidth and slow us down.  If matches are found, the slurm-side partner will issue `SLURMJC.FETCH` to receive the job data from redis.

Let me know if you find this plugin useful.  More plugins to follow ...
___

### Requirements

Two methods are provided for building this package from source:
1. A patch set that applies patches directly over the slurm source tree and
2. The slurm-redis native build system which uses [cmake](https://cmake.org/)

The patch set method is perhaps easier to use as it does not require the cmake build system. The plugins were developed with cmake, however, and using cmake allows you to configure and build the slurm-redis plugins separately from slurm source tree.  All compile-time options are available in either case.

These are the additional software requirements to run slurm-redis:

- [redis](https://redis.io/), including its `redismodule.h` development header
- [hiredis](https://github.com/redis/hiredis), the c client for redis, headers and library
- `libuuid`, its header (uuid/uuid.h) and library `libuuid.so`, (available in utils-linux)
___

#### Build slurm-redis using provided slurm patch set

The patch sets are named according to the version of slurm-redis and the version of slurm to which they apply, for example:

`slurm-redis-0.1.3-slurm-19.05.patch.bz2` would be the patch you could use for slurm-redis version 0.1.3 that patches over the slurm 19.05 source tree.  Here's an example of using the patch set:

```bash
# Unpack both tarballs and apply the patch set
$ tar -xjf slurm-19.05.5.tar.bz2
$ bunzip2 slurm-redis-0.1.3-slurm-19.05.patch.bz2
$ cd slurm-19.05.5
$ patch -p1 < ../slurm-redis-0.1.3-slurm-19.05.patch
# patching file configure.ac
# ...
# Re-run autoreconf to apply build system changes
./autoreconf
# Configure slurm as you normally would, noting these additional options:
./configure --help # See section "Advanced configuration"
#  ...
#  --with-jcr-cache-size=N set jobcomp/redis cache size [128]
#  --with-jcr-cache-ttl=N  set jobcomp/redis cache ttl [120]
#  --with-jcr-fetch-count=N
#                          set jobcomp/redis fetch count [500]
#  --with-jcr-fetch-limit=N
#                          set jobcomp/redis fetch limit [1000]
#  --with-jcr-query-ttl=N  set jobcomp/redis query ttl [60]
#  --with-jcr-ttl=N        set jobcomp/redis ttl: -1=permanent [-1]
#  --with-jcr-tmf=N        set jobcomp/redis date/time format: 0=unix epoch,
#                          1=iso8601 [1]
#  ...
```
___

#### Build slurm-redis using native build system

```bash
# CMAKE_INSTALL_PREFIX must be set to the system's library installation prefix,
#       e.g. /usr or /usr/local
#   - set this to the same prefix as your slurm installation so that the
#     jobcomp_redis.so plugin installs alongside the other slurm plugins.
#   - hint: /usr installs to /usr/lib64/slurm
#   - hint: /usr/local installs to /usr/local/lib64/slurm
#
# CMAKE_INCLUDE_PATH must be set to a configured slurm source tree,
#       matching your current slurm installation.
#   - hint: a configured source tree is one in which slurm/slurm.h exists.
#

$ tar -xjf slurm-redis-0.1.3.tar.bz2
$ cd slurm-redis-0.1.3
$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INCLUDE_PATH=/home/phil/slurm-19.05.5/ ..
$ make
$ sudo make install
```

After installation, restart `slurmctld` if it was running with a previous `jobcomp_redis.so` loaded. You do not have to restart redis, however, in order to load a newer version of the `slurm_jobcomp.so` plugin, in fact, keys can be lost if you restart redis in between its persistence cycles.  Instead, simply open a redis cli and manually unload the current module, then load the new module (or write a script to do this):

```bash
redis-cli -h <host>
redis-cli> auth <password>
OK
redis-cli> module unload slurm_jobcomp
OK
redis-cli> module load /usr/lib64/slurm/redis/slurm_jobcomp.so
OK
```
___

#### Advanced configuration

```bash
$ cmake -DJCR_TMF=N ... # [0 = unix epoch times, 1 = ISO 8601 format] or
$ ./configure --with-jcr-tmf=N ...
# The default is 1 (ISO8601).

# This setting will cause the slurm jobcomp_redis plugin to send all date/time elements
# either as ISO8601 strings, GMT with timezone "Z" (Zero/Zulu), or as Unix Epoch times.

$ cmake -DJCR_TTL=N ... # [-1 or a positive integer (seconds)] or
$ ./configure --with-jcr-ttl=N ...
# The default is -1: keys are permanent.

# This setting will set the time-to-live in seconds of your job completion data.  If you
# use the value 86400, for example, your job keys will disappear after 1 day.

$ cmake -DJCR_QUERY_TTL=N ... # or
$ ./configure --with-jcr-query-ttl=N ...
# The default is 60 seconds.

# This setting should not need to be changed.  When clients such as saact request job
# data, the jobcomp_redis plugin sends the job criteria to redis as a set of transient
# keys and then issues SLURMJC.MATCH.  The latency between the time that the criteria
# arrives in redis and the command SLURMJC.FETCH completes in redis is where this setting
# matters.

$ cmake -DJCR_FETCH_LIMIT=N ... # or
$ ./configure --with-jcr-fetch-limit=N ...
# The default is 1000 job records.

# The maximum number of jobs that redis will allow to be sent to the client in one
# iteration of SLURMJC.FETCH.

$ cmake -DJCR_FETCH_COUNT=N ... # or
$ ./configure --with-jcr-fetch-count=N ...
# The default is 500 job records.

# The maximum number of jobs records that the client would like to receive in one
# iteration of SLURMJC.FETCH.

$ cmake -DJCR_CACHE_SIZE=N ... # or
$ ./configure --with-jcr-cache-size=N
# The default is 128 entries (there are separate uid and gid caches).

# As job records complete the jobcomp/redis plugin maintains small caches of uid and gid
# to name, e.g. 0 -> root, to take pressure off distributed LDAP and similar systems.

$ cmake -DJCR_CACHE_TTL=N ... # or
$ ./configure --with-jcr-cache-ttl=N
# The default is 120 seconds.

# The time-to-live of the uid and gid cache entries.  If a cache entry is missing or has
# expired, slurm apis are called to fetch the names.
```
___

### Slurm Configuration

```bash
# /etc/slurm/slurm.conf

JobCompHost=<redis listen ip>
JobCompLoc=<an optional, (short!) prefix to prepend to your redis keys>
JobCompPass=<redis password, if redis configured for password authentication>
JobCompPort=<redis listen port, e.g. 6379>
JobCompType=jobcomp/redis
#JobCompUser=<unused, redis has no notion of user>
```

___

### Redis Configuration

```bash
# /etc/redis.conf

# Add a directive to load the slurm_jobcomp.so plugin
loadmodule /usr/lib64/slurm/redis/slurm_jobcomp.so

# Change the default listen address from 127.0.0.1 to an address reachable
# by the slurm job controller slurmctld and sacct clients
#bind 127.0.0.1
bind <ip address> 127.0.0.1

# if you want to use redis authentication, set a password and make sure
# to add the password to slurm's JobCompPass config key
requirepass <password>

# Pay some attention to redis persistence settings and possibly adjust those
# as needed
```

#### Redis System Config

After you start redis, check its logs, `/var/log/redis/redis.log`, for any
complaints it may have about your system configuration.  In particular, redis
does not like to operate with transparent huge pages activated.  There are a 
number of ways to turn that off:

- `cat /sys/kernel/mm/transparent_hugepage/enabled` to check your system
- add `transparent_hugepage=never` to your kernel boot comand (`cat /proc/cmdline`)
- `echo never > /sys/kernel/mm/redhat_transparent_hugepage/enabled` at system start

There may be some other system settings, e.g. overcommit_memory, that you need to adjust using `/etc/sysctl.conf`.  Refer to the redis log file for more details.

___

### Usage

```bash
# sacct filtering switches that work with jobcomp_redis:

  --start=     // start time
  --end=       // end time
  --uid=       // uid list
  --user=      // user list
  --gid=       // gid list
  --group=     // group list
  --nnodes=    // number of nodes min/max
  --state=     // job completion state (CD=COMPLETED, F=FAILED, etc.)
  --partition= // partition list
  --job=       // job id list
  --name=      // job name list

# NOTE: pay attention to slurm's DEFAULT TIME WINDOW (see: man sacct) since some
# combinations of the --job and --state switches can yield unintuitive time ranges
# and thus require you to use -S and -E to get the time range you really want.

# Show my recently completed jobs
$ sacct -cl

# Show my jobs that failed this morning
$ sacct -cl -S09:00 -E12:00 --status=FAILED

# Show my jobs that ran successfully on at least 10 nodes
$ sacct -cl --state=CD --nnodes=10

# Show my jobs that ran successfully on partition alpha and used no more than 5 nodes
$ sacct -cl --state=CD --nnodes=0-5 --partition=alpha

# Show the completion status of my jobs named spatial and temporal
$ sacct -cl --name=spatial,temporal

# Show the completion status of jobs 2142, 2143 and 2144
$ sacct -cl --jobs=2142,2143,2144
```

### FAQ
