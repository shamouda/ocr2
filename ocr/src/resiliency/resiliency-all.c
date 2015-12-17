/*
* This file is subject to the license agreement located in the file LICENSE
* and cannot be distributed without it. This notice cannot be
* removed or modified.
*/

#include "resiliency/resiliency-all.h"
#include "debug.h"

const char * resiliency_types[] = {
#ifdef ENABLE_RESILIENCY_NULL
    "null",
#endif
#ifdef ENABLE_RESILIENCY_X86
    "x86",
#endif
#ifdef ENABLE_RESILIENCY_TG
    "tg",
#endif
    NULL,
};

ocrResiliencyFactory_t *newResiliencyFactory(resiliencyType_t type, ocrParamList_t *typeArg) {
    switch(type) {
#ifdef ENABLE_RESILIENCY_NULL
    case resiliencyNull_id:
        return newResiliencyFactoryNull(typeArg);
#endif
#ifdef ENABLE_RESILIENCY_X86
    case resiliencyX86_id:
        return newResiliencyFactoryX86(typeArg);
#endif
#ifdef ENABLE_RESILIENCY_TG
    case resiliencyTG_id:
        return newResiliencyFactoryTG(typeArg);
#endif
    default:
        ASSERT(0);
        return NULL;
    };
}

