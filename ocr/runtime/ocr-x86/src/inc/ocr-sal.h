/**
 * @brief System Abstraction Layer
 * Not quite hardware ops, not quite runtime, e.g. printf().
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_SAL_H__
#define __OCR_SAL_H__

#include "ocr-config.h"

struct _ocrPolicyDomain_t;
struct _ocrWorker_t;
struct _ocrTask_t;
struct _ocrPolicyMsg_t;
void getCurrentEnv(struct _ocrPolicyDomain_t** pd, struct _ocrWorker_t** worker,
                   struct _ocrTask_t **task, struct _ocrPolicyMsg_t* msg);

// abstract log class
typedef struct _ocrLog_t
{
    u32 (*print)(struct _ocrLog_t *self, char *msg, u32 len);
    u32 (*destruct)(struct _ocrLog_t *self);
} ocrLog_t;

typedef struct _ocrLogFactory_t
{
    ocrLog_t * (*instantiate)(struct _ocrLogFactory_t *self, char *filename);
    u32 (*initialize)(struct _ocrLogFactory_t *self, ocrLog_t *log, char *filename);
    void (*destruct)(struct _ocrLogFactory_t *self);
} ocrLogFactory_t;


u32 SNPRINTF(char * buf, u32 size, char * fmt, ...);
u32 PRINTF(char * fmt, ...);
u32 FPRINTF(ocrLog_t * log, char * fmt, ...);

#if defined(SAL_FSIM_XE)
#include "sal/fsim-xe/ocr-sal-fsim-xe.h"
#elif defined(SAL_FSIM_CE)
#include "sal/fsim-ce/ocr-sal-fsim-ce.h"
#elif defined(SAL_LINUX)
#include "sal/linux/ocr-sal-linux.h"
#else
#error "Unknown SAL layer"
#endif /* Switch of SAL_LAYER */

#endif /* __OCR_SAL_H__ */
