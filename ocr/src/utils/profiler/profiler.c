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

#include "profiler-internal.h"
#include "debug.h"

#ifdef OCR_RUNTIME_PROFILER


#include <stdio.h>
#include <pthread.h>

// BUG #591: Make this more platform independent
extern pthread_key_t _profilerThreadData;

/* _profiler functions */
void _profilerInit(_profiler *self, u32 event, u64 prevTicks) {
    self->accumulatorTicks = self->accumulatedChildrenTicks = self->startTicks = self->endTicks =
        self->recurseAccumulate = self->currentRecurseAccumulate = 0UL;
    self->countResume = 0;
    self->onStackCount = 1;
    self->myEvent = event;
    self->previousLastLevel = 0;
    *(u8*)(&(self->flags)) = 0;

    self->myData = (_profilerData*)pthread_getspecific(_profilerThreadData);
    if(self->myData) {
#ifdef PROFILER_FOCUS

        if(self->myData->level > 0) {
#ifdef PROFILER_COUNT_OTHER
            if(self->myData->stack[self->myData->level-1]->myEvent == EVENT_OTHER) {
                if(event != PROFILER_FOCUS) {
                    // We don't track under the "other" bucket except if it is what we are supposed to focus on
                    return;
                } else {
                    self->flags.hasAddLevel = true;
                }
            }
#endif

#ifdef PROFILER_FOCUS_DEPTH
            // We are already tracking so we just check to make sure we still
            // should be tracking
            // +1 because focus function is 0 and we want to track up to FOCUS_DEPTH
            // included
            if(self->myData->level == (PROFILER_FOCUS_DEPTH + 1 + self.flags.hasAddLevel)) {
                // We don't track
                // active flag is already false, just return
                return;
            }
#endif /* PROFILER_FOCUS_DEPTH */

#ifdef PROFILER_IGNORE_RT
            if(event != PROFILER_FOCUS && self->myData->stack[self->myData->level-1]->myEvent != PROFILER_FOCUS &&
               self->myData->stack[self->myData->level-1]->myEvent <
#ifdef PROFILER_W_APPS
               MAX_EVENTS_RT
#else
               MAX_EVENTS
#endif
               ) {
                // We already saw a call to the runtime, we return
                // Make sure we skip the FOCUS function though (which could
                // be 'userCode' which is < MAX_EVENTS_RT)
                return;
            }
#endif /* PROFILER_IGNORE_RT */

        } else if ((u32)event != (u32)PROFILER_FOCUS) {
#ifdef PROFILER_COUNT_OTHER
            // We will bucket everything else under another event
            // and count it all in one lump
            event = EVENT_OTHER;
            self->myEvent = event;
#else
            // Not the right event, we return
            return;
#endif
        }
#endif /* PROFILER_FOCUS */

        // If we are here, we need to track this
        self->flags.active = true;
        if(self->myData->level >= 1) {
            ASSERT(prevTicks != 0);
            if(self->myData->stack[self->myData->level-1]->myEvent == event) {
                ++(self->myData->stack[self->myData->level-1]->onStackCount);
                self->endTicks = prevTicks; // We cheat to keep track of this overhead
                                            // startTicks will be udpated when we return from here
                                            // and we will be able to remove startTicks - endTicks
                                            // from our "parent" (another instance of us in this case)
                                            // to remove the overhead of the init
            } else {
                // "Fake" pause; we are "pausing" to remove this overhead from the parent's total
                _profilerPause(self->myData->stack[self->myData->level-1], prevTicks); // It's a "fake" pause in the sense that we still want this time in the parent's total

                // We push ourself on the stack
                self->myData->stack[self->myData->level] = self;
                // We check if we are in a recursive chain
                self->previousLastLevel = self->myData->stackPosition[event];
                self->flags.isRecurse = (self->previousLastLevel &&
                                         self->myData->stack[self->previousLastLevel-1]->flags.active);

                ++(self->myData->level);
                ASSERT(self->myData->level < MAX_PROFILER_LEVEL);
                self->myData->stackPosition[event] = self->myData->level; // +1 taken care of above
                // _gettime will be called after we returned to remove the return overhead

            }
        } else {

            self->flags.isRecurse = false;
            // We push ourself on the stack
            self->myData->stack[self->myData->level] = self;

            ++(self->myData->level);
            ASSERT(self->myData->level < MAX_PROFILER_LEVEL);
            self->myData->stackPosition[event] = self->myData->level; // +1 taken care of above
            // _gettime will be called after we returned to remove the return overhead
        }
    }
}

_profiler* _profilerDestroy(_profiler *self, u64 end) {
    u8 removedFromStack = 0;
    if(self->flags.active && !self->flags.isPaused) {
        ASSERT(self->myData->stack[self->myData->level-1]->onStackCount >= 1);
        if(--(self->myData->stack[self->myData->level-1]->onStackCount) == 0) {
            --(self->myData->level);
            self->myData->stack[self->myData->level] = NULL;

            self->endTicks = end;
            // self->endTicks > self->startTicks
            self->accumulatorTicks += self->endTicks - self->startTicks;
            self->accumulatorTicks += self->accumulatedChildrenTicks;
            self->flags.isPaused = true;
            removedFromStack = 1;
        } else {
            // Our cheat to measure overhead of profilerInit
            // self->startTicks > self->endTicks
            // We pause the "parent" to remove the overhead of this call
            // The parent is us here (another instance of us)
            _profilerPause(self->myData->stack[self->myData->level-1], end);
            // We compute the time to "fix-up" by
            self->flags.isPaused = true;
        }
    }

    if(!self->flags.active) {
        return NULL;
    }

    // Here self->flags.active && self->flags.isPaused
    if(self->accumulatorTicks < (1+self->countResume)*self->myData->overheadTimer) {
        self->accumulatorTicks = 0;
    } else {
        self->accumulatorTicks -= (1+self->countResume)*self->myData->overheadTimer;
    }

    u64 accumulatorMs = self->accumulatorTicks/PROFILER_KHZ;
    u32 accumulatorNs = (u32)(1000000.0*((double)self->accumulatorTicks/PROFILER_KHZ - (double)accumulatorMs));

    if(removedFromStack) {
        // First the self counter
        if(0 && self->recurseAccumulate) {
            // Remove the time from our self-entry. We only do this for our own entry because
            // this is the only place that the time is counted twice (once for the execution of the child of the same
            // type and once inside the execution time of the parent).
            ASSERT(self->accumulatorTicks > self->recurseAccumulate);
            u64 t = self->accumulatorTicks - self->recurseAccumulate;
            u64 tt = t/PROFILER_KHZ;

            _profilerSelfEntryAddTime(&(self->myData->selfEvents[self->myEvent]), tt,
                                      (u32)(1000000.0*((double)t/PROFILER_KHZ - tt)));
        } else {
            _profilerSelfEntryAddTime(&(self->myData->selfEvents[self->myEvent]), accumulatorMs, accumulatorNs);
        }

        // We also update the child counter
        if(self->myData->level >= 1) {
            // We have a parent so we update there
            _profiler *parentEventPtr = self->myData->stack[self->myData->level-1];
            u32 parentEvent = parentEventPtr->myEvent;
            //ASSERT(parentEvent != self->myEvent);
            _profilerChildEntryAddTime(
                &(self->myData->childrenEvents[parentEvent][self->myEvent<parentEvent?self->myEvent:(self->myEvent-1)]),
                accumulatorMs, accumulatorNs);

            if(parentEventPtr->currentRecurseAccumulate != 0.0) {
                u64 t = parentEventPtr->currentRecurseAccumulate;
                u64 tt = t/PROFILER_KHZ;
                _profilerChildEntryAddRecurseTime(
                    &(self->myData->childrenEvents[parentEvent][self->myEvent<parentEvent?self->myEvent:(self->myEvent-1)]),
                    tt, (u32)(1000000.0*((double)t/PROFILER_KHZ - tt)));
                _profilerSwapRecurse(parentEventPtr);
            }

            // We also add the time directly to the parent as the parent has been paused to avoid counting
            // the overhead of creating the child profiling object
            _profilerAddTime(self->myData->stack[self->myData->level-1], self->accumulatorTicks);

            if(self->myData->level >= 2) {
                // We have a parent of parent
                // We add the time in the [grandParent, parent] entry to indicate that
                // the time that the parent spent in the grandparent
                // was due to a child of parent (us in this case)
                _profiler *grandParentEventPtr = self->myData->stack[self->myData->level-2];
                u32 grandParentEvent = grandParentEventPtr->myEvent;
                //ASSERT(grandParentEvent != parentEvent);
                _profilerChildEntryAddChildTime(
                    &(self->myData->childrenEvents[grandParentEvent][parentEvent<grandParentEvent?parentEvent:(parentEvent-1)]),
                    accumulatorMs, accumulatorNs);
            }
            // Deal with recursion. We are going to remove our time from the entry of our
            // ancestor that is the same event
            if(self->flags.isRecurse) {
                // We need to remove the time from our ancestor that is of the same type

                _profilerRecurseAccumulate(self->myData->stack[self->previousLastLevel-1],
                                           self->accumulatorTicks);
            }
            // Set the stack information properly for recursion
            self->myData->stackPosition[self->myEvent] = self->previousLastLevel;
            return parentEventPtr;
        } else {
            // Still set the recurse info properly (resets it)
            self->myData->stackPosition[self->myEvent] = self->previousLastLevel;
        }
    } else {
        // Not removed from stack which means that this is a collapsed call
        //ASSERT(self->myEvent == self->myData->stack[self->myData->level-1]->myEvent);
        _profilerSubTime(self->myData->stack[self->myData->level-1], self->accumulatorTicks);
        return self->myData->stack[self->myData->level-1]; // We resume our "parent" from the pause earlier in this function
                                                           // In this case, it's just us (another iteration)
    }
    return NULL;
}

void _profilerResumeInternal(_profiler *self) {
    self->myData->stack[self->myData->level] = self; // Put ourself back on the stack
    self->previousLastLevel = self->myData->stackPosition[self->myEvent];
    self->flags.isRecurse = (self->previousLastLevel &&
                             self->myData->stack[self->previousLastLevel-1]->flags.active);
    ++(self->myData->level);
    self->myData->stackPosition[self->myEvent] = self->myData->level; // +1 taken care of above
}

void _profilerPauseInternal(_profiler *self) {
    // Remove ourself from the stack
    --(self->myData->level);
    self->myData->stack[self->myData->level] = NULL;
    self->myData->stackPosition[self->myEvent] = self->previousLastLevel;

    // Here: self->endTicks > self->startTicks
    self->accumulatorTicks += self->endTicks - self->startTicks;
    self->flags.isPaused = 1;
}

// This returns ns for 1000 tries!!
#define DO_OVERHEAD_DIFF(start, end, res) \
    if(end.tv_nsec < start.tv_nsec) { \
        end.tv_nsec += 1000000000L; \
        --end.tv_sec; \
    } \
    res = (end.tv_sec-start.tv_sec)*1000000L + (end.tv_nsec - start.tv_nsec)/1000;

/* _profilerData functions */
void _profilerDataInit(_profilerData *self) {
    self->level = 0;
    self->overheadTimer = 0;
    // Calibrate overhead
    u64 start, end, temp1, temp2;
    _gettime(start);
    u32 i;
    for(i=0; i<999; ++i) {
        __asm__ __volatile__ (
            "rdtscp"
            : "=a" (temp1), "=d" (temp2)
            :
            : "ecx"
            );
    }
    _gettime(end);
    ASSERT(end > start);
    self->overheadTimer = (end - start)/1000;
    //fprintf(stderr, "Got RDTSCP overhead of %"PRIu64" ticks\n", self->overheadTimer);

    memset(&(self->selfEvents[0]), 0, sizeof(_profilerSelfEntry)*MAX_EVENTS);
    memset(&(self->childrenEvents[0][0]), 0, sizeof(_profilerChildEntry)*(MAX_EVENTS)*(MAX_EVENTS-1));
    memset(&(self->stackPosition[0]), 0, sizeof(u32)*MAX_EVENTS);
}

void _profilerDataDestroy(void* _self) {
    _profilerData *self = (_profilerData*)_self;

    // This will dump the profile and delete everything. This can be called
    // when the thread is exiting (and the TLS is destroyed)
    u32 i;
    for(i=0; i<(u32)MAX_EVENTS; ++i) {
        fprintf(self->output, "DEF %s %"PRIu32"\n", _profilerEventNames[i], i);
    }

    // Print out the entries
    for(i=0; i<(u32)MAX_EVENTS; ++i) {
        u32 j;
        for(j=0; j<(u32)MAX_EVENTS; ++j) {
            if(j == i) {
                _profilerSelfEntry *entry = &(self->selfEvents[i]);
                if(entry->count == 0) continue; // Skip entries with no content
                fprintf(self->output, "ENTRY %"PRIu32":%"PRIu32" = count(%"PRIu64"), sum(%"PRIu64".%06"PRIu32"), sumSq(%"PRIu64".%012"PRIu64")\n",
                        i, j, entry->count, entry->sumMs, entry->sumNs, entry->sumSqMs, entry->sumSqNs);
            } else {
                // Child entry
                _profilerChildEntry *entry = &(self->childrenEvents[i][j<i?j:(j-1)]);
                if(entry->count == 0) continue;
                fprintf(self->output,
                        "ENTRY %"PRIu32":%"PRIu32" = count(%"PRIu64"), sum(%"PRIu64".%06"PRIu32"), sumSq(%"PRIu64".%012"PRIu64"), sumChild(%"PRIu64".%06"PRIu32"), sumSqChild(%"PRIu64".%012"PRIu64"), sumRecurse(%"PRIu64".%06"PRIu32"), sumSqRecurse(%"PRIu64".%012"PRIu64")\n",
                        i, j, entry->count, entry->sumMs, entry->sumNs, entry->sumSqMs,
                        entry->sumSqNs, entry->sumInChildrenMs, entry->sumInChildrenNs,
                        entry->sumSqInChildrenMs, entry->sumSqInChildrenNs,
                        entry->sumRecurseMs, entry->sumRecurseNs, entry->sumSqRecurseMs,
                        entry->sumSqRecurseNs);
            }
        }
    }
    fclose(self->output);
}

#endif /* OCR_RUNTIME_PROFILER */
