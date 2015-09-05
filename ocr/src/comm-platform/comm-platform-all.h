/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __COMM_PLATFORM_ALL_H__
#define __COMM_PLATFORM_ALL_H__

#include "debug.h"
#include "ocr-comm-platform.h"
#include "ocr-config.h"
#include "utils/ocr-utils.h"

typedef enum _commPlatformType_t {
#ifdef ENABLE_COMM_PLATFORM_NULL
    commPlatformNull_id,
#endif
#ifdef ENABLE_COMM_PLATFORM_CE
    commPlatformCe_id,
#endif
#ifdef ENABLE_COMM_PLATFORM_XE
    commPlatformXe_id,
#endif
#ifdef ENABLE_COMM_PLATFORM_CE_PTHREAD
    commPlatformCePthread_id,
#endif
#ifdef ENABLE_COMM_PLATFORM_XE_PTHREAD
    commPlatformXePthread_id,
#endif
#ifdef ENABLE_COMM_PLATFORM_MPI
    commPlatformMPI_id,
#endif
#ifdef ENABLE_COMM_PLATFORM_GASNET
    commPlatformGasnet_id,
#endif
    commPlatformMax_id
} commPlatformType_t;

extern const char * commplatform_types[];

#ifdef ENABLE_COMM_PLATFORM_NULL
#include "comm-platform/null/null-comm-platform.h"
#endif
#ifdef ENABLE_COMM_PLATFORM_CE
#include "comm-platform/ce/ce-comm-platform.h"
#endif
#ifdef ENABLE_COMM_PLATFORM_XE
#include "comm-platform/xe/xe-comm-platform.h"
#endif
#ifdef ENABLE_COMM_PLATFORM_CE_PTHREAD
#include "comm-platform/ce-pthread/ce-pthread-comm-platform.h"
#endif
#ifdef ENABLE_COMM_PLATFORM_XE_PTHREAD
#include "comm-platform/xe-pthread/xe-pthread-comm-platform.h"
#endif
#ifdef ENABLE_COMM_PLATFORM_MPI
#include "comm-platform/mpi/mpi-comm-platform.h"
#endif
#ifdef ENABLE_COMM_PLATFORM_GASNET
#include "comm-platform/gasnet/gasnet-comm-platform.h"
#endif

// Add other communication platforms using the same pattern as above

ocrCommPlatformFactory_t *newCommPlatformFactory(commPlatformType_t type, ocrParamList_t *typeArg);

#endif /* __COMM_PLATFORM_ALL_H__ */
