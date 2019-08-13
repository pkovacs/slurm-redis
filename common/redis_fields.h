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

#define MAX_REDIS_FIELDS 28

// Redis field index
enum redis_field_index {
    kABI = 0,
    kTimeFormat = 1,
    kJobID = 2,
    kPartition = 3,
    kStart = 4,
    kEnd = 5,
    kElapsed = 6,
    kUID = 7,
    kUser = 8,
    kGID = 9,
    kGroup = 10,
    kNNodes = 11,
    kNCPUs = 12,
    kNodeList = 13,
    kJobName = 14,
    kState = 15,
    kTimeLimit = 16,
    kWorkDir = 17,
    kReservation = 18,
    kReqGRES = 19,
    kAccount = 20,
    kQOS = 21,
    kWCKey = 22,
    kCluster = 23,
    kSubmit = 24,
    kEligible = 25,
    kDerivedExitCode = 26,
    kExitCode = 27
};

// Redis field labels
extern const char *redis_field_labels[MAX_REDIS_FIELDS];

#endif /* REDIS_FIELDS_H */
