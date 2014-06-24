/**
 * @brief OCR hints
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "hint-all.h"
#include "hint/hint-1-0/hint-1-0.h"

const char * hint_types [] = {
    "HINT_1_0",
    NULL
};

// Add other hints using the same pattern as above

inline ocrHintFactory_t *newHintFactory(hintType_t type, ocrParamList_t *typeArg) {
    switch(type) {
#ifdef ENABLE_HINT_1_0
    case hint_1_0_id:
        return newHintFactory_1_0(typeArg, (u32)type);
#endif
    default:
        ASSERT(0);
    };
    return NULL;
}

