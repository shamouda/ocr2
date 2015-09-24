/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __WORKER_ALL_H__
#define __WORKER_ALL_H__

#include "ocr-config.h"
#include "utils/ocr-utils.h"
#include "ocr-worker.h"

typedef enum _workerType_t {
#ifdef ENABLE_WORKER_HC
    workerHc_id,
#endif
#ifdef ENABLE_WORKER_HC_COMM
    workerHcComm_id,
#endif
#ifdef ENABLE_WORKER_XE
    workerXe_id,
#endif
#ifdef ENABLE_WORKER_CE
    workerCe_id,
#endif
#ifdef ENABLE_WORKER_SYSTEM
    workerSystem_id,
#endif
    workerMax_id
} workerType_t;

extern const char * worker_types[];

// The below is to look up ocrWorkerType_t in inc/ocr-runtime-types.h
extern const char * ocrWorkerType_types[];

#ifdef ENABLE_WORKER_HC
#include "worker/hc/hc-worker.h"
#endif
#ifdef ENABLE_WORKER_HC_COMM
#include "worker/hc-comm/hc-comm-worker.h"
#endif
#ifdef ENABLE_WORKER_XE
#include "worker/xe/xe-worker.h"
#endif
#ifdef ENABLE_WORKER_CE
#include "worker/ce/ce-worker.h"
#endif
#ifdef ENABLE_WORKER_SYSTEM
#include "worker/system/system-worker.h"
#endif

ocrWorkerFactory_t * newWorkerFactory(workerType_t type, ocrParamList_t *perType);

#endif /* __WORKER_ALL_H__ */
