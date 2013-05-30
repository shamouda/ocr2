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

#ifndef __OCR_WORKPILE_H_
#define __OCR_WORKPILE_H_

#include "ocr-guid.h"
#include "ocr-runtime-def.h"


/****************************************************/
/* OCR WORKPILE FACTORY                             */
/****************************************************/

// Forward declaration
struct ocrWorkpile_t;

typedef struct ocrWorkpileFactory_t {
    struct ocrWorkpile_t * (*instantiate) ( struct ocrWorkpileFactory_t * factory, void * per_type_configuration, void * per_instance_configuration);
    void (*destruct)(struct ocrWorkpileFactory_t * factory);
} ocrWorkpileFactory_t;


/****************************************************/
/* OCR WORKPILE API                                 */
/****************************************************/

/*! \brief Abstract class to represent OCR task pool data structures.
 *
 *  This class provides the interface for the underlying implementation to conform.
 *  As we want to support work stealing, we current have pop, push and steal interfaces
 */
//TODO We may be influenced by how STL resolves this issue as in push_back, push_front, pop_back, pop_front
typedef struct ocrWorkpile_t {
    ocr_module_t module;
    /*! \brief Virtual destructor for the WorkPool interface
     *  As this class does not have any state, the virtual destructor does not do anything
     */
    void (*destruct)(struct ocrWorkpile_t* base);
    /*! \brief Interface to extract a task from this pool
     *  \return GUID of the task that is extracted from this task pool
     */
    ocrGuid_t (*pop) ( struct ocrWorkpile_t* base );
    /*! \brief Interface to enlist a task
     *  \param[in]  task_guid   GUID of the task that is to be pushed into this task pool.
     */
    void (*push) ( struct ocrWorkpile_t* base, ocrGuid_t g );
    /*! \brief Interface to alternative extract a task from this pool
     *  \return GUID of the task that is extracted from this task pool
     */
    ocrGuid_t (*steal) ( struct ocrWorkpile_t* base );
} ocrWorkpile_t;


/****************************************************/
/* OCR WORKPILE KINDS AND CONSTRUCTORS              */
/****************************************************/

typedef enum ocr_workpile_kind_enum {
    OCR_DEQUE = 1,
    OCR_MESSAGE_QUEUE = 2
} ocr_workpile_kind;

ocrWorkpile_t * newWorkpile(ocr_workpile_kind workpileType, void * per_type_configuration, void * per_instance_configuration);


/****************************************************/
/* OCR WORKPILE ITERATOR API                        */
/****************************************************/

typedef struct ocrWorkpileIterator_t {
    bool (*hasNext) (struct ocrWorkpileIterator_t*);
    ocrWorkpile_t * (*next) (struct ocrWorkpileIterator_t*);
    void (*reset) (struct ocrWorkpileIterator_t*);
    ocrWorkpile_t ** array;
    int id;
    int curr;
    int mod;
} ocrWorkpileIterator_t;

void workpile_iterator_reset (ocrWorkpileIterator_t * base);
bool workpile_iterator_hasNext (ocrWorkpileIterator_t * base);
ocrWorkpile_t * workpile_iterator_next (ocrWorkpileIterator_t * base);
ocrWorkpileIterator_t* workpile_iterator_constructor ( int i, size_t n_pools, ocrWorkpile_t ** pools );
void workpile_iterator_destructor (ocrWorkpileIterator_t* base);

#endif /* __OCR_WORKPILE_H_ */
