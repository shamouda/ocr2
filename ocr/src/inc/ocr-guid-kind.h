/**
 * @brief OCR internal API to GUID management
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_GUID_KIND_H__
#define __OCR_GUID_KIND_H__

// Warning: Any updates made to the enum must also be applied
//          to the implementation of the ocrGuidKindToChar function
// Warning: This also needs to be kept in sync with the ocrGuidUserKind
//          struct in inc/extensions/ocr-labeling.h. See BUG #561
typedef enum {
    OCR_GUID_NONE = 0,
    OCR_GUID_ALLOCATOR = 1,
    OCR_GUID_DB = 2,
    OCR_GUID_EDT = 3,
    OCR_GUID_EDT_TEMPLATE = 4,
    OCR_GUID_POLICY = 5,
    OCR_GUID_WORKER = 6,
    OCR_GUID_MEMTARGET = 7,
    OCR_GUID_COMPTARGET = 8,
    OCR_GUID_SCHEDULER = 9,
    OCR_GUID_WORKPILE = 10,
    OCR_GUID_COMM = 11,
    OCR_GUID_AFFINITY = 12,
    OCR_GUID_SCHEDULER_OBJECT = 13,
    OCR_GUID_SCHEDULER_HEURISTIC = 14,
    OCR_GUID_GUIDMAP = 15,
    OCR_GUID_EVENT = 16, // 0x10 bit denotes an event
    OCR_GUID_EVENT_ONCE = 17,
#ifdef ENABLE_EXTENSION_COUNTED_EVT
    OCR_GUID_EVENT_COUNTED = 18,
#endif
    OCR_GUID_EVENT_IDEM = 19,
    OCR_GUID_EVENT_STICKY = 21,
    OCR_GUID_EVENT_LATCH = 22,
#ifdef ENABLE_EXTENSION_CHANNEL_EVT
    OCR_GUID_EVENT_CHANNEL = 23,
#endif
    OCR_GUID_MAX = 24 // Not a guid kind, just a marker
} ocrGuidKind;

char * ocrGuidKindToChar(ocrGuidKind kind);

#endif /* __OCR_GUID_KIND_H__ */
