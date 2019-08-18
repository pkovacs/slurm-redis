#
# MIT License
#
# Copyright (c) 2019 Philip Kovacs
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

include(CheckFunctionExists)

check_function_exists("difftime" HAVE_DIFFTIME)
if(NOT HAVE_DIFFTIME)
    message(FATAL_ERROR "difftime not found")
endif()

check_function_exists("gmtime_r" HAVE_GMTIME_R)
if(NOT HAVE_GMTIME_R)
    message(FATAL_ERROR "gmtime_r not found")
endif()

check_function_exists("memset" HAVE_MEMSET)
if(NOT HAVE_MEMSET)
    message(FATAL_ERROR "memset not found")
endif()

check_function_exists("snprintf" HAVE_SNPRINTF)
if(NOT HAVE_SNPRINTF)
    message(FATAL_ERROR "snprintf not found")
endif()

check_function_exists("sscanf" HAVE_SSCANF)
if(NOT HAVE_SSCANF)
    message(FATAL_ERROR "sscanf not found")
endif()

check_function_exists("strcmp" HAVE_STRCMP)
if(NOT HAVE_STRCMP)
    message(FATAL_ERROR "strcmp not found")
endif()

check_function_exists("strncmp" HAVE_STRNCMP)
if(NOT HAVE_STRNCMP)
    message(FATAL_ERROR "strncmp not found")
endif()

check_function_exists("strftime" HAVE_STRFTIME)
if(NOT HAVE_STRFTIME)
    message(FATAL_ERROR "strftime not found")
endif()

check_function_exists("strtol" HAVE_STRTOL)
if(NOT HAVE_STRTOL)
    message(FATAL_ERROR "strtol not found")
endif()

check_function_exists("strtoll" HAVE_STRTOLL)
if(NOT HAVE_STRTOLL)
    message(FATAL_ERROR "strtoll not found")
endif()

check_function_exists("strtoul" HAVE_STRTOUL)
if(NOT HAVE_STRTOUL)
    message(FATAL_ERROR "strtoul not found")
endif()

check_function_exists("strtoull" HAVE_STRTOULL)
if(NOT HAVE_STRTOULL)
    message(FATAL_ERROR "strtoull not found")
endif()

check_function_exists("time" HAVE_TIME)
if(NOT HAVE_TIME)
    message(FATAL_ERROR "time not found")
endif()

check_function_exists("timegm" HAVE_TIMEGM)
if(NOT HAVE_TIMEGM)
    message(FATAL_ERROR "timegm not found")
endif()

set(CMAKE_REQUIRED_LIBRARIES Threads::Threads)
check_function_exists("pthread_rwlock_destroy" HAVE_PTHREAD_RWLOCK_DESTROY)
if(NOT HAVE_PTHREAD_RWLOCK_DESTROY)
    message(FATAL_ERROR "pthread_rwlock_destroy not found")
endif()

check_function_exists("pthread_rwlock_init" HAVE_PTHREAD_RWLOCK_INIT)
if(NOT HAVE_PTHREAD_RWLOCK_INIT)
    message(FATAL_ERROR "pthread_rwlock_init not found")
endif()

check_function_exists("pthread_rwlock_rdlock" HAVE_PTHREAD_RWLOCK_RDLOCK)
if(NOT HAVE_PTHREAD_RWLOCK_RDLOCK)
    message(FATAL_ERROR "pthread_rwlock_rdlock not found")
endif()

check_function_exists("pthread_rwlock_unlock" HAVE_PTHREAD_RWLOCK_UNLOCK)
if(NOT HAVE_PTHREAD_RWLOCK_UNLOCK)
    message(FATAL_ERROR "pthread_rwlock_unlock not found")
endif()

check_function_exists("pthread_rwlock_wrlock" HAVE_PTHREAD_RWLOCK_WRLOCK)
if(NOT HAVE_PTHREAD_RWLOCK_WRLOCK)
    message(FATAL_ERROR "pthread_rwlock_wrlock not found")
endif()

check_function_exists("pthread_rwlockattr_destroy"
    HAVE_PTHREAD_RWLOCKATTR_DESTROY)
if(NOT HAVE_PTHREAD_RWLOCKATTR_DESTROY)
    message(FATAL_ERROR "pthread_rwlockattr_destroy not found")
endif()

check_function_exists("pthread_rwlockattr_init"
    HAVE_PTHREAD_RWLOCKATTR_INIT)
if(NOT HAVE_PTHREAD_RWLOCKATTR_INIT)
    message(FATAL_ERROR "pthread_rwlockattr_init not found")
endif()

check_function_exists("pthread_rwlockattr_setkind_np"
    HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP)
if(NOT HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP)
    message(FATAL_ERROR "pthread_rwlockattr_setkind_np not found")
endif()
unset(CMAKE_REQUIRED_LIBRARIES)

set(CMAKE_REQUIRED_LIBRARIES ${UUID_LIBRARIES})
check_function_exists("uuid_generate" HAVE_UUID_GENERATE)
if(NOT HAVE_UUID_GENERATE)
    message(FATAL_ERROR "uuid_generate not found")
endif()

check_function_exists("uuid_unparse" HAVE_UUID_UNPARSE)
if(NOT HAVE_UUID_UNPARSE)
    message(FATAL_ERROR "uuid_unparse not found")
endif()
unset(CMAKE_REQUIRED_LIBRARIES)

set(CMAKE_REQUIRED_LIBRARIES ${HIREDIS_LIBRARIES})
check_function_exists("redisCommand" HAVE_REDISCOMMAND)
if(NOT HAVE_REDISCOMMAND)
    message(FATAL_ERROR "redisCommand not found")
endif()

check_function_exists("redisAppendCommand" HAVE_REDISAPPENDCOMMAND)
if(NOT HAVE_REDISAPPENDCOMMAND)
    message(FATAL_ERROR "redisAppendCommand not found")
endif()

check_function_exists("redisGetReply" HAVE_REDISGETREPLY)
if(NOT HAVE_REDISGETREPLY)
    message(FATAL_ERROR "redisGetReply not found")
endif()

check_function_exists("freeReplyObject" HAVE_FREEREPLYOBJECT)
if(NOT HAVE_FREEREPLYOBJECT)
    message(FATAL_ERROR "freeReplyObject not found")
endif()
unset(CMAKE_REQUIRED_LIBRARIES)
