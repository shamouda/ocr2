
/**
 * @brief Trivial implementation of GUIDs
 * @authors Romain Cledat, Intel Corporation
 * @date 2012-11-13
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of Intel Corporation nor the names
 *       of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "debug.h"
#include "guid/counter/atomic-guid.h"
#include "ocr-macros.h"

#include <stdlib.h>

extern void initMap();
extern void * getGuid(ocrGuid_t guid);
extern void setGuid(ocrGuid_t guid, void * data);

typedef struct {
    ocrGuidKind kind;
    void * ptr;
} ocrGuidImpl_t;

static u64 guidCounter = 0;

static ocrGuid_t nextGuid() {
	//TODO can workout something with the thread-id
    // Not sure if a PD and its atomic factory are live at that point.
    return (ocrGuid_t) __sync_add_and_fetch(&guidCounter, 1);
}

static void atomicDestruct(ocrGuidProvider_t* self) {
    free(self);
    return;
}

static u8 atomicGetGuid(ocrGuidProvider_t* self, ocrGuid_t* guid, u64 val, ocrGuidKind kind) {
	ocrGuid_t newGuid = nextGuid();
    ocrGuidImpl_t * guidInst = (ocrGuidImpl_t *) malloc(sizeof(ocrGuidImpl_t));
    guidInst->kind = kind;
    guidInst->ptr = (void *) val;
	setGuid(newGuid, guidInst);
    *guid = newGuid;
    return 0;
}

static u8 atomicGetVal(ocrGuidProvider_t* self, ocrGuid_t guid, u64* val, ocrGuidKind* kind) {
    ocrGuidImpl_t * guidInst = (ocrGuidImpl_t *) getGuid(guid);
    *val = (u64) guidInst->ptr;
    if(kind)
        *kind = guidInst->kind;
    return 0;
}

static u8 atomicGetKind(ocrGuidProvider_t* self, ocrGuid_t guid, ocrGuidKind* kind) {
    ocrGuidImpl_t * guidInst = (ocrGuidImpl_t *) getGuid(guid);
    *kind = guidInst->kind;
    return 0;
}

static u8 atomicReleaseGuid(ocrGuidProvider_t *self, ocrGuid_t guid) {
	//TODO remove entry from hash_map
	ocrGuidImpl_t * guidInst = (ocrGuidImpl_t *) getGuid(guid);
    free((ocrGuidImpl_t*) guidInst);
    return 0;
}

static ocrGuidProvider_t* newGuidProviderAtomic(ocrGuidProviderFactory_t *factory,
                                             ocrParamList_t *perInstance) {
    ocrGuidProvider_t *base = (ocrGuidProvider_t*) malloc(sizeof(ocrGuidProviderAtomic_t));
    base->fctPtrs = &(factory->providerFcts);
    return base;
}

/****************************************************/
/* OCR GUID PROVIDER PTR FACTORY                    */
/****************************************************/

static void destructGuidProviderFactoryAtomic(ocrGuidProviderFactory_t *factory) {
    free(factory);
}

ocrGuidProviderFactory_t *newGuidProviderFactoryAtomic(ocrParamList_t *typeArg) {
    ocrGuidProviderFactory_t *base = (ocrGuidProviderFactory_t*) malloc(sizeof(ocrGuidProviderFactoryAtomic_t));
    base->instantiate = &newGuidProviderAtomic;
    base->destruct = &destructGuidProviderFactoryAtomic;
    base->providerFcts.destruct = &atomicDestruct;
    base->providerFcts.getGuid = &atomicGetGuid;
    base->providerFcts.getVal = &atomicGetVal;
    base->providerFcts.getKind = &atomicGetKind;
    base->providerFcts.releaseGuid = &atomicReleaseGuid;
    initMap();
    return base;
}
