#ifndef __GASNET_POLICY_DOMAIN_H__
#define __GASNET_POLICY_DOMAIN_H__

#include "splay-tree.h"
#include "gasnet-comm-platform.h"

/*
 * Data structure for gasnet policy domain using splay tree
 * The tree will be used to store the most current policy domain looking for work
 * (which is the root)
 */
typedef struct PDQueue_s {
    SPLAY_ENTRY(PDQueue_s) link;
    ocrCommPlatformGasnet_t *platform;
    int counter;
} PDQueue_t;

ocrCommPlatformGasnet_t* getCommPlatform();
void addCommPlatform(ocrCommPlatform_t * self);
void pdLookingForWork( ocrCommPlatformGasnet_t* self );

#endif /* __GASNET_POLICY_DOMAIN_H__ */
