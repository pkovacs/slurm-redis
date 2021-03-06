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

add_library(slurm_jobcomp MODULE
    jobcomp_auto.c
    jobcomp_auto.h
    jobcomp_command.c
    jobcomp_command.h
    jobcomp_query.c
    jobcomp_query.h
    slurm_jobcomp.c
)

set_target_properties(slurm_jobcomp
    PROPERTIES
    OUTPUT_NAME "slurm_jobcomp"
    PREFIX ""
)

target_include_directories(slurm_jobcomp
    PRIVATE ${REDIS_INCLUDE_DIRS}
    PRIVATE ${CMAKE_SOURCE_DIR}/redis
)

target_link_libraries(slurm_jobcomp
    PRIVATE $<TARGET_OBJECTS:redis_common>
    PRIVATE $<TARGET_OBJECTS:common>
)

install(TARGETS
    slurm_jobcomp
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/slurm/redis)
