## Slurm Job Completion Plugin for Redis <!-- omit in toc -->

- [Purpose and Design Goals](#purpose-and-design-goals)
- [Requirements](#requirements)
- [Plugin Configuration](#plugin-configuration)
  - [Basic configuration](#basic-configuration)
  - [Advanced configuration](#advanced-configuration)
- [Slurm Configuration](#slurm-configuration)
- [Redis Configuration](#redis-configuration)
  - [Module Loading](#module-loading)
  - [Port Binding](#port-binding)
  - [Authentication](#authentication)
  - [Persistence](#persistence)
  - [System Config](#system-config)
- [Usage](#usage)
- [FAQ](#faq)

### Purpose and Design Goals

I wanted a lightweight and _fast_ job completion plugin for slurm that has fairly good support for client-side filtering of job completion criteria.  Redis fits this need very nicely because it is memory-based and very fast indeed.  This jobcomp_redis plugin can  produce permanent redis keys or, using redis key expiry, it can produce keys which live only for as long as you configure.  This makes the jobcomp_redis plugin a good complement to accounting storage plugins, e.g. mysql/mariadb.  For example, you could configure the jobcomp_redis plugin so that keys live for only a week, thus implementing a super-fast, memory-based cache of a rolling week's worth of jobs.  If you save the keys permanently (the default), you can configure redis persistence to suite your needs.

In terms of design, the jobcomp_redis slurm plugin works with a partner plugin that I also wrote for this project, slurm_jobcomp, which is loaded into redis and implements specialized commands that are invoked by the slurm-side plugin when jobs complete or when clients such as `sacct` request job data.  This provides nice separation of concerns and minimizes network traffic.  To elaborate on that: the slurm-side plugin issues the custom redis command `SLURMJC.INDEX` after it sends job data to redis, but the indexing scheme itself is completely opaque to slurm and fully the responsiblility of the redis-side partner.

When job data is requested from slurm, jobcomp_redis sends job _criteria_ to redis and then issues the command `SLURMJC.MATCH` to ask redis to perform the job matching.  In this way, we avoid pulling job candidates across the wire just to test if they match which can waste network bandwidth and slow us down.  If matches are found, the slurm-side partner will issue `SLURMJC.FETCH` to receive the job data from redis.

Let me know if you find this plugin useful.  More plugins to follow ...

### Requirements

To configure, build and run this package from source you will need the following:
- a working slurm installation with slurm libraries (including `libslurm.so` symlink) in your library search path
- a *_configured_* slurm source tree (`slurm/slurm.h` exists) that *_matches_* your slurm installation
- `gcc` and `pkg-config` which you already have if you've built slurm successfully
- [cmake](https://cmake.org/), a relatively recent version (3.4.0+)
- [redis](https://redis.io/), including its `redismodule.h` development header
- [hiredis](https://github.com/redis/hiredis), the c client for redis, headers and library
- `libuuid`, its header (uuid/uuid.h) and library `libuuid.so`, (available in utils-linux)


### Plugin Configuration

#### Basic configuration

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
$ tar -xjf slurm-redis-0.1.0-Source.tar.bz2
$ cd slurm-redis-0.1.0-Source
$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INCLUDE_PATH=/home/phil/slurm-19.05.2/ ..
$ make
$ sudo make install
# restart slurmctld and redis if they are running with previous versions
# of their respective plugins
```

#### Advanced configuration

### Slurm Configuration

### Redis Configuration

#### Module Loading

#### Port Binding

#### Authentication

#### Persistence

#### System Config

### Usage

```bash
# Show my recently completed jobs
$ sacct -c

# Show my jobs in long form
$ sacct -cl
```

### FAQ
