/* Copyright (c) 2013, Rice University

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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "ocr.h"


/**
 * DESC: RT-API: Test 'ocrThrottle down'
 */

// Only tested when OCR runtime API is available
#ifdef RUNTIME_ITF_EXT

#include "ocr-runtime-itf.h"

u64 myAdaptObjective(u64 tuning) {
  // Input is number of active workers
  // Output is chunk size
	int nbWorkers = ocrNbWorkers();
	if (tuning > (nbWorkers/2)) {
		return 10;
	} else {
		return 5;
	}
}

ocrGuid_t thirdEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    printf("Execute third async\n");
    u64 obj = ocrGetAdaptObjective();
    assert(obj == 10);
    ocrShutdown(); // This is the last EDT to execute, terminate
    return NULL_GUID;
}

ocrGuid_t secondEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    printf("Execute second async\n");
    u64 obj = ocrGetAdaptObjective();
    assert(obj == 5);
    printf("Throttling up\n");
    ocrThrottle(150);
    printf("Throttled up complete\n");
	// spawn a new edt after throttling up
	ocrGuid_t thirdEdtTemplateGuid;
    ocrEdtTemplateCreate(&thirdEdtTemplateGuid, thirdEdt, 0 /*paramc*/, 0 /*depc*/);
	ocrGuid_t thirdEdtGuid;
    ocrEdtCreate(&thirdEdtGuid, thirdEdtTemplateGuid, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL_GUID,
                    /*properties=*/EDT_PROP_NONE, NULL_GUID, /*outEvent=*/ NULL);
    return NULL_GUID;
}

ocrGuid_t firstEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    printf("Execute first async\n");
    u64 obj = ocrGetAdaptObjective();
    assert(obj == 10);
    printf("Throttling down\n");
    ocrThrottle(50);
    printf("Throttled down complete\n");
 	// spawn a new edt after throttling down
	ocrGuid_t secondEdtTemplateGuid;
    ocrEdtTemplateCreate(&secondEdtTemplateGuid, secondEdt, 0 /*paramc*/, 0 /*depc*/);
	ocrGuid_t secondEdtGuid;
    ocrEdtCreate(&secondEdtGuid, secondEdtTemplateGuid, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL_GUID,
                    /*properties=*/EDT_PROP_NONE, NULL_GUID, /*outEvent=*/ NULL);
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
	// register objective function
	ocrRegisterAdaptFct(myAdaptObjective);

	ocrGuid_t firstEdtTemplateGuid;
    ocrEdtTemplateCreate(&firstEdtTemplateGuid, firstEdt, 0 /*paramc*/, 0 /*depc*/);

	ocrGuid_t firstEdtGuid;
    ocrEdtCreate(&firstEdtGuid, firstEdtTemplateGuid, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL_GUID,
                    /*properties=*/EDT_PROP_NONE, NULL_GUID, /*outEvent=*/ NULL);

    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    printf("No RT API\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif
