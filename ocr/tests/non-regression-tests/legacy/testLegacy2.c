/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

// Only tested when OCR legacy interface is available
#ifdef ENABLE_EXTENSION_LEGACY

#include "extensions/ocr-legacy.h"

/**
 * DESC: OCR-legacy, init, edt-finish does shutdown, block, finalize
 */

#define N 100

// This edt is triggered when the output event of the other edt is satisfied by the runtime
ocrGuid_t terminateEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ASSERT(!(ocrGuidIsNull(depv[1].guid)));
    u64 * array = (u64*)depv[1].ptr;
    u64 i = 0;
    while (i < N) {
        ASSERT(array[i] == i);
        i++;
    }
    PRINTF("Everything went OK\n");
    ocrShutdown(); // This is the last EDT to execute, terminate
    return NULL_GUID;
}

ocrGuid_t updaterEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ASSERT(paramc == 1);
    // Retrieve id
    u64 id = paramv[0];
    ASSERT ((id>=0) && (id < N));
    u64 * dbPtr = (u64 *) depv[0].ptr;
    dbPtr[id] = id;
    return NULL_GUID;
}

ocrGuid_t rootEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t dbGuid = depv[0].guid;
    ocrGuid_t updaterEdtTemplateGuid;
    ocrEdtTemplateCreate(&updaterEdtTemplateGuid, updaterEdt, 1 /*paramc*/, 1/*depc*/);
    u64 i = 0;
    while (i < N) {
        // Pass down the index to write to and the db guid through params
        // (Could also be done through dependences)
        u64 nparamv = i;
        // Pass the guid we got fron depv to the updaterEdt through depv
        ocrGuid_t updaterEdtGuid;
        ocrEdtCreate(&updaterEdtGuid, updaterEdtTemplateGuid, EDT_PARAM_DEF, &nparamv, EDT_PARAM_DEF, &dbGuid, 0, NULL_HINT, NULL);
        i++;
    }
    return NULL_GUID;
}

ocrGuid_t headEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t terminateEdtGuid;
    ocrGuid_t terminateEdtTemplateGuid;
    ocrEdtTemplateCreate(&terminateEdtTemplateGuid, terminateEdt, 0 /*paramc*/, 2 /*depc*/);
    ocrEdtCreate(&terminateEdtGuid, terminateEdtTemplateGuid, EDT_PARAM_DEF, /*paramv=*/NULL, EDT_PARAM_DEF, /*depv=*/NULL,
                 0, NULL_HINT, /*outEvent=*/NULL);
    ocrGuid_t outputEventGuid;
    ocrGuid_t rootEdtGuid;
    ocrGuid_t rootEdtTemplateGuid;
    ocrEdtTemplateCreate(&rootEdtTemplateGuid, rootEdt, 0 /*paramc*/, 1 /*depc*/);
    ocrEdtCreate(&rootEdtGuid, rootEdtTemplateGuid, EDT_PARAM_DEF, /*paramv=*/NULL, EDT_PARAM_DEF, /*depv=*/NULL,
                 EDT_PROP_FINISH, NULL_HINT, /*outEvent=*/&outputEventGuid);
    u64 * array;
    ocrGuid_t dbGuid;
    ocrDbCreate(&dbGuid,(void **) &array, sizeof(u64)*N, 0, NULL_HINT, NO_ALLOC);
    ocrDbRelease(dbGuid);

    // When output-event will be satisfied, the whole task sub-tree
    // spawned by rootEdt will be done, and shutdownEdt is called.
    ocrAddDependence(outputEventGuid, terminateEdtGuid, 0, DB_MODE_NULL);
    ocrAddDependence(dbGuid, terminateEdtGuid, 1, DB_MODE_RO);

    // Schedule the root EDT
    ocrAddDependence(dbGuid, rootEdtGuid, 0, DB_MODE_RO);

    return NULL_GUID;
}

int main(int argc, const char * argv[]) {
    ocrConfig_t ocrConfig;
    ocrGuid_t legacyCtx;
    ocrParseArgs(argc, argv, &ocrConfig);
    ocrLegacyInit(&legacyCtx, &ocrConfig);
    ocrGuid_t headEdtGuid;
    ocrGuid_t headEdtTemplateGuid;
    ocrEdtTemplateCreate(&headEdtTemplateGuid, headEdt, 0, 0);
    ocrEdtCreate(&headEdtGuid, headEdtTemplateGuid, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL,
                 EDT_PROP_NONE, NULL_HINT, NULL);
    ocrLegacyFinalize(legacyCtx, true);
    return 0;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrShutdown();
    return NULL_GUID;
}

#endif
