/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "guid/guid-all.h"
#include "debug.h"

const char * guid_types[] = {
#ifdef ENABLE_GUID_PTR
    "PTR",
#endif
#ifdef ENABLE_GUID_COUNTED_MAP
    "COUNTED_MAP",
#endif
#ifdef ENABLE_GUID_LABELED
    "LABELED",
#endif
    NULL
};

ocrGuidProviderFactory_t *newGuidProviderFactory(guidType_t type, ocrParamList_t *typeArg) {
    switch(type) {
#ifdef ENABLE_GUID_PTR
    case guidPtr_id:
        return newGuidProviderFactoryPtr(typeArg, (u32)type);
#endif
#ifdef ENABLE_GUID_COUNTED_MAP
    case guidCountedMap_id:
        return newGuidProviderFactoryCountedMap(typeArg, (u32)type);
#endif
#ifdef ENABLE_GUID_LABELED
    case guidLabeled_id:
        return newGuidProviderFactoryLabeled(typeArg, (u32)type);
#endif
    default:
        ASSERT(0);
    }
    return NULL;
}

char * ocrGuidKindToChar(ocrGuidKind kind) {
    // WARNING these must match the declaration in 'ocr-guid-kind.h'
    switch(kind) {
        case OCR_GUID_NONE:
        return "none";
        case OCR_GUID_ALLOCATOR:
        return "allocator";
        case OCR_GUID_DB:
        return "datablock";
        case OCR_GUID_EDT:
        return "EDT";
        case OCR_GUID_EDT_TEMPLATE:
        return "EDT-template";
        case OCR_GUID_POLICY:
        return "policy-domain";
        case OCR_GUID_WORKER:
        return "worker";
        case OCR_GUID_MEMTARGET:
        return "mem-target";
        case OCR_GUID_COMPTARGET:
        return "comp-target";
        case OCR_GUID_SCHEDULER:
        return "scheduler";
        case OCR_GUID_WORKPILE:
        return "workpile";
        case OCR_GUID_COMM:
        return "comm";
        case OCR_GUID_AFFINITY:
        return "affinity";
        case OCR_GUID_SCHEDULER_OBJECT:
        return "scheduler-object";
        case OCR_GUID_SCHEDULER_HEURISTIC:
        return "scheduler-heuristic";
        case OCR_GUID_GUIDMAP:
        return "map";
        case OCR_GUID_EVENT:
        return "event-abstract";
        case OCR_GUID_EVENT_ONCE:
        return "event-once";
#ifdef ENABLE_EXTENSION_COUNTED_EVT
        case OCR_GUID_EVENT_COUNTED:
        return "event-counted";
#endif
        case OCR_GUID_EVENT_IDEM:
        return "event-idem";
        case OCR_GUID_EVENT_STICKY:
        return "event-sticky";
        case OCR_GUID_EVENT_LATCH:
        return "event-latch";
#ifdef ENABLE_EXTENSION_CHANNEL_EVT
        case OCR_GUID_EVENT_CHANNEL:
        return "event-channel";
#endif
        default:
        return "unknown kind";
    }
}
