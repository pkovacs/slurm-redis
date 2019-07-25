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

# SLURM_INCLUDE_DIRS   - where to find slurm.h, etc.
# SLURM_LIBRARIES      - slurm libraries 
# SLURM_FOUND          - true if slurm found

include(FindPackageHandleStandardArgs)

find_path(SLURM_INCLUDE_DIR
    NAMES slurm.h
    PATH_SUFFIXES slurm
)

find_library(SLURM_LIBRARY
    NAMES slurm
)

find_package_handle_standard_args(
    SLURM REQUIRED_VARS SLURM_INCLUDE_DIR SLURM_LIBRARY)

if (SLURM_FOUND)
    set(SLURM_INCLUDE_DIRS ${SLURM_INCLUDE_DIR})
    set(SLURM_LIBRARIES ${SLURM_LIBRARY})
endif()
