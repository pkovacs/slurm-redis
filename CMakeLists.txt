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

project(slurm-redis C)
cmake_minimum_required(VERSION 3.4.0 FATAL_ERROR)

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

# -------------------
# Set project version
# -------------------
set(SLURM_REDIS_VERSION_MAJOR 0)
set(SLURM_REDIS_VERSION_MINOR 1)
set(SLURM_REDIS_VERSION_PATCH 0)
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
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

if(NOT SLURM_SOURCE_DIR)
    message(FATAL_ERROR "SLURM_SOURCE_DIR not set")
endif()

if(NOT EXISTS ${SLURM_SOURCE_DIR})
    message(FATAL_ERROR "Slurm source dir (${SLURM_SOURCE_DIR}) not found")
endif()
message(STATUS "Found Slurm source dir at ${SLURM_SOURCE_DIR}")

option(ISO8601_DATES "Store ISO8601 dates in redis" ON)

if(NOT QUERY_KEY_TTL)
    set(QUERY_KEY_TTL "300")
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

# -----------------
# Add CPack support
# -----------------
set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_NAME ${CMAKE_PROJECT_NAME})
set(CPACK_PACKAGE_VERSION_MAJOR ${SLURM_REDIS_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${SLURM_REDIS_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${SLURM_REDIS_VERSION_PATCH})
string(CONCAT CPACK_PACKAGE_VERSION
    "${SLURM_REDIS_VERSION_MAJOR}."
    "${SLURM_REDIS_VERSION_MINOR}."
    "${SLURM_REDIS_VERSION_PATCH}")
set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION})
include(CPack)
