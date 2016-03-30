/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_EXTENSION_LABELING

#include "debug.h"
#include "extensions/ocr-labeling.h"
#include "experimental/ocr-labeling-runtime.h"
#include "ocr-errors.h"
#include "ocr-hal.h"
#include "ocr-policy-domain.h"
#include "ocr-types.h"

#include "utils/profiler/profiler.h"

#pragma message "GUID labeling extension is experimental and may not be supported on all platforms"

// Related to GUIDs
#define DEBUG_TYPE GUID

u8 ocrGuidMapCreate(ocrGuid_t *mapGuid, u32 numParams,
                    ocrGuid_t (*mapFunc)(ocrGuid_t startGuid, u64 skipGuid,
                                         s64* params, s64* tuple),
                    s64* params, u64 numberGuid, ocrGuidUserKind kind) {
    START_PROFILE(api_ocrGuidMapCreate);
    ocrPolicyDomain_t *pd = NULL;
    ocrGuidMap_t *myMap = NULL;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_CREATE
    msg.type = PD_MSG_GUID_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid.guid) = UNINITIALIZED_GUID;
    PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
    // Size is properly rounded so that the s64 params are properly aligned
    PD_MSG_FIELD_I(size) = ((sizeof(ocrGuidMap_t) + sizeof(s64) - 1) & ~(sizeof(s64)-1)) + numParams*sizeof(s64);
    PD_MSG_FIELD_I(kind) = OCR_GUID_GUIDMAP;
    PD_MSG_FIELD_I(properties) = 0;
    u8 returnCode = pd->fcts.processMessage(pd, &msg, true);
    if(!((returnCode == 0) && ((returnCode = PD_MSG_FIELD_O(returnDetail)) == 0))) {
        RETURN_PROFILE(returnCode);
    }
    myMap = PD_MSG_FIELD_IO(guid.metaDataPtr);
    *mapGuid = PD_MSG_FIELD_IO(guid.guid);
#undef PD_TYPE
    ASSERT(myMap != NULL);
    myMap->mapFunc = mapFunc;
    myMap->params = (s64*)((char*)myMap + ((sizeof(ocrGuidMap_t) + sizeof(s64) - 1) & ~(sizeof(s64)-1)));
    myMap->numGuids = numberGuid;
    myMap->numParams = numParams;
    hal_memCopy(myMap->params, params, sizeof(s64)*numParams, false);

    // Now actually reserve the GUID space
    getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_TYPE PD_MSG_GUID_RESERVE
    msg.type = PD_MSG_GUID_RESERVE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_I(numberGuids) = numberGuid;
    PD_MSG_FIELD_I(guidKind) = (ocrGuidKind)kind;
    //BUG #527: memory reclaim: There is a leak if this fails
    returnCode = pd->fcts.processMessage(pd, &msg, true);
    if(!((returnCode == 0) && ((returnCode = PD_MSG_FIELD_O(returnDetail)) == 0))) {
        RETURN_PROFILE(returnCode);
    }

    myMap->startGuid = PD_MSG_FIELD_O(startGuid);
    myMap->skipGuid = PD_MSG_FIELD_O(skipGuid);
#undef PD_TYPE
#undef PD_MSG
    RETURN_PROFILE(0);
}

u8 ocrGuidRangeCreate(ocrGuid_t *mapGuid,
                      u64 numberGuid, ocrGuidUserKind kind) {

    START_PROFILE(api_ocrGuidRangeCreate);
    ocrPolicyDomain_t *pd = NULL;
    ocrGuidMap_t *myMap = NULL;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_CREATE
    msg.type = PD_MSG_GUID_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid.guid) = UNINITIALIZED_GUID;
    PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
    // Size is properly rounded so that the s64 params are properly aligned
    PD_MSG_FIELD_I(size) = sizeof(ocrGuidMap_t);
    PD_MSG_FIELD_I(kind) = OCR_GUID_GUIDMAP;
    PD_MSG_FIELD_I(properties) = 0;
    u8 returnCode = pd->fcts.processMessage(pd, &msg, true);
    if(!((returnCode == 0) && ((returnCode = PD_MSG_FIELD_O(returnDetail)) == 0))) {
        RETURN_PROFILE(returnCode);
    }

    myMap = PD_MSG_FIELD_IO(guid.metaDataPtr);
    *mapGuid = PD_MSG_FIELD_IO(guid.guid);
#undef PD_TYPE
    ASSERT(myMap != NULL);
    myMap->mapFunc = NULL;
    myMap->params = NULL;
    myMap->numGuids = numberGuid;
    myMap->numParams = 0;

    // Now actually reserve the GUID space
    getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_TYPE PD_MSG_GUID_RESERVE
    msg.type = PD_MSG_GUID_RESERVE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_I(numberGuids) = numberGuid;
    PD_MSG_FIELD_I(guidKind) = (ocrGuidKind)kind;
    //BUG #527: memory reclaim: There is a leak if this fails
    returnCode = pd->fcts.processMessage(pd, &msg, true);
    if(!((returnCode == 0) && ((returnCode = PD_MSG_FIELD_O(returnDetail)) == 0))) {
        RETURN_PROFILE(returnCode);
    }
    myMap->startGuid = PD_MSG_FIELD_O(startGuid);
    myMap->skipGuid = PD_MSG_FIELD_O(skipGuid);
#undef PD_TYPE
#undef PD_MSG
    RETURN_PROFILE(0);
}

u8 ocrGuidMapDestroy(ocrGuid_t mapGuid) {
    // Reverse of the map create: unreserve the space and destroy our GUID

    START_PROFILE(api_ocrGuidMapDestroy);
    ocrPolicyDomain_t *pd = NULL;
    ocrGuidMap_t *myMap = NULL;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_INFO
    msg.type = PD_MSG_GUID_INFO | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid.guid) = mapGuid;
    PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(properties) = RMETA_GUIDPROP;
    RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, true));
    myMap = (ocrGuidMap_t*)PD_MSG_FIELD_IO(guid.metaDataPtr);
#undef PD_TYPE
    getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_TYPE PD_MSG_GUID_UNRESERVE
    ASSERT(myMap); // This means that the map was not found. Runtime error
    msg.type = PD_MSG_GUID_UNRESERVE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_I(startGuid) = myMap->startGuid;
    PD_MSG_FIELD_I(skipGuid) = myMap->skipGuid;
    PD_MSG_FIELD_I(numberGuids) = myMap->numGuids;
    u8 returnCode = pd->fcts.processMessage(pd, &msg, true);
    if(!((returnCode == 0) && ((returnCode = PD_MSG_FIELD_O(returnDetail)) == 0))) {
        RETURN_PROFILE(returnCode);
    }
#undef PD_TYPE

    // Now free the GUID
    getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_TYPE PD_MSG_GUID_DESTROY
    msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
    PD_MSG_FIELD_I(guid.guid) = mapGuid;
    PD_MSG_FIELD_I(guid.metaDataPtr) = myMap;
    PD_MSG_FIELD_I(properties) = 1;
    RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, true));
#undef PD_TYPE
#undef PD_MSG
    RETURN_PROFILE(0);
}

u8 ocrGuidMapSetDefaultMap(u32 numParams, ocrGuid_t (*mapFunc)(
                               ocrGuid_t startGuid, u64 skipGuid, s64* params, s64* tuple),
                           s64* params, u64 numberGuid, ocrGuidUserKind kind) {

    // Default map unsupported for now
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 ocrGuidFromLabel(ocrGuid_t *outGuid, ocrGuid_t mapGuid, s64* tuple) {
    START_PROFILE(api_ocrGuidFromLabel);
    ASSERT(!(ocrGuidIsNull(mapGuid)));  // Default map unsupported for now
    ocrPolicyDomain_t *pd = NULL;
    ocrGuidMap_t *myMap = NULL;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_INFO
    msg.type = PD_MSG_GUID_INFO | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid.guid) = mapGuid;
    PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(properties) = RMETA_GUIDPROP;
    u8 returnCode = pd->fcts.processMessage(pd, &msg, true);
    //Warning PD_MSG_GUID_INFO returns GUID properties as 'returnDetail', not error code
    if(returnCode != 0) {
        RETURN_PROFILE(returnCode);
    }
    myMap = (ocrGuidMap_t*)PD_MSG_FIELD_IO(guid.metaDataPtr);
#undef PD_TYPE
#undef PD_MSG
    ASSERT(myMap != NULL);
    DPRINTF(DEBUG_LVL_VVERB, "For map "GUIDF", calling map with start: "GUIDF", stride: 0x%"PRIx64"\n",
            GUIDA(mapGuid), GUIDA(myMap->startGuid), myMap->skipGuid);
    if(myMap->mapFunc == NULL) {
        DPRINTF(DEBUG_LVL_WARN, "ocrGuidFromLabel requires a map created with ocrGuidMapCreate (not a range)\n");
        RETURN_PROFILE(OCR_EINVAL);
    }
    *outGuid = myMap->mapFunc(myMap->startGuid, myMap->skipGuid, myMap->params, tuple);
    DPRINTF(DEBUG_LVL_VERB, "Returning GUID "GUIDF"\n", GUIDA(*outGuid));
    RETURN_PROFILE(0);
}

u8 ocrGuidFromIndex(ocrGuid_t *outGuid, ocrGuid_t rangeGuid, u64 idx) {
    START_PROFILE(api_ocrGuidFromIndex);
    if(ocrGuidIsNull(rangeGuid)){
        RETURN_PROFILE(OCR_EINVAL);
    }

    ocrPolicyDomain_t *pd = NULL;
    ocrGuidMap_t *myMap = NULL;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_INFO
    msg.type = PD_MSG_GUID_INFO | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid.guid) = rangeGuid;
    PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(properties) = RMETA_GUIDPROP;
    u8 returnCode = pd->fcts.processMessage(pd, &msg, true);
    //Warning PD_MSG_GUID_INFO returns GUID properties as 'returnDetail', not error code
    if(returnCode != 0) {
        RETURN_PROFILE(returnCode);
    }
    myMap = (ocrGuidMap_t*)PD_MSG_FIELD_IO(guid.metaDataPtr);
#undef PD_TYPE
#undef PD_MSG
    ASSERT(myMap != NULL);
    DPRINTF(DEBUG_LVL_VVERB, "For range "GUIDF", calling map with start: "GUIDF", stride: 0x%"PRIx64"\n",
            GUIDA(rangeGuid), GUIDA(myMap->startGuid), myMap->skipGuid);
    if(myMap->mapFunc != NULL) {
        DPRINTF(DEBUG_LVL_WARN, "ocrGuidFromLabel requires a map created with ocrGuidRangeCreate (not a map)\n");
        RETURN_PROFILE(OCR_EINVAL);
    }
    if(idx >= myMap->numGuids) {
        DPRINTF(DEBUG_LVL_WARN, "Invalid index value in ocrGuidFromIndex. Got %"PRIu64", expected 0..%"PRIu64"\n",
                idx, myMap->numGuids-1);
        RETURN_PROFILE(OCR_EINVAL);
    }

    // See BUG #928 on GUID issues
#if GUID_BIT_COUNT == 64
    outGuid->guid = myMap->startGuid.guid + myMap->skipGuid*idx;
#elif GUID_BIT_COUNT == 128
    outGuid->lower = myMap->startGuid.lower + myMap->skipGuid*idx;
    outGuid->upper = 0x0;
#endif
    DPRINTF(DEBUG_LVL_VERB, "Returning GUID "GUIDF"\n", GUIDA(*outGuid));
    RETURN_PROFILE(0);
}

u8 ocrGetGuidKind(ocrGuidUserKind *outKind, ocrGuid_t guid) {
    ASSERT(0); // Not supported just now
    return 0;
}
#endif /* ENABLE_EXTENSION_LABELING */
