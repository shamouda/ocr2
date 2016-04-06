#include "perfs.h"
#include "ocr.h"
#include <math.h>

// DESC: Hierarchically create a tree of task for which each node fanout is 'NODE_FANOUT'
//       Nodes at depth 'TREE_DEPTH-1' create 'LEAF_FANOUT' leaf tasks.
// TIME: Execution of all tasks
// FREQ: Tree is created once
//
// VARIABLES:
// - TREE_DEPTH
// - NODE_FANOUT
// - LEAF_FANOUT

#ifdef REC_SPAWN_FINISH
#define EDT_SPAWN_PROPS EDT_PROP_FINISH
#else
#define EDT_SPAWN_PROPS EDT_PROP_NONE
#endif

ocrGuid_t terminateEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    timestamp_t * timers = (timestamp_t *) depv[1].ptr;
    get_time(&timers[1]);
    ASSERT(TREE_DEPTH > 1);
    PRINTF("Number of nodes %f\n", (pow((double)NODE_FANOUT,(double)(TREE_DEPTH-1))-1));
    PRINTF("Number of leaves %f\n", pow(NODE_FANOUT,(TREE_DEPTH-1))*LEAF_FANOUT);
    summary_throughput_timer(&timers[0], &timers[1],
        (pow((double)NODE_FANOUT,(double)(TREE_DEPTH-1))-1) + // number of nodes
        (pow(NODE_FANOUT,(TREE_DEPTH-1))*LEAF_FANOUT)); // number of leaves
    ocrShutdown(); // This is the last EDT to execute, terminate
    return NULL_GUID;
}

ocrGuid_t workEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    return NULL_GUID;
}

ocrGuid_t spawnerEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {

    u64 depth = paramv[0] + 1;
    if (depth == TREE_DEPTH) {
        ocrGuid_t workEdtTemplateGuid;
        ocrEdtTemplateCreate(&workEdtTemplateGuid, workEdt, 0, 0);
        u64 i = 0;
        while (i < LEAF_FANOUT) {
            ocrGuid_t workEdtGuid;
            ocrEdtCreate(&workEdtGuid, workEdtTemplateGuid,
                         0, NULL, 0, NULL, EDT_PROP_NONE, NULL_HINT, NULL);
            i++;
        }
        ocrEdtTemplateDestroy(workEdtTemplateGuid);
    } else {
        ocrGuid_t spawnerEdtTemplateGuid;
        ocrEdtTemplateCreate(&spawnerEdtTemplateGuid, spawnerEdt, 1, 0);
        u64 i = 0;
        while (i < NODE_FANOUT) {
            ocrGuid_t spawnerEdtGuid;
            ocrEdtCreate(&spawnerEdtGuid, spawnerEdtTemplateGuid,
                         1, &depth, 0, NULL, EDT_SPAWN_PROPS, NULL_HINT, NULL);
            i++;
        }
        ocrEdtTemplateDestroy(spawnerEdtTemplateGuid);
    }

    return NULL_GUID;
}

ocrGuid_t finishEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {

    timestamp_t * dbPtr = (timestamp_t *) depv[0].ptr;

    get_time(&dbPtr[0]);

    ocrGuid_t spawnerEdtTemplateGuid;
    ocrEdtTemplateCreate(&spawnerEdtTemplateGuid, spawnerEdt, 1, 0);

    u64 depth = 1;
    ocrGuid_t spawnerEdtGuid;
    ocrEdtCreate(&spawnerEdtGuid, spawnerEdtTemplateGuid,
                 1, &depth, 0, NULL, EDT_SPAWN_PROPS, NULL_HINT, NULL);

    ocrEdtTemplateDestroy(spawnerEdtTemplateGuid);
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {

    ocrGuid_t terminateEdtTemplateGuid;
    ocrEdtTemplateCreate(&terminateEdtTemplateGuid, terminateEdt, 0, 2);

    ocrGuid_t terminateEdtGuid;
    ocrEdtCreate(&terminateEdtGuid, terminateEdtTemplateGuid,
                 0, NULL, 2, NULL, EDT_PROP_NONE, NULL_HINT, NULL);

    timestamp_t * dbPtr;
    ocrGuid_t dbGuid;
    ocrDbCreate(&dbGuid, (void **)&dbPtr, (sizeof(timestamp_t)*2), 0, NULL_HINT, NO_ALLOC);
    ocrDbRelease(dbGuid);
    ocrAddDependence(dbGuid, terminateEdtGuid, 1, DB_MODE_RW);

    ocrGuid_t oEvtGuid;
    ocrGuid_t finishEdtTemplateGuid;
    ocrEdtTemplateCreate(&finishEdtTemplateGuid, finishEdt, 0, 1);
    ocrGuid_t finishEdtGuid;
    ocrEdtCreate(&finishEdtGuid, finishEdtTemplateGuid,
                 0, NULL, 1, NULL,  EDT_PROP_FINISH, NULL_HINT, &oEvtGuid);

    ocrAddDependence(oEvtGuid, terminateEdtGuid, 0, DB_MODE_CONST);
    ocrAddDependence(dbGuid, finishEdtGuid, 0, DB_MODE_RW);
    return NULL_GUID;
}
