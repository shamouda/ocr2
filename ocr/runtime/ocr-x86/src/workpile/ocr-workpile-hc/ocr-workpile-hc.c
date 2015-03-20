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

#include "hc.h"


/******************************************************/
/* OCR-HC WorkPool                                    */
/******************************************************/

static void hc_workpile_create ( ocr_workpile_t * base, void * configuration) {
    hc_workpile* derived = (hc_workpile*) base;
    derived->deque = (deque_t *) malloc(sizeof(deque_t));
    deque_init(derived->deque, (void *) NULL_GUID);
}

static void hc_workpile_destruct ( ocr_workpile_t * base ) {
    hc_workpile* derived = (hc_workpile*) base;
    free(derived->deque->buffer->data);
    free(derived->deque->buffer);
    free(derived->deque);
    free(derived);
}

static ocrGuid_t hc_workpile_pop ( ocr_workpile_t * base ) {
    hc_workpile* derived = (hc_workpile*) base;
    return (ocrGuid_t) deque_pop(derived->deque);
}

static void hc_workpile_push (ocr_workpile_t * base, ocrGuid_t g ) {
    hc_workpile* derived = (hc_workpile*) base;
    deque_push(derived->deque, (void *)g);
}

static ocrGuid_t hc_workpile_steal ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    hc_workpile* derived = (hc_workpile*) base;
    return (ocrGuid_t) deque_steal(derived->deque);
}

ocr_workpile_t * hc_workpile_constructor(void) {
    hc_workpile* derived = (hc_workpile*) malloc(sizeof(hc_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = hc_workpile_create;
    base->destruct = hc_workpile_destruct;
    base->pop = hc_workpile_pop;
    base->push = hc_workpile_push;
    base->steal = hc_workpile_steal;
    return base;
}

static void priority_workpile_create ( ocr_workpile_t * base, void * configuration) {
    priority_workpile* derived = (priority_workpile*) base;
    derived->heap = (heap_t *) malloc(sizeof(heap_t));
    heap_init(derived->heap, (void *) NULL_GUID);
}

static void priority_workpile_destruct ( ocr_workpile_t * base ) {
    priority_workpile* derived = (priority_workpile*) base;
    free(derived->heap->buffer->data);
    free(derived->heap->buffer);
    free(derived->heap);
    free(derived);
}

static ocrGuid_t dequeish_heap_pop_tail_no_priority ( ocr_workpile_t * base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_dequeish_heap_tail_pop_no_priority (derived->heap);
}

static ocrGuid_t dequeish_heap_pop_head_no_priority ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_dequeish_heap_head_pop_no_priority (derived->heap);
}

static ocrGuid_t dequeish_heap_pop_head_half_no_priority ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_dequeish_heap_pop_head_half_no_priority (derived->heap, thief_base);
}

static ocrGuid_t dequeish_heap_pop_head_counting_half_no_priority ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_dequeish_heap_pop_head_counting_half_no_priority (derived->heap, thief_base);
}

static ocrGuid_t priority_workpile_pop_best_priority ( ocr_workpile_t * base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_heap_pop_priority_best (derived->heap);
}

static ocrGuid_t priority_workpile_sorted_pop_best_priority ( ocr_workpile_t * base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_heap_sorted_pop_priority_best (derived->heap);
}

static ocrGuid_t priority_workpile_pop_worst_priority ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_heap_pop_priority_worst (derived->heap);
}

static ocrGuid_t priority_workpile_sorted_pop_worst_priority ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_heap_sorted_pop_priority_worst (derived->heap);
}

static ocrGuid_t priority_workpile_pop_worst_half_priority ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_heap_pop_priority_worst_half (derived->heap, thief_base);
}

static ocrGuid_t priority_workpile_sorted_pop_worst_half_priority ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_heap_sorted_pop_priority_worst_half (derived->heap, thief_base);
}

static ocrGuid_t priority_workpile_pop_worst_counting_half_priority ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_heap_pop_priority_worst_counting_half (derived->heap, thief_base);
}

static ocrGuid_t priority_workpile_sorted_pop_worst_counting_half_priority ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_heap_sorted_pop_priority_worst_counting_half (derived->heap, thief_base);
}

static ocrGuid_t priority_workpile_pop_last ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_heap_pop_priority_last (derived->heap);
}

static ocrGuid_t priority_workpile_steal_event_selfish ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_heap_pop_event_priority_selfish (derived->heap, thiefID);
}

static ocrGuid_t priority_workpile_steal_data_selfish ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_heap_pop_data_priority_selfish (derived->heap, thiefID);
}

static ocrGuid_t priority_workpile_sorted_steal_event_selfish ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_heap_sorted_pop_event_priority_selfish (derived->heap, thiefID);
}

static ocrGuid_t priority_workpile_sorted_steal_data_selfish ( ocr_workpile_t * base, int thiefID, ocr_scheduler_t* thief_base ) {
    priority_workpile* derived = (priority_workpile*) base;
    return (ocrGuid_t) locked_heap_sorted_pop_data_priority_selfish (derived->heap, thiefID);
}

static void dequeish_heap_tail_push_no_priority (ocr_workpile_t * base, ocrGuid_t g ) {
    priority_workpile* derived = (priority_workpile*) base;
    locked_dequeish_heap_tail_push_no_priority (derived->heap, (void *)g);
}

static void priority_workpile_push_event_priority (ocr_workpile_t * base, ocrGuid_t g ) {
    priority_workpile* derived = (priority_workpile*) base;
    locked_heap_push_event_priority (derived->heap, (void *)g);
}

static void priority_workpile_sorted_push_event_priority (ocr_workpile_t * base, ocrGuid_t g ) {
    priority_workpile* derived = (priority_workpile*) base;
    locked_heap_sorted_push_event_priority (derived->heap, (void *)g);
}

static void priority_workpile_push_data_priority (ocr_workpile_t * base, ocrGuid_t g ) {
    priority_workpile* derived = (priority_workpile*) base;
    locked_heap_push_data_priority (derived->heap, (void *)g);
}

static void priority_workpile_sorted_push_data_priority (ocr_workpile_t * base, ocrGuid_t g ) {
    priority_workpile* derived = (priority_workpile*) base;
    locked_heap_sorted_push_data_priority (derived->heap, (void *)g);
}

static void priority_workpile_push_user_priority (ocr_workpile_t * base, ocrGuid_t g ) {
    priority_workpile* derived = (priority_workpile*) base;
    locked_heap_push_user_priority (derived->heap, (void *)g);
}

static void priority_workpile_sorted_push_user_priority (ocr_workpile_t * base, ocrGuid_t g ) {
    priority_workpile* derived = (priority_workpile*) base;
    locked_heap_sorted_push_user_priority (derived->heap, (void *)g);
}

ocr_workpile_t * dequeish_heap_steal_last_workpile_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = dequeish_heap_pop_tail_no_priority;
    base->push = dequeish_heap_tail_push_no_priority;
    base->steal = dequeish_heap_pop_head_no_priority;
    return base;
}

ocr_workpile_t * dequeish_heap_steal_half_workpile_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = dequeish_heap_pop_tail_no_priority;
    base->push = dequeish_heap_tail_push_no_priority;
    base->steal = dequeish_heap_pop_head_half_no_priority;
    return base;
}

ocr_workpile_t * dequeish_heap_counting_steal_half_workpile_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = dequeish_heap_pop_tail_no_priority;
    base->push = dequeish_heap_tail_push_no_priority;
    base->steal = dequeish_heap_pop_head_counting_half_no_priority;
    return base;
}

ocr_workpile_t * event_priority_workpile_steal_altruistic_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_pop_best_priority;
    base->push = priority_workpile_push_event_priority;
    base->steal = priority_workpile_pop_worst_priority;
    return base;
}

ocr_workpile_t * event_priority_workpile_steal_half_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_pop_best_priority;
    base->push = priority_workpile_push_event_priority;
    base->steal = priority_workpile_pop_worst_half_priority;
    return base;
}

ocr_workpile_t * event_priority_workpile_counting_steal_half_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_pop_best_priority;
    base->push = priority_workpile_push_event_priority;
    base->steal = priority_workpile_pop_worst_counting_half_priority;
    return base;
}

ocr_workpile_t * event_priority_workpile_steal_last_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_pop_best_priority;
    base->push = priority_workpile_push_event_priority;
    base->steal = priority_workpile_pop_last;
    return base;
}

ocr_workpile_t * event_priority_workpile_steal_selfish_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_pop_best_priority;
    base->push = priority_workpile_push_event_priority;
    base->steal = priority_workpile_steal_event_selfish;
    return base;
}

ocr_workpile_t * data_priority_workpile_steal_altruistic_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_pop_best_priority;
    base->push = priority_workpile_push_data_priority;
    base->steal = priority_workpile_pop_worst_priority;
    return base;
}

ocr_workpile_t * data_priority_workpile_steal_half_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_pop_best_priority;
    base->push = priority_workpile_push_data_priority;
    base->steal = priority_workpile_pop_worst_half_priority;
    return base;
}

ocr_workpile_t * data_priority_workpile_counting_steal_half_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_pop_best_priority;
    base->push = priority_workpile_push_data_priority;
    base->steal = priority_workpile_pop_worst_counting_half_priority;
    return base;
}

ocr_workpile_t * data_priority_workpile_steal_last_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_pop_best_priority;
    base->push = priority_workpile_push_data_priority;
    base->steal = priority_workpile_pop_last;
    return base;
}

ocr_workpile_t * data_priority_workpile_steal_selfish_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_pop_best_priority;
    base->push = priority_workpile_push_data_priority;
    base->steal = priority_workpile_steal_data_selfish;
    return base;
}

ocr_workpile_t * user_priority_workpile_steal_altruistic_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_pop_best_priority;
    base->push = priority_workpile_push_user_priority;
    base->steal = priority_workpile_pop_worst_priority;
    return base;
}

ocr_workpile_t * user_priority_workpile_steal_half_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_pop_best_priority;
    base->push = priority_workpile_push_user_priority;
    base->steal = priority_workpile_pop_worst_half_priority;
    return base;
}

ocr_workpile_t * user_priority_workpile_counting_steal_half_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_pop_best_priority;
    base->push = priority_workpile_push_user_priority;
    base->steal = priority_workpile_pop_worst_counting_half_priority;
    return base;
}

ocr_workpile_t * user_priority_workpile_steal_last_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_pop_best_priority;
    base->push = priority_workpile_push_user_priority;
    base->steal = priority_workpile_pop_last;
    return base;
}

ocr_workpile_t * event_sorted_priority_workpile_steal_altruistic_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_sorted_pop_best_priority;
    base->push = priority_workpile_sorted_push_event_priority;
    base->steal = priority_workpile_sorted_pop_worst_priority;
    return base;
}

ocr_workpile_t * event_sorted_priority_workpile_steal_half_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_sorted_pop_best_priority;
    base->push = priority_workpile_sorted_push_event_priority;
    base->steal = priority_workpile_sorted_pop_worst_half_priority;
    return base;
}

ocr_workpile_t * event_sorted_priority_workpile_counting_steal_half_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_sorted_pop_best_priority;
    base->push = priority_workpile_sorted_push_event_priority;
    base->steal = priority_workpile_sorted_pop_worst_counting_half_priority;
    return base;
}

ocr_workpile_t * event_sorted_priority_workpile_steal_selfish_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_sorted_pop_best_priority;
    base->push = priority_workpile_sorted_push_event_priority;
    base->steal = priority_workpile_sorted_steal_event_selfish;
    return base;
}

ocr_workpile_t * data_sorted_priority_workpile_steal_altruistic_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_sorted_pop_best_priority;
    base->push = priority_workpile_sorted_push_data_priority;
    base->steal = priority_workpile_sorted_pop_worst_priority;
    return base;
}

ocr_workpile_t * data_sorted_priority_workpile_steal_half_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_sorted_pop_best_priority;
    base->push = priority_workpile_sorted_push_data_priority;
    base->steal = priority_workpile_sorted_pop_worst_half_priority;
    return base;
}

ocr_workpile_t * data_sorted_priority_workpile_counting_steal_half_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_sorted_pop_best_priority;
    base->push = priority_workpile_sorted_push_data_priority;
    base->steal = priority_workpile_sorted_pop_worst_counting_half_priority;
    return base;
}

ocr_workpile_t * data_sorted_priority_workpile_steal_selfish_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_sorted_pop_best_priority;
    base->push = priority_workpile_sorted_push_data_priority;
    base->steal = priority_workpile_sorted_steal_data_selfish;
    return base;
}

ocr_workpile_t * user_sorted_priority_workpile_steal_altruistic_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_sorted_pop_best_priority;
    base->push = priority_workpile_sorted_push_user_priority;
    base->steal = priority_workpile_sorted_pop_worst_priority;
    return base;
}

ocr_workpile_t * user_sorted_priority_workpile_steal_half_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_sorted_pop_best_priority;
    base->push = priority_workpile_sorted_push_user_priority;
    base->steal = priority_workpile_sorted_pop_worst_half_priority;
    return base;
}

ocr_workpile_t * user_sorted_priority_workpile_counting_steal_half_constructor(void) {
    priority_workpile* derived = (priority_workpile*) malloc(sizeof(priority_workpile));
    ocr_workpile_t * base = (ocr_workpile_t *) derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = NULL;
    base->create = priority_workpile_create;
    base->destruct = priority_workpile_destruct;
    base->pop = priority_workpile_sorted_pop_best_priority;
    base->push = priority_workpile_sorted_push_user_priority;
    base->steal = priority_workpile_sorted_pop_worst_counting_half_priority;
    return base;
}
