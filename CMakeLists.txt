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

cmake_policy(VERSION 3.4.0)
cmake_minimum_required(VERSION 3.4.0 FATAL_ERROR)

project(slurm-redis
    VERSION 0.1.2
    DESCRIPTION "Redis plugins for slurm"
    HOMEPAGE_URL "https://github.com/pkovacs/slurm-redis"
    LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(GNUInstallDirs)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# ------------
# Local checks
# ------------
include(FindPackages)
include(FindHeaders)
include(FindFunctions)
include(FindSymbols)
include(FindLibraries)
include(TestCompiler)

# ---------------
# Set ABI version
# ---------------
set(SLURM_REDIS_ABI 0)

# -----------------
# Set compile flags
# -----------------
string(CONCAT LOCAL_C_FLAGS
    "-Wunused-result -pipe -Wall -Wextra ")
string(CONCAT LOCAL_C_FLAGS_DEBUG
    "-Wformat -Wformat-overflow -Wformat-security -Wformat-truncation ")
string(CONCAT LOCAL_C_FLAGS_RELEASE
    "-Werror=format -Werror=format-overflow -Werror=format-security "
    "-Werror=format-truncation "
    "-Wp,-D_FORTIFY_SOURCE=2 -fstack-protector-strong ")
list(APPEND CMAKE_C_FLAGS ${LOCAL_C_FLAGS})
set(CMAKE_C_FLAGS_DEBUG "-g ${LOCAL_C_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_RELEASE "-O2 ${LOCAL_C_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g ${LOCAL_C_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os ${LOCAL_C_FLAGS_RELEASE}")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(NOT JCR_CACHE_SIZE)
    set(JCR_CACHE_SIZE "128")
endif()

if(NOT JCR_CACHE_TTL)
    set(JCR_CACHE_TTL "120")
endif()

if(NOT JCR_FETCH_COUNT)
    set(JCR_FETCH_COUNT "500")
endif()

if(NOT JCR_FETCH_LIMIT)
    set(JCR_FETCH_LIMIT "1000")
endif()

if(NOT JCR_QUERY_TTL)
    set(JCR_QUERY_TTL "60")
endif()

if(NOT JCR_TTL)
    set(JCR_TTL "-1")
endif()

if(NOT JCR_TMF)
    set(JCR_TMF "1")
endif()

# --------------
# Build config.h
# --------------
configure_file(
    ${CMAKE_SOURCE_DIR}/config.h.in
    ${CMAKE_BINARY_DIR}/config.h
)
add_definitions(-DHAVE_CONFIG_H)
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_definitions(-DDEBUG)
endif()

# ---------------
# Global Includes
# ---------------
include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}
)

# ------------------
# Add subdirectories
# ------------------
add_subdirectory(common)
add_subdirectory(redis)
add_subdirectory(slurm)

# ----------------------
# Generate a slurm patch
# ----------------------
configure_file(
    ${CMAKE_SOURCE_DIR}/patch/x_ac_slurm_redis.m4.in
    ${CMAKE_BINARY_DIR}/patch/x_ac_slurm_redis.m4
)
add_custom_target(patch ${CMAKE_SOURCE_DIR}/generate_patch
    ${PROJECT_NAME}-${PROJECT_VERSION} ${CMAKE_INCLUDE_PATH}
    ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
    DEPENDS ${CMAKE_BINARY_DIR}/patch/x_ac_slurm_redis.m4
)

# -----------------
# Add CPack support
# -----------------
set(CPACK_GENERATOR "RPM")
set(CPACK_PACKAGE_NAME ${CMAKE_PROJECT_NAME})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION})
set(CPACK_PACKAGE_CHECKSUM "SHA1")
set(CPACK_SOURCE_GENERATOR "TGZ;TBZ2")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME
    ${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION})
set(CPACK_SOURCE_IGNORE_FILES "/build/;/.git*;/.*.swp")
include(CPack)
