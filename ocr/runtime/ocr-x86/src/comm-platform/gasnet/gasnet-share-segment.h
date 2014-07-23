#ifndef __GASNET_SHARE_SEGMENT_H__
#define __GASNET_SHARE_SEGMENT_H__

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

// maximum blocks of shared segments.
// other nodes will use this block to send large messages.
// if a message doesn't fit into a reserved block, the
// sender is required to send multiple split messages
#define GASNET_MAX_SEGMENT_BLOCK 4

/*
 * blocks of segment structure
 * containing the address of the segment and its maximum size
 */
typedef struct {
    void *addr;    // beginning address of the segment
    u32  size;     // the size of this block
} gasnetCommBlock_t;


/*
 * blocks of segment for the sender
 * we assume only one communication worker at a time accessing this data
 */
typedef struct {
    gasnetCommBlock_t block;    // the block of the segment
    u32  reserved;      // flag if the block has been reserved
} gasnetCommBlockSender_t;


/*
 * @brief setup initialization
 */
void gasnetBlockInit();

/*
 * @brief get and reserve a segment block
 * if no more block available, it returns null
 */
gasnetCommBlockSender_t* gasnetReserveSegmentBlock();

/*
 * @brief release a segment block
 */
void gasnetReleaseSegmentBlock(void *addr);

/*
 * @brief push a segment block into the database
 */
void gasnetSegmentBlockPush(ocrPolicyDomain_t * pd, u32 node, gasnetCommBlock_t *block);

/*
 * @brief retrieve a segment block of a remote node ID
 * If there's no block for this node, it returns null
 */
gasnetCommBlock_t *gasnetSegmentBlockGet(ocrPolicyDomain_t * pd, u32 provider);

#endif // __GASNET_SHARE_SEGMENT_H__
