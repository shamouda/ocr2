/* Modified in 2014 by Romain Cledat (now at Intel). The original
 * license (BSD) is below. This file is also subject to the license
 * aggrement located in the file LICENSE and cannot be distributed
 * without it. This notice cannot be removed or modified
 */

/* Copyright (c) 2011, Romain Cledat
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Georgia Institute of Technology nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL ROMAIN CLEDAT BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __OCR_RUNTIME_PROFILER_INTERNAL_H__
#define __OCR_RUNTIME_PROFILER_INTERNAL_H__

#include "ocr-config.h"
#ifdef OCR_RUNTIME_PROFILER
#include "ocr-types.h"

#ifndef ENABLE_COMP_PLATFORM_PTHREAD
#error "The current runtime profiler is only compatible with the pthread comp-platform at this time"
#endif

#include <stdio.h>

#include "profiler.h"
#ifdef PROFILER_W_APPS
#include "profilerAutoGenApp.h"
#endif

#ifndef MAX_PROFILER_LEVEL
#define MAX_PROFILER_LEVEL 512
#endif

#ifndef PROFILER_KHZ
#define PROFILER_KHZ 3400000
#endif

//timeMs = a/PROFILER_KHZ;
//timeNs = (unsigned int)(1000000.0*((double)a/PROFILER_KHZ - (double)timeMs));

typedef struct __profilerChildEntry {
    u64 count;
    u64 sumMs, sumSqMs, sumInChildrenMs, sumSqInChildrenMs,
        sumRecurseMs, sumSqRecurseMs;
    u64 sumSqNs, sumSqInChildrenNs, sumSqRecurseNs; // 64 bits because needs to contain 1e12
    u32 sumNs, sumInChildrenNs, sumRecurseNs;
} _profilerChildEntry;

typedef struct __profilerSelfEntry {
    u64 count;
    u64 sumMs, sumSqMs, sumSqNs;
    u32 sumNs;
} _profilerSelfEntry;


typedef struct __profilerData {
    _profiler* stack[MAX_PROFILER_LEVEL];
    FILE *output;
    u64 overheadTimer;
    u32 level;                  /**< Current level in the profiler */

    _profilerSelfEntry selfEvents[MAX_EVENTS];
    _profilerChildEntry childrenEvents[MAX_EVENTS][MAX_EVENTS-1]; // We already have the self entry
    u32 stackPosition[MAX_EVENTS]; // Contains either 0 or the level at which the most recent
                                   // entry for the event is made (+1: level 0 is encoded as 1)
} _profilerData;

/* Non-inline profilerData functions */
void _profilerDataInit(_profilerData *self);
void _profilerDataDestroy(void * self);

/* _profilerChildEntry functions */
static inline void _profilerChildEntryReset(_profilerChildEntry *self) {
    self->count = self->sumMs = self->sumSqMs = self->sumInChildrenMs =
        self->sumSqInChildrenMs = self->sumRecurseMs = self->sumSqRecurseMs =
        self->sumSqNs = self->sumSqInChildrenNs = self->sumSqRecurseNs = 0UL;
    self->sumNs = self->sumInChildrenNs = self->sumRecurseNs = 0;
}

static inline void _profilerChildEntryAddTime(_profilerChildEntry *self,
                                              u32 timeMs, u32 timeNs) {
    ++self->count;
    self->sumNs += timeNs;
    if(self->sumNs >= 1000000) {
        self->sumNs -= 1000000;
        ++self->sumMs;
    }
    self->sumMs += timeMs;

    self->sumSqNs += timeNs*timeNs;
    while(self->sumSqNs >= 1000000000000UL) {
        self->sumSqNs -= 1000000000000UL;
        ++self->sumSqMs;
    }
    self->sumSqMs += timeMs*timeMs;
}

static inline void _profilerChildEntryAddChildTime(_profilerChildEntry *self,
                                                   u32 timeMs, u32 timeNs) {
    self->sumInChildrenNs += timeNs;
    if(self->sumInChildrenNs >= 1000000) {
        self->sumInChildrenNs -= 1000000;
        ++self->sumInChildrenMs;
    }

    self->sumInChildrenMs += timeMs;

    self->sumSqInChildrenNs += timeNs*timeNs;
    while(self->sumSqInChildrenNs >= 1000000000000UL) {
        self->sumSqInChildrenNs -= 1000000000000UL;
        ++self->sumSqInChildrenMs;
    }
    self->sumSqInChildrenMs += timeMs*timeMs;
}

static inline void _profilerChildEntryAddRecurseTime(_profilerChildEntry *self,
                                                     u32 timeMs, u32 timeNs) {
    self->sumRecurseNs += timeNs;
    if(self->sumRecurseNs >= 1000000) {
        self->sumRecurseNs -= 1000000;
        ++self->sumRecurseMs;
    }

    self->sumRecurseMs += timeMs;

    self->sumSqRecurseNs += timeNs*timeNs;
    while(self->sumSqRecurseNs >= 1000000000000UL) {
        self->sumSqRecurseNs -= 1000000000000UL;
        ++self->sumSqRecurseMs;
    }
    self->sumSqRecurseMs += timeMs*timeMs;
}

/* _profilerSelfEntry functions */
static inline void _profilerSelfEntryReset(_profilerSelfEntry *self) {
    self->count = self->sumMs = self->sumSqMs = self->sumSqNs = 0UL;
    self->sumNs = 0;
}

static inline void _profilerSelfEntryAddTime(_profilerSelfEntry *self,
                                             u64 timeMs, u32 timeNs) {
    ++self->count;

    self->sumNs += timeNs;
    if(self->sumNs >= 1000000) {
        self->sumNs -= 1000000;
        ++self->sumMs;
    }
    self->sumMs += timeMs;

    self->sumSqNs += timeNs*timeNs;
    while(self->sumSqNs >= 1000000000000UL) {
        self->sumSqNs -= 1000000000000UL;
        ++self->sumSqMs;
    }
    self->sumSqMs += timeMs*timeMs;
}

static inline void _profilerSubTime(_profiler *self, u64 ticks) __attribute__((always_inline));
static inline void _profilerSubTime(_profiler *self, u64 ticks) {
    self->accumulatorTicks -= ticks;
}

static inline void _profilerRecurseAccumulate(_profiler *self, u64 ticks) __attribute__((always_inline));
static inline void _profilerRecurseAccumulate(_profiler *self, u64 ticks) {
    self->currentRecurseAccumulate += ticks;
}

static inline void _profilerSwapRecurse(_profiler *self) __attribute__((always_inline));
static inline void _profilerSwapRecurse(_profiler *self) {
    self->recurseAccumulate += self->currentRecurseAccumulate;
    self->currentRecurseAccumulate = 0.0;
}

static inline void _profilerAddTime(_profiler *self, u64 ticks) __attribute__((always_inline));
static inline void _profilerAddTime(_profiler *self, u64 ticks) {
    self->accumulatedChildrenTicks += ticks;
}

#endif
#endif
