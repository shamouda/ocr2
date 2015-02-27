/* Copyright (c) 2012, Rice University

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

   1.  Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   2.  Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following
   disclaimer in the documentation and/or other materials provided
   with the distribution.
   3.  Neither the name of Intel Corporation
   nor the names of its contributors may be used to endorse or
   promote products derived from this software without specific
   prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef HEAP_H
#define HEAP_H

#include "deque.h"

struct ocr_scheduler_struct;
typedef struct heap {
        volatile int lock;
        volatile int head;
        volatile int tail;
        buffer_t * buffer;
        long double nDescendantsSum;
} heap_t;

#define INIT_HEAP_CAPACITY 1440

void heap_init(heap_t * heap, void * init_value);

void    locked_dequeish_heap_tail_push_no_priority   ( heap_t* heap, void* entry);

void    locked_heap_push_event_priority ( heap_t* heap, void* entry);
void    locked_heap_sorted_push_event_priority ( heap_t* heap, void* entry);

void    locked_heap_push_data_priority  ( heap_t* heap, void* entry);
void    locked_heap_sorted_push_data_priority  ( heap_t* heap, void* entry);

void    locked_heap_push_user_priority  ( heap_t* heap, void* entry);
void    locked_heap_sorted_push_user_priority  ( heap_t* heap, void* entry);

void*   locked_heap_pop_priority_best       ( heap_t* heap );
void*   locked_heap_sorted_pop_priority_best       ( heap_t* heap );

void*   locked_heap_pop_priority_worst      ( heap_t* heap );
void*   locked_heap_sorted_pop_priority_worst      ( heap_t* heap );

void*   locked_heap_pop_priority_last       ( heap_t* heap );

void*   locked_heap_pop_event_priority_selfish    ( heap_t* heap , int thiefID );
void*   locked_heap_pop_data_priority_selfish    ( heap_t* heap , int thiefID );
void*   locked_heap_sorted_pop_event_priority_selfish    ( heap_t* heap , int thiefID );
void*   locked_heap_sorted_pop_data_priority_selfish    ( heap_t* heap , int thiefID );

void*   locked_dequeish_heap_tail_pop_no_priority    ( heap_t* heap );
void*   locked_dequeish_heap_head_pop_no_priority    ( heap_t* heap );

void*   locked_heap_pop_priority_worst_half   ( heap_t* heap, struct ocr_scheduler_struct* thief_base);
void*   locked_heap_sorted_pop_priority_worst_half   ( heap_t* heap, struct ocr_scheduler_struct* thief_base);

void*   locked_heap_pop_priority_worst_counting_half   ( heap_t* heap, struct ocr_scheduler_struct* thief_base);
void*   locked_heap_sorted_pop_priority_worst_counting_half   ( heap_t* heap, struct ocr_scheduler_struct* thief_base);

void*   locked_dequeish_heap_pop_head_half_no_priority ( heap_t* heap, struct ocr_scheduler_struct* thief_base);
void*   locked_dequeish_heap_pop_head_counting_half_no_priority ( heap_t* heap, struct ocr_scheduler_struct* thief_base);

#endif /* HEAP_H */

