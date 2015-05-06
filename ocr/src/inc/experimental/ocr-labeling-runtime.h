/**
 * @brief Runtime support for labeling.
  **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_LABELING_RUNTIME_H__
#define __OCR_LABELING_RUNTIME_H__

#ifdef ENABLE_EXTENSION_LABELING

/**
 * @brief Class to represent a GUID map which
 * allows the conversion of a tuple to a GUID
 */
typedef struct _ocrGuidMap_t {
    ocrGuid_t (*mapFunc)(ocrGuid_t, u64, s64*, s64*);
    ocrGuid_t startGuid;
    u64 skipGuid;
    u64 numGuids;
    s64* params; // Points to the parameters (usually right after this structure
                 // Pointer is not absolutely necessary but convenient
    u32 numParams;
} ocrGuidMap_t;


#endif /* ENABLE_EXTENSION_LABELING */
#endif /* __OCR_LABELING_RUNTIME_H__ */
