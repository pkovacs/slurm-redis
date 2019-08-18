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

include(CheckSymbolExists)

check_symbol_exists("assert" "assert.h" HAVE_ASSERT)
if(NOT HAVE_ASSERT)
    message(FATAL_ERROR "assert not found")
endif()

check_symbol_exists("ERANGE" "errno.h" HAVE_ERANGE)
if(NOT HAVE_ERANGE)
    message(FATAL_ERROR "ERANGE not found")
endif()

check_symbol_exists("LONG_MAX" "limits.h" HAVE_LONG_MAX)
if(NOT HAVE_LONG_MAX)
    message(FATAL_ERROR "LONG_MAX not found")
endif()

check_symbol_exists("LONG_MIN" "limits.h" HAVE_LONG_MIN)
if(NOT HAVE_LONG_MIN)
    message(FATAL_ERROR "LONG_MIN not found")
endif()

check_symbol_exists("LLONG_MAX" "limits.h" HAVE_LLONG_MAX)
if(NOT HAVE_LLONG_MAX)
    message(FATAL_ERROR "LLONG_MAX not found")
endif()

check_symbol_exists("LLONG_MIN" "limits.h" HAVE_LLONG_MIN)
if(NOT HAVE_LLONG_MIN)
    message(FATAL_ERROR "LLONG_MIN not found")
endif()

check_symbol_exists("ULONG_MAX" "limits.h" HAVE_ULONG_MAX)
if(NOT HAVE_ULONG_MAX)
    message(FATAL_ERROR "ULONG_MAX not found")
endif()

check_symbol_exists("ULLONG_MAX" "limits.h" HAVE_ULLONG_MAX)
if(NOT HAVE_ULLONG_MAX)
    message(FATAL_ERROR "ULLONG_MAX not found")
endif()

set(CMAKE_REQUIRED_INCLUDES ${REDIS_INCLUDE_DIRS})
check_symbol_exists("RedisModule_Call" "redismodule.h" HAVE_RM_CALL)
if(NOT HAVE_RM_CALL)
    message(FATAL_ERROR "RedisModule_Call not found")
endif()

check_symbol_exists("RedisModule_Calloc" "redismodule.h" HAVE_RM_CALLOC)
if(NOT HAVE_RM_CALLOC)
    message(FATAL_ERROR "RedisModule_Calloc not found")
endif()

check_symbol_exists("RedisModule_CallReplyArrayElement" "redismodule.h"
    HAVE_RM_CALLREPLYARRAYELEMENT)
if(NOT HAVE_RM_CALLREPLYARRAYELEMENT)
    message(FATAL_ERROR "RedisModule_CallReplyArrayElement not found")
endif()

check_symbol_exists("RedisModule_CallReplyLength" "redismodule.h"
    HAVE_RM_CALLREPLYLENGTH)
if(NOT HAVE_RM_CALLREPLYLENGTH)
    message(FATAL_ERROR "RedisModule_CallReplyLength not found")
endif()

check_symbol_exists("RedisModule_CallReplyType" "redismodule.h"
    HAVE_RM_CALLREPLYTYPE)
if(NOT HAVE_RM_CALLREPLYTYPE)
    message(FATAL_ERROR "RedisModule_CallReplyType not found")
endif()

check_symbol_exists("RedisModule_CloseKey" "redismodule.h"
    HAVE_RM_CLOSEKEY)
if(NOT HAVE_RM_CLOSEKEY)
    message(FATAL_ERROR "RedisModule_CloseKey not found")
endif()

check_symbol_exists("RedisModule_CreateStringPrintf" "redismodule.h"
    HAVE_RM_CREATESTRINGPRINTF)
if(NOT HAVE_RM_CREATESTRINGPRINTF)
    message(FATAL_ERROR "RedisModule_CreateStringPrintf not found")
endif()

check_symbol_exists("RedisModule_Free" "redismodule.h" HAVE_RM_FREE)
if(NOT HAVE_RM_FREE)
    message(FATAL_ERROR "RedisModule_Free not found")
endif()

check_symbol_exists("RedisModule_FreeString" "redismodule.h"
    HAVE_RM_FREESTRING)
if(NOT HAVE_RM_FREESTRING)
    message(FATAL_ERROR "RedisModule_FreeString not found")
endif()

check_symbol_exists("RedisModule_HashGet" "redismodule.h"
    HAVE_RM_HASHGET)
if(NOT HAVE_RM_HASHGET)
    message(FATAL_ERROR "RedisModule_HashGet not found")
endif()

check_symbol_exists("RedisModule_OpenKey" "redismodule.h"
    HAVE_RM_OPENKEY)
if(NOT HAVE_RM_OPENKEY)
    message(FATAL_ERROR "RedisModule_OpenKey not found")
endif()

check_symbol_exists("RedisModule_ReplySetArrayLength" "redismodule.h"
    HAVE_RM_REPLYSETARRAYLENGTH)
if(NOT HAVE_RM_REPLYSETARRAYLENGTH)
    message(FATAL_ERROR "RedisModule_ReplySetArrayLength not found")
endif()

check_symbol_exists("RedisModule_ReplyWithArray" "redismodule.h"
    HAVE_RM_REPLYWITHARRAY)
if(NOT HAVE_RM_REPLYWITHARRAY)
    message(FATAL_ERROR "RedisModule_ReplyWithArray not found")
endif()

check_symbol_exists("RedisModule_ReplyWithError" "redismodule.h"
    HAVE_RM_REPLYWITHERROR)
if(NOT HAVE_RM_REPLYWITHERROR)
    message(FATAL_ERROR "RedisModule_ReplyWithError not found")
endif()

check_symbol_exists("RedisModule_ReplyWithNull" "redismodule.h"
    HAVE_RM_REPLYWITHNULL)
if(NOT HAVE_RM_REPLYWITHNULL)
    message(FATAL_ERROR "RedisModule_ReplyWithNull not found")
endif()

check_symbol_exists("RedisModule_ReplyWithString" "redismodule.h"
    HAVE_RM_REPLYWITHSTRING)
if(NOT HAVE_RM_REPLYWITHSTRING)
    message(FATAL_ERROR "RedisModule_ReplyWithString not found")
endif()

check_symbol_exists("RedisModule_SetExpire" "redismodule.h"
    HAVE_RM_SETEXPIRE)
if(NOT HAVE_RM_SETEXPIRE)
    message(FATAL_ERROR "RedisModule_SetExpire not found")
endif()

check_symbol_exists("RedisModule_StringPtrLen" "redismodule.h"
    HAVE_RM_STRINGPTRLEN)
if(NOT HAVE_RM_STRINGPTRLEN)
    message(FATAL_ERROR "RedisModule_StringPtrLen not found")
endif()

check_symbol_exists("RedisModule_StringToLongLong" "redismodule.h"
    HAVE_RM_STRINGTOLONGLONG)
if(NOT HAVE_RM_STRINGTOLONGLONG)
    message(FATAL_ERROR "RedisModule_StringToLongLong not found")
endif()

check_symbol_exists("RedisModule_WrongArity" "redismodule.h"
    HAVE_RM_WRONGARITY)
if(NOT HAVE_RM_WRONGARITY)
    message(FATAL_ERROR "RedisModule_WrongArity not found")
endif()
unset(CMAKE_REQUIRED_INCLUDES)
