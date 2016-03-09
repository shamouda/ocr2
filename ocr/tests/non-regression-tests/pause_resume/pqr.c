/*
 * @brief: Application to demo the usage of the pause/query/resume framework.
 *         Added test application to demo pause/query/resume framework
 *         Creates a spawner EDT that spawns two functionally equivalent EDTs
 *         <EDT_COUNT> times. Each spawned EDT shows the methodology of using
 *         ocrPause(), ocrQuery(), and ocrResume().
 *
 *
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include <ocr.h>
#define EDT_COUNT 5

#ifdef ENABLE_EXTENSION_PAUSE

#include "extensions/ocr-pause.h"



ocrGuid_t pqrTaskA(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]){

    //OUT variables for query call
    void *result;
    u32 dbSize;

    //Call pause with blocking flag.
    u32 ret = ocrPause(true);
    if(ret == 1){
        PRINTF("Pausing in Dummy A.\n");
    }

    //Call query
    ocrGuid_t dbGuid = ocrQuery(OCR_QUERY_READY_EDTS, NULL_GUID, &result, &dbSize, 0);

    //Get query contents from datablock.
    //This query type returns a datablock in *result containing an
    //array of EDT guids appearing next in each worker's workpile.
    ocrGuid_t *resultEdts = (ocrGuid_t *) result;
    u32 numEdts = dbSize/(sizeof(ocrGuid_t));
    u64 i;

    for(i = 0; i < numEdts; i++){
        ocrGuid_t curResult = resultEdts[i];
        //At this point it is up to the user to decide
        //what to do with each EDT guid... here we just print
        PRINTF("Next workiple EDT on worker %"PRId32": "GUIDF"\n", i, GUIDA(curResult));
    }

    //Destroy the datablock - Must be done by user
    ocrDbDestroy(dbGuid);

    //Call resume
    ocrResume(ret);

    return NULL_GUID;
}

ocrGuid_t pqrTaskB(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]){
    //Pause/Query/Resume calls have the ability to block within EDTs
    //thus, can be called from multiple EDTs concurrently

    void *result;
    u32 dbSize;

    //Call pause with blocking flag
    u32 ret = ocrPause(true);
    if(ret==1){
        PRINTF("Pausing in Dummy B.\n");
    }

    //Call query
    ocrGuid_t dbGuid = ocrQuery(OCR_QUERY_READY_EDTS, NULL_GUID, &result, &dbSize, 0);

    //Get query contents from result datablock
    ocrGuid_t *resultEdts = (ocrGuid_t *) result;
    u32 numEdts = dbSize/(sizeof(ocrGuid_t));
    u64 i;

    for(i = 0; i < numEdts; i++){
        ocrGuid_t curResult = resultEdts[i];
        PRINTF("Next workiple EDT on worker %"PRId32": "GUIDF"\n", i, GUIDA(curResult));
    }

    //Destroy the datablock
    ocrDbDestroy(dbGuid);

    //Call resume
    ocrResume(ret);

    return NULL_GUID;
}

ocrGuid_t finishTask( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]){
    PRINTF("Shutting Down OCR\n");
    ocrShutdown();
    return NULL_GUID;
}


ocrGuid_t spawnerTask(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]){
    u32 loopCounter = paramv[0];
    int i = 0;
    for(i = 0; i < loopCounter; i++){
        ocrGuid_t templateA, templateB;
        ocrGuid_t edtA, edtB;

        ocrEdtTemplateCreate(&templateA, pqrTaskA, 1, 0);
        ocrEdtTemplateCreate(&templateB, pqrTaskB, 1, 0);
        u64 paramsA[1] = {i};
        u64 paramsB[1] = {i};

        ocrEdtCreate(&edtA, templateA, EDT_PARAM_DEF, paramsA, EDT_PARAM_DEF,
            NULL, EDT_PROP_NONE, NULL_HINT, NULL);

        ocrEdtCreate(&edtB, templateB, EDT_PARAM_DEF, paramsB, EDT_PARAM_DEF,
            NULL, EDT_PROP_NONE, NULL_HINT, NULL);
    }

    return NULL_GUID;
}


ocrGuid_t mainEdt( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]){
    ocrGuid_t spawnerTemplate;
    ocrGuid_t spawnerEdt;
    ocrGuid_t finishEvent;


    ocrEdtTemplateCreate(&spawnerTemplate, spawnerTask, 1, 1);

    u64 spawnerParamv[1] = {(u32) EDT_COUNT};

    ocrEdtCreate(&spawnerEdt, spawnerTemplate, EDT_PARAM_DEF, spawnerParamv,
        EDT_PARAM_DEF, NULL, EDT_PROP_FINISH, NULL_HINT, &finishEvent);

    ocrGuid_t finishTemplate;
    ocrGuid_t finishEdt;

    ocrEdtTemplateCreate(&finishTemplate, finishTask, 0, 1);
    ocrEdtCreate(&finishEdt, finishTemplate, EDT_PARAM_DEF, NULL,
        EDT_PARAM_DEF, &finishEvent, EDT_PROP_NONE, NULL_HINT, NULL);

    ocrAddDependence(NULL_GUID, spawnerEdt, 0, DB_DEFAULT_MODE);

    return NULL_GUID;

}

#else
ocrGuid_t mainEdt( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]){
    //Temporary placeholder until pause/query/resume is supported
    //on distributed x86 implementations.
    ocrShutdown();
    return NULL_GUID;
}

#endif /*ENABLE_EXTENSION_PAUSE*/

