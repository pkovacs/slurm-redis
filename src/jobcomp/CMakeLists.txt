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

add_library(jobcomp_redis MODULE
    jobcomp_redis.c
    jobcomp_redis_format.c
    jobcomp_redis_format.h
)

set_target_properties(jobcomp_redis
    PROPERTIES
    OUTPUT_NAME "jobcomp_redis"
    PREFIX ""
)

target_link_libraries(jobcomp_redis
    $<TARGET_OBJECTS:common>
    ${HIREDIS_LIBRARIES}
    ${SLURM_LIBRARIES}
    Threads::Threads
)

install(TARGETS
    jobcomp_redis
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/slurm)
