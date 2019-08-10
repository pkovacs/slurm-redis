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

#ifndef REDIS_FIELDS_H
#define REDIS_FIELDS_H

#define MAX_REDIS_FIELDS 27

// Redis field index
enum redis_field_index {
    kJobID = 0,
    kPartition = 1,
    kStart = 2,
    kEnd = 3,
    kElapsed = 4,
    kUID = 5,
    kUser = 6,
    kGID = 7,
    kGroup = 8,
    kNNodes = 9,
    kNCPUs = 10,
    kNodeList = 11,
    kJobName = 12,
    kState = 13,
    kTimeLimit = 14,
    kBlockID = 15,
    kWorkDir = 16,
    kReservation = 17,
    kReqGRES = 18,
    kAccount = 19,
    kQOS = 20,
    kWCKey = 21,
    kCluster = 22,
    kSubmit = 23,
    kEligible = 24,
    kDerivedExitCode = 25,
    kExitCode = 26
};

// Redis field labels
extern const char *redis_field_labels[MAX_REDIS_FIELDS];

#endif /* REDIS_FIELDS_H */
