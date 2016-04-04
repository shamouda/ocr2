/**
 * @brief Implementation of an 'simple' first-fit allocator
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

//     poolHdr_t      pool header annex      glebe
//   +--------------+----------------------+------------------------------------------------+
//   |              |                      |                                                |
//   | see notes at | see notes at         |                                                |
//   | poolHdr_t    | poolHdr_t            |                                                |
//   | typedef      | typedef              |                                                |
//   | for          | for                  |                                                |
//   | contents     | contents             |                                                |
//   |              |                      |                                                |
//   +--------------+----------------------+------------------------------------------------+
//                                         .                                                .
//                                         .                                                .
//                                         .                                                .
//                                         .    *  The glebe contains the blocks:           .
//   . . . . . . . . . . . . . . . . . . . .       ==============================           .
//   .                                                                                      .
//   .                                                                                      .
//   . used block    free block    used block        used blk  free block        used block .
//   +-------------+-------------+-----------------+---------+-----------------+------------+
//   |             |             |                 |         |                 |            |
//   |             |             |                 |         |                 |            |
//   +-------------+-------------+-----------------+---------+-----------------+------------+
//   .             .                                         .                 .
//   .             .                                         .                 .
//   .             . * Blocks have a header, space, tail     .                 .
//   .             .   =================================     .                 .
//   .             .                                         .                 .
//   . used block: . . . . . . . . . . . . . .     . . . . . . free block:     . . . . . . .
//   .                                       .     .                                       .
//   . blkHdr_t   payload                    .     . blkHdr_t   free space                 .
//   +----------+------------------------+---+     +----------+------------------------+---+
//   |          |                        |   |     |          |                        |   |
//   | see      |                        | T |     | see      |                        | T |
//   | blkHdr_t | user-visible datablock | a |     | blkHdr_t |                        | a |
//   | typedef  |                        | i |     | typedef  |                        | i |
//   |          |                        | l |     |          |                        | l |
//   +----------+------------------------+---+     +----------+------------------------+---+

#include "ocr-config.h"
#ifdef ENABLE_ALLOCATOR_SIMPLE
#include "ocr-hal.h"
#include "debug.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "simple-allocator.h"
#include "allocator/allocator-all.h"
#include "ocr-mem-platform.h"

#define ALIGNMENT 8LL

#define DEBUG_TYPE ALLOCATOR

//helpful to run this code on x86
//#define DPRINTF(X, ...)    printf(__VA_ARGS__)
//#define ASSERT  assert

// start of simple_alloc core part

// Block header layout (old blkHeader_t)
//
// * free block
//
//  x[0]  x[1]  x[2]  x[3]  x[4]
// +-----+-----+-----+-----+-----+-----------+-----+
// |HEAD |INFO1|INFO2|NEXT |PREV |    ...    |TAIL |
// |     |     |     |     |     |           |     |
// +-----+-----+-----+-----+-----+-----------+-----+
//
// * allocated block
//
//  x[0]  x[1]  x[2]
// +-----+-----+-----+-----------------------+-----+
// |HEAD |INFO1|INFO2|          ...          |TAIL |
// |     |     |     |      user-visible     |     |
// +-----+-----+-----+-----------------------+-----+
//
// HEAD and TAIL contains full size including HEAD/INFOs/TAIL in bytes and HEAD also has MARK in its higher 16 bits.
// HEAD's bit0 is 1 for allocated block, or 0 for free block.
// i.e.  HEAD == ( MARK | size | bit0 )  , and   TAIL == size
// PEER_LEFT and PEER_RIGHT helps access neightbor blocks.
// NEXT and PREV is valid for free blocks and basically forms linked list for free list.
// INFO1 contains a pointer to the pool header which it belongs to. i.e. poolHdr_t
// INFO2 contains
// These INFOs are only for TG arch, not for x86.

// arbitrary value 0xfeef. This mark helps detect invalid ptr or double free.
#define MARK                    (0xfeefL << 48)
#define ALIGNMENT_MASK          (ALIGNMENT-1)
#define HEAD(X)                 ((X)[0])
#define TAIL(X,SIZE)            (*(u64 *)((u8 *)(X)+(SIZE)-sizeof(u64)))
#define PEER_LEFT(X)            ((X)[-( (X)[-1] >> 3 )])
#define PEER_RIGHT(X,SIZE)      (*(u64 *)((u8 *)(X)+(SIZE)))
#define GET_MARK(X)             ((((1UL << 16)-1) << 48) & (X))
#define GET_SIZE(X)             ((((1UL << 48)-1-3)    ) & (X))
#define GET_BIT0(X)             ((                   1) & (X))
#define GET_BIT1(X)             ((                   2) & (X))

// Additional INFOs
#define INFO1(X)                ((X)[1])
#define INFO2(X)                ((X)[2])
// in case of free block, prev/next is used to get linked into free list
// PREV/NEXT exists only on free blocks.. i.e. not on allocated blocks,
// because only free blocks go into free list.
#define NEXT(X)                 ((X)[3])
#define PREV(X)                 ((X)[4])
// Conversion between user-provided address and block header
#define HEAD_TO_USER(X)         ((X)+3)
#define USER_TO_HEAD(X)         (((u64 *)(X))-3)
// At the moment, we have alloc overhead of 4 u64 to user payload (HEAD,INFO1,INFO2,and TAIL)
#define ALLOC_OVERHEAD          (4*sizeof(u64))
// Minimum allocatable size from user's perspective is 2*u64 for prev/next for free list
// Thus, the minimum block size is MINIMUM_SIZE_USER + ALLOC_OVERHEAD
#define MINIMUM_SIZE_USER       (2*sizeof(u64))
#define MINIMUM_SIZE            (MINIMUM_SIZE_USER + ALLOC_OVERHEAD)

// VALGRIND SUPPORT
//
// VALGRIND_MEMPOOL_ALLOC: If the pool was created with the is_zeroed argument set, Memcheck will mark the chunk as DEFINED, otherwise Memcheck will mark the chunk as UNDEFINED.
// VALGRIND_MEMPOOL_FREE:  Memcheck will mark the chunk associated with addr as NOACCESS, and delete its record of the chunk's existence.
// VALGRIND_MAKE_MEM_NOACCESS, VALGRIND_MAKE_MEM_UNDEFINED and VALGRIND_MAKE_MEM_DEFINED. These mark address ranges as completely inaccessible, accessible but containing undefined data, and accessible and containing defined data, respectively. They return -1, when run on Valgrind and 0 otherwise.

// VALGRIND_CREATE_MEMPOOL :
// VALGRIND_DESTROY_MEMPOOL :    // Memcheck resets the redzones of any live chunks in the pool to NOACCESS.

#ifdef ENABLE_VALGRIND
#include <valgrind/memcheck.h>
#define VALGRIND_POOL_OPEN(X)   VALGRIND_MAKE_MEM_DEFINED((X) , sizeof(pool_t))
#define VALGRIND_POOL_CLOSE(X)  VALGRIND_MAKE_MEM_NOACCESS((X), sizeof(pool_t))
#define VALGRIND_CHUNK_OPEN(X)  do {VALGRIND_MAKE_MEM_DEFINED(&HEAD(X), 3*sizeof(u64)); if (GET_BIT0(HEAD(X)) == 0) VALGRIND_MAKE_MEM_DEFINED((u64 *)(X)+3 , 2*sizeof(u64));  VALGRIND_MAKE_MEM_DEFINED(&TAIL((X), GET_SIZE(HEAD(X))), sizeof(u64)); } while(0)
#define VALGRIND_CHUNK_OPEN_INIT(X, Y)   do {VALGRIND_MAKE_MEM_DEFINED(&HEAD(X), 5*sizeof(u64)); VALGRIND_MAKE_MEM_DEFINED(&TAIL((X), (Y)), sizeof(u64)); } while(0)
#define VALGRIND_CHUNK_OPEN_LEFT(X)     VALGRIND_MAKE_MEM_DEFINED(&(X)[-1], sizeof(u64));
#define VALGRIND_CHUNK_OPEN_COND(X, Y)  if ((X) != (Y)) VALGRIND_CHUNK_OPEN(Y);
#define VALGRIND_CHUNK_CLOSE(X)         do {VALGRIND_MAKE_MEM_NOACCESS((X) , GET_SIZE(HEAD(X))); } while(0)
#define VALGRIND_CHUNK_CLOSE_COND(X, Y)  if ((X) != (Y)) VALGRIND_CHUNK_CLOSE(Y);
#else
#define VALGRIND_POOL_OPEN(X)
#define VALGRIND_POOL_CLOSE(X)
#define VALGRIND_CHUNK_OPEN(X)
#define VALGRIND_CHUNK_OPEN_INIT(X, Y)
#define VALGRIND_CHUNK_OPEN_LEFT(X)
#define VALGRIND_CHUNK_OPEN_COND(X, Y)
#define VALGRIND_CHUNK_CLOSE(X)
#define VALGRIND_CHUNK_CLOSE_COND(X, Y)
#endif

static void simpleTest(u64 start, u64 size)
{
#if 1
    // boundary check code for sanity check.
    // This helps early detection of malformed addresses.
    do {
        DPRINTF(DEBUG_LVL_INFO, "simpleBegin : pool range [0x%"PRIx64" - 0x%"PRIx64")\n", start, start+size);

        u8 *p = (u8 *)((start + size - 128)&(~0x7UL));      // at least 128 bytes
        u8 *q = (u8 *)(start + size);
        u64 size = q-p;

        while (size >= 8) {
            *((u64 *)p) = 0xdeadbeef0000dead;   // just random value
            p+=8; size -= 8;
        }
        while (size) {
            *p = '0';
            p++; size--;
        }
    } while(0);
    DPRINTF(DEBUG_LVL_INFO, "simpleBegin : simple test passed\n");
#endif
}

static void simpleInit(pool_t *pool, u64 size)
{
    u8 *p = (u8 *)pool;
    u64 *q;
    q = (u64 *)(p + sizeof(pool_t));
    ASSERT(((u64)q & ALIGNMENT_MASK) == 0);
    ASSERT((sizeof(pool_t) & ALIGNMENT_MASK) == 0);
    ASSERT((size & ALIGNMENT_MASK) == 0);
    size = size - sizeof(pool_t);

    // pool->lock and pool->inited is already 0 at startup (on x86, it's done at mallocBegin())
#ifdef ENABLE_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(&(pool->lock), sizeof(pool->lock));
    hal_lock32(&(pool->lock));
    VALGRIND_MAKE_MEM_NOACCESS(&(pool->lock), sizeof(pool->lock));
#else
    hal_lock32(&(pool->lock));
#endif

#ifdef ENABLE_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(&(pool->inited), sizeof(pool->inited));
#endif
    if (!(pool->inited)) {
        simpleTest((u64)pool, size+sizeof(pool_t));
        HEAD(q) = MARK | size;
        NEXT(q) = 0;
        PREV(q) = 0;
        TAIL(q,size) = size;
        pool->pool_start = (u64 *)q;
        pool->pool_end = (u64 *)(p+size+sizeof(pool_t));
        pool->freelist = (u64 *)q;
        DPRINTF(DEBUG_LVL_INFO, "init'ed pool %p, avail %"PRId64" bytes , sizeof(pool_t) = %zd\n", pool, size, sizeof(pool_t));
        pool->inited = 1;
#ifdef ENABLE_VALGRIND
        VALGRIND_CREATE_MEMPOOL(p, 0, 1);  // BUG #600: Mempool needs to be destroyed
        VALGRIND_MAKE_MEM_NOACCESS(p, size+sizeof(pool_t));
#endif
    } else {
        DPRINTF(DEBUG_LVL_INFO, "init skip for pool %p\n", pool);
    }
#ifdef ENABLE_VALGRIND
    VALGRIND_MAKE_MEM_NOACCESS(&(pool->inited), sizeof(pool->inited));
#endif

#ifdef ENABLE_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(&(pool->lock), sizeof(pool->lock));
    hal_unlock32(&(pool->lock));
    VALGRIND_MAKE_MEM_NOACCESS(&(pool->lock), sizeof(pool->lock));
#else
    hal_unlock32(&(pool->lock));
#endif
}

#if 0 // debugging code. disabled to prevent warning messages
static void simplePrint(pool_t *pool)
{
    // debugging code
    u64 *p = pool->freelist;
    u64 *next;
    u64 size = 0, count = 0;

    if (p == NULL) {
        DPRINTF(DEBUG_LVL_VERB, "[free list] empty.\n");
        return;
    }

    do {
        count++;
        size += GET_SIZE(HEAD(p));
        ASSERT(GET_BIT0(HEAD(p)) == 0);
        //printf("%p [%"PRId32"]: size %"PRId32" next %"PRId32" prev %"PRId32" \n", p, p-(pool->pool_start) , HEAD(p), NEXT(p), PREV(p) );
        next = NEXT(p) + pool->pool_start;
        if (next == pool->freelist)
            break;
        p = next;
    } while(1);
    DPRINTF(DEBUG_LVL_VERB, "[free list] count %"PRId64"  size %"PRId64" (%"PRIx64")\n", count, size, size);
}

static void simpleWalk(pool_t *pool)
{
    // debugging code.
    u64 *p = pool->pool_start;
    u64 size;
    u64 count = 0;

    do {
        if (  GET_MARK(HEAD(p)) != MARK ) {
            DPRINTF(DEBUG_LVL_WARN, "[walk] mark not found %p\n", p);
            break;
        }
        size = GET_SIZE(HEAD(p));
        ASSERT((size & ALIGNMENT_MASK) == 0);
        if (TAIL(p, size) != size) {
            DPRINTF(DEBUG_LVL_WARN, "[walk] two sizes doesn't match. p=%p  size=%"PRId64" , tail=%"PRId64"\n", p, size, TAIL(p,size));
            break;
        }

        count++;
        p = &PEER_RIGHT(p, size);
        if (p == pool->pool_end)
            break;
        if (p > pool->pool_end) {
            DPRINTF(DEBUG_LVL_WARN, "[walk] p %p > end %p\n", p, pool->pool_end);
            break;
        }
    } while(1);
    DPRINTF(DEBUG_LVL_VERB, "[walk] count %"PRId64"\n", count);
}
#endif

static void simpleInsertFree(pool_t *pool,u64 *p, u64 size)
{
    VALGRIND_CHUNK_OPEN_INIT(p, size);
    ASSERT((size & ALIGNMENT_MASK) == 0);
    HEAD(p) = MARK | size;
    TAIL(p, size) = size;

    VALGRIND_POOL_OPEN(pool);
    if (pool->freelist == NULL) {
        NEXT(p) = p-(pool->pool_start);
        PREV(p) = p-(pool->pool_start);
        pool->freelist = p;
    } else {
        u64 *q = pool->freelist;
        VALGRIND_CHUNK_OPEN(q);
        u64 *r = PREV(q)+(pool->pool_start);
        VALGRIND_CHUNK_OPEN_COND(q, r);
        NEXT(r) = p-(pool->pool_start);
        VALGRIND_CHUNK_CLOSE_COND(q, r);
        NEXT(p) = q-(pool->pool_start);
        PREV(p) = PREV(q);
        PREV(q) = p-(pool->pool_start);
        VALGRIND_CHUNK_CLOSE(q);
    }
    //simplePrint(pool);
    VALGRIND_POOL_CLOSE(pool);
    VALGRIND_CHUNK_CLOSE(p);
}

static void simpleSplitFree(pool_t *pool,u64 *p, u64 size)
{
    VALGRIND_CHUNK_OPEN_INIT(p, size);
    u64 remain = GET_SIZE(HEAD(p)) - size;
    ASSERT( remain < GET_SIZE(HEAD(p)) );
    ASSERT((size & ALIGNMENT_MASK) == 0);
    // make sure the remaining block is bigger than minimum size
    if (remain >= MINIMUM_SIZE) {
        HEAD(p) = MARK | size | 0x1;    // in-use mark
        TAIL(p, size) = size;
        VALGRIND_CHUNK_CLOSE(p);
        simpleInsertFree(pool, &PEER_RIGHT(p, size), remain);
    } else {
        HEAD(p) |= 0x1;         // in-use mark
        VALGRIND_CHUNK_CLOSE(p);
    }
}

static void simpleDeleteFree(pool_t *pool,u64 *p)
{
    VALGRIND_POOL_OPEN(pool);
    VALGRIND_CHUNK_OPEN(p);
    u64 *next = NEXT(p) + pool->pool_start;
    u64 *prev = PREV(p) + pool->pool_start;
    ASSERT(GET_BIT0(HEAD(p)) == 0);

    if (next == p) {
        pool->freelist = NULL;
        VALGRIND_CHUNK_CLOSE(p);
        VALGRIND_POOL_CLOSE(pool);
        return;
    }
    VALGRIND_CHUNK_OPEN(next);
    VALGRIND_CHUNK_OPEN_COND(next, prev);

    NEXT(prev) = NEXT(p);
    PREV(next) = PREV(p);
    if (p == pool->freelist) {
        pool->freelist = next;
    }
    VALGRIND_CHUNK_CLOSE(p);
    VALGRIND_CHUNK_CLOSE(next);
    VALGRIND_CHUNK_CLOSE_COND(next, prev);
    VALGRIND_POOL_CLOSE(pool);
}

static void *simpleMalloc(pool_t *pool,u64 size, struct _ocrPolicyDomain_t *pd)
{
    VALGRIND_POOL_OPEN(pool);
    hal_lock32(&(pool->lock));
    u64 *p = pool->freelist;
    VALGRIND_POOL_CLOSE(pool);
    u64 *next;
#ifdef ENABLE_VALGRIND
    u64 size_orig = size;
#endif
    DPRINTF(DEBUG_LVL_VERB, "before malloc size %"PRId64":\n", size);
    //simplePrint(pool);
    if (p == NULL)
        goto exit_fail;

    // This guarantees that the block will be able to embed NEXT/PREV
    // in case that it's freed in the future.
    if (size < MINIMUM_SIZE_USER)  // should be bigger than minimum size
        size = MINIMUM_SIZE_USER;
    size = (size + ALIGNMENT_MASK)&(~ALIGNMENT_MASK);   // ceiling
    do {
        VALGRIND_CHUNK_OPEN(p);
        if (GET_SIZE(HEAD(p)) >= size + ALLOC_OVERHEAD) {
            VALGRIND_CHUNK_CLOSE(p);
            simpleDeleteFree(pool, p);
            simpleSplitFree(pool, p, size + ALLOC_OVERHEAD);

            void *ret = HEAD_TO_USER(p);
            VALGRIND_CHUNK_OPEN(p);
            INFO1(p) = (u64)addrGlobalizeOnTG((void *)pool, pd);   // old : INFO1(p) = (u64)pool;
            INFO2(p) = (u64)addrGlobalizeOnTG((void *)ret, pd);    // old : INFO2(p) = (u64)ret;

            ASSERT((*(u8 *)(&INFO2(p)) & POOL_HEADER_TYPE_MASK) == 0);
            *(u8 *)(&INFO2(p)) |= allocatorSimple_id;

            ASSERT_BLOCK_BEGIN((*(u8 *)(&INFO2(p)) & POOL_HEADER_TYPE_MASK) == allocatorSimple_id)
            DPRINTF(DEBUG_LVL_WARN, "SimpleAlloc : id != allocatorSimple_id \n");
            ASSERT_BLOCK_END
            VALGRIND_CHUNK_CLOSE(p);
            VALGRIND_POOL_OPEN(pool);
            hal_unlock32(&(pool->lock));
            VALGRIND_POOL_CLOSE(pool);
#ifdef ENABLE_VALGRIND
            VALGRIND_MEMPOOL_ALLOC(pool, ret, size_orig);
//            printf("mempool_alloc, pool %p , ret %p\n", pool, ret);
#endif
            return ret;
        }
        next = NEXT(p) + pool->pool_start;
        VALGRIND_CHUNK_CLOSE(p);
        if (next == pool->freelist)
            break;
        p = next;
    } while(1);
exit_fail:
    //DPRINTF(DEBUG_LVL_INFO, "OUT OF HEAP! malloc failed\n");
    VALGRIND_POOL_OPEN(pool);
    hal_unlock32(&(pool->lock));
    VALGRIND_POOL_CLOSE(pool);
    return NULL;
}

void simpleFree(void *p)
{
    if (p == NULL)
        return;
    u64 *q = USER_TO_HEAD(p);
    VALGRIND_CHUNK_OPEN(q);
    pool_t *pool = (pool_t *)INFO1(q);
    ASSERT_BLOCK_BEGIN (  GET_MARK(HEAD(q)) == MARK )
    DPRINTF(DEBUG_LVL_WARN, "SimpleAlloc : free: cannot find mark. Probably wrong address is passed to free()? %p\n", p);
    ASSERT_BLOCK_END
    VALGRIND_CHUNK_CLOSE(q);
#ifdef ENABLE_VALGRIND
    VALGRIND_MEMPOOL_FREE(pool, p);
#endif
    VALGRIND_POOL_OPEN(pool);
    u64 start = (u64)pool->pool_start;
    u64 end   = (u64)pool->pool_end;
    hal_lock32(&(pool->lock));
    VALGRIND_POOL_CLOSE(pool);

    ASSERT((*(u8 *)(&INFO2(q)) & POOL_HEADER_TYPE_MASK) == allocatorSimple_id);
    *(u8 *)(&INFO2(q)) &= ~POOL_HEADER_TYPE_MASK;

    q = USER_TO_HEAD(INFO2(q)); // For TG. no effects on x86

    ASSERT_BLOCK_BEGIN ( GET_MARK(HEAD(q)) == MARK )
    DPRINTF(DEBUG_LVL_WARN, "SimpleAlloc : free: mark not found %p\n", p);
    ASSERT_BLOCK_END

    ASSERT_BLOCK_BEGIN ( GET_BIT0(HEAD(q)) )
    DPRINTF(DEBUG_LVL_WARN, "SimpleAlloc : free not-allocated block? double free? p=%p\n", p);
    ASSERT_BLOCK_END

    u64 size = GET_SIZE(HEAD(q));
    ASSERT_BLOCK_BEGIN(TAIL(q, size) == size)
    DPRINTF(DEBUG_LVL_WARN, "SimpleAlloc : two sizes doesn't match. p=%p\n", p);
    ASSERT_BLOCK_END

    //DPRINTF(DEBUG_LVL_VERB, "before free : pool = %p, addr=%p\n", pool, INFO2(q));
    //simplePrint(pool);

    u64 *peer_right = &PEER_RIGHT(q, size);
    ASSERT_BLOCK_BEGIN (!((u64)peer_right > end))
    DPRINTF(DEBUG_LVL_WARN, "SimpleAlloc : PEER_RIGHT address %p is above the heap area\n", peer_right);
    ASSERT_BLOCK_END

    ASSERT_BLOCK_BEGIN (!((u64)&HEAD(q) < start))
    DPRINTF(DEBUG_LVL_WARN, "SimpleAlloc : address %p is below the heap area\n", &HEAD(q));
    ASSERT_BLOCK_END
    VALGRIND_CHUNK_CLOSE(q);

    if ((u64)peer_right != end) {
        VALGRIND_CHUNK_OPEN(peer_right);

        ASSERT_BLOCK_BEGIN (  GET_MARK(HEAD(peer_right)) == MARK )
        DPRINTF(DEBUG_LVL_WARN, "SimpleAlloc : right neighbor's mark not found %p\n", p);
        ASSERT_BLOCK_END

        if (!(GET_BIT0(HEAD(peer_right)))) {     // right block is free?
            size += GET_SIZE(HEAD(peer_right));
            VALGRIND_CHUNK_CLOSE(peer_right);
            simpleDeleteFree(pool, peer_right);
            VALGRIND_CHUNK_OPEN(peer_right);
            HEAD(peer_right) = 0;    // erase header (and mark)
        }
        VALGRIND_CHUNK_CLOSE(peer_right);
    }
    VALGRIND_CHUNK_OPEN(q);
    if ((u64)&HEAD(q) != start) {
        VALGRIND_CHUNK_OPEN_LEFT(q);
        u64 *peer_left = &PEER_LEFT(q);
        VALGRIND_CHUNK_CLOSE(q);
        // just omit chunk_close_left()
        VALGRIND_CHUNK_OPEN(peer_left);

        ASSERT_BLOCK_BEGIN ( GET_MARK(HEAD(peer_left)) == MARK )
        DPRINTF(DEBUG_LVL_WARN, "SimpleAlloc : left neighbor's mark not found %p\n", p);
        ASSERT_BLOCK_END

        if (!(GET_BIT0(HEAD(peer_left)))) {      // left block is free?
            size += GET_SIZE(HEAD(peer_left));
            VALGRIND_CHUNK_CLOSE(peer_left);
            simpleDeleteFree(pool, peer_left);
            VALGRIND_CHUNK_OPEN(peer_left);
            HEAD(q) = 0;    // erase header (and mark)
            q = peer_left;
        }
        VALGRIND_CHUNK_CLOSE(peer_left);
    } else {
        VALGRIND_CHUNK_CLOSE(q);
    }
    simpleInsertFree(pool, &HEAD(q), size);
    VALGRIND_POOL_OPEN(pool);
    hal_unlock32(&(pool->lock));
    VALGRIND_POOL_CLOSE(pool);
#ifdef ENABLE_VALGRIND
    VALGRIND_MEMPOOL_FREE(pool, p);
//    printf("mempool_free, pool %p , p %p\n", pool, p);
#endif
}

// end of simple_alloc core part

void simpleDestruct(ocrAllocator_t *self) {
    DPRINTF(DEBUG_LVL_VERB, "Entered simpleDesctruct (This is x86 only?) on allocator 0x%"PRIx64"\n", (u64) self);
    ASSERT(self->memoryCount == 1);
    self->memories[0]->fcts.destruct(self->memories[0]);
    runtimeChunkFree((u64)self->memories, PERSISTENT_CHUNK);
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
    DPRINTF(DEBUG_LVL_INFO, "Leaving simpleDestruct on allocator 0x%"PRIx64" (free)\n", (u64) self);
}

u8 simpleSwitchRunlevel(ocrAllocator_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                        phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {

    u8 toReturn = 0;
    // This is an inert module, we do not handle callbacks (caller needs to wait on us)
    ASSERT(callback == NULL);

    // Verify properties for this call
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));

    ASSERT(self->memoryCount == 1);
    // Call the runlevel change on the underlying memory
    // On tear-down, we do it *AFTER* we do stuff because otherwise our mem-platform goes away
    if(properties & RL_BRING_UP)
        toReturn |= self->memories[0]->fcts.switchRunlevel(self->memories[0], PD, runlevel, phase, properties,
                                                           NULL, 0);
    switch(runlevel) {
    case RL_CONFIG_PARSE:
    {
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
        break;
    }
    case RL_NETWORK_OK:
        // Nothing
        break;
    case RL_PD_OK:
        if(properties & RL_BRING_UP) {
            // We can now set our PD (before this, we couldn't because
            // "our" PD might not have been started
            self->pd = PD;
        }
        break;
    case RL_MEMORY_OK:
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_MEMORY_OK, phase)) {
            ocrAllocatorSimple_t *rself = (ocrAllocatorSimple_t*)self;
            u64 poolAddr = 0;
            DPRINTF(DEBUG_LVL_INFO, "simple bring up: poolsize 0x%"PRIx64", level %"PRIu64"\n",
                    rself->poolSize, self->memories[0]->level);
            RESULT_ASSERT(self->memories[0]->fcts.chunkAndTag(
                              self->memories[0], &poolAddr, rself->poolSize,
                              USER_FREE_TAG, USER_USED_TAG), ==, 0);
            rself->poolAddr = poolAddr;
            DPRINTF(DEBUG_LVL_INFO, "simple bring up : 0x%"PRIx64"\n", poolAddr);

            // Adjust alignment if required
            u64 fiddlyBits = ((u64) rself->poolAddr) & (ALIGNMENT - 1LL);
            if (fiddlyBits == 0) {
                rself->poolStorageOffset = 0;
            } else {
                rself->poolStorageOffset = ALIGNMENT - fiddlyBits;
                rself->poolAddr += rself->poolStorageOffset;
                rself->poolSize -= rself->poolStorageOffset;
            }
            rself->poolStorageSuffix = rself->poolSize & (ALIGNMENT-1LL);
            rself->poolSize &= ~(ALIGNMENT-1LL);

            DPRINTF(DEBUG_LVL_VERB,
                    "SIMPLE Allocator @ %p got pool at address 0x%"PRIx64" of size 0x%"PRIx64" (%"PRId64"), offset from storage addr by %"PRId64"\n",
                    rself, rself->poolAddr, (u64) (rself->poolSize),
                    (u64)(rself->poolSize), (u64) (rself->poolStorageOffset));

            ASSERT(self->memories[0]->memories[0]->startAddr /* startAddr of the memory that memplatform allocated. (for x86, at mallocBegin()) */
                   + MEM_PLATFORM_ZEROED_AREA_SIZE >= /* Add the size of zero-ed area (for x86, at mallocBegin()), then this should be greater than */
                   rself->poolAddr + sizeof(pool_t) /* the end of pool_t, so this ensures zero'ed rangeTracker,pad,pool_t */ );
            simpleInit( (pool_t *)addrGlobalizeOnTG((void *)rself->poolAddr, PD), rself->poolSize);
        } else if((properties & RL_TEAR_DOWN) && RL_IS_LAST_PHASE_DOWN(PD, RL_MEMORY_OK, phase)) {
            ocrAllocatorSimple_t * rself = (ocrAllocatorSimple_t *) self;
            RESULT_ASSERT(self->memories[0]->fcts.tag(
                              rself->base.memories[0],
                              rself->poolAddr - rself->poolStorageOffset,
                              rself->poolAddr + rself->poolSize + rself->poolStorageSuffix,
                              USER_FREE_TAG), ==, 0);
        }
        break;
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
        if(properties & RL_BRING_UP) {
            if(RL_IS_FIRST_PHASE_UP(PD, RL_COMPUTE_OK, phase)) {
                // We get a GUID for ourself
                guidify(self->pd, (u64)self, &(self->fguid), OCR_GUID_ALLOCATOR);
            }
        } else {
            // Tear-down
            if(RL_IS_LAST_PHASE_DOWN(PD, RL_COMPUTE_OK, phase)) {
                PD_MSG_STACK(msg);
                getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_DESTROY
                msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
                PD_MSG_FIELD_I(guid) = self->fguid;
                PD_MSG_FIELD_I(properties) = 0;
                toReturn |= self->pd->fcts.processMessage(self->pd, &msg, false);
                self->fguid.guid = NULL_GUID;
#undef PD_MSG
#undef PD_TYPE
            }
        }
        break;
    case RL_USER_OK:
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }
    if(properties & RL_TEAR_DOWN)
        toReturn |= self->memories[0]->fcts.switchRunlevel(self->memories[0], PD, runlevel, phase, properties,
                                                           NULL, 0);
    return toReturn;
}

void* simpleAllocate(
    ocrAllocator_t *self,   // Allocator to attempt block allocation
    u64 size,               // Size of desired block, in bytes
    u64 hints) {            // Allocator-dependent hints; SIMPLE supports reduced contention

    ocrAllocatorSimple_t * rself = (ocrAllocatorSimple_t *) self;
    void *ret = simpleMalloc((pool_t *)rself->poolAddr, size, self->pd);
    DPRINTF(DEBUG_LVL_VERB, "simpleAllocate called, ret %p from PoolAddr 0x%"PRIx64"\n", ret, rself->poolAddr);
    return ret;
}
void simpleDeallocate(void* address) {
    DPRINTF(DEBUG_LVL_VERB, "simpleDeallocate called, %p\n", address);
    simpleFree(address);
}
void* simpleReallocate(
    ocrAllocator_t *self,   // Allocator to attempt block allocation
    void * pCurrBlkPayload, // Address of existing block.  (NOT necessarily allocated to this Allocator instance, nor even in an allocator of this type.)
    u64 size,               // Size of desired block, in bytes
    u64 hints) {            // Allocator-dependent hints; SIMPLE supports reduced contention
    ASSERT(0);
    return 0;
}

/******************************************************/
/* OCR ALLOCATOR SIMPLE FACTORY                         */
/******************************************************/

// Method to create the SIMPLE allocator
ocrAllocator_t * newAllocatorSimple(ocrAllocatorFactory_t * factory, ocrParamList_t *perInstance) {

    ocrAllocatorSimple_t *result = (ocrAllocatorSimple_t*)
        runtimeChunkAlloc(sizeof(ocrAllocatorSimple_t), PERSISTENT_CHUNK);
    ocrAllocator_t * base = (ocrAllocator_t *) result;
    factory->initialize(factory, base, perInstance);
    return (ocrAllocator_t *) result;
}
void initializeAllocatorSimple(ocrAllocatorFactory_t * factory, ocrAllocator_t * self, ocrParamList_t * perInstance) {
    initializeAllocatorOcr(factory, self, perInstance);

    ocrAllocatorSimple_t *derived = (ocrAllocatorSimple_t *)self;
    paramListAllocatorSimple_t *perInstanceReal = (paramListAllocatorSimple_t*)perInstance;

    derived->poolAddr          = 0ULL;
    derived->poolSize          = perInstanceReal->base.size;
    derived->poolStorageOffset = 0;
    derived->poolStorageSuffix = 0;
}

static void destructAllocatorFactorySimple(ocrAllocatorFactory_t * factory) {
    DPRINTF(DEBUG_LVL_VERB, "destructSimple called. (This is x86 only?) free %p\n", factory);
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrAllocatorFactory_t * newAllocatorFactorySimple(ocrParamList_t *perType) {
    ocrAllocatorFactory_t* base = (ocrAllocatorFactory_t*)
        runtimeChunkAlloc(sizeof(ocrAllocatorFactorySimple_t), NONPERSISTENT_CHUNK);
    ASSERT(base);
    DPRINTF(DEBUG_LVL_VERB,
        "newAllocatorFactorySimple called, (This is x86 only?) alloc %p (Q: who free this?)\n", base);
    base->instantiate = &newAllocatorSimple;
    base->initialize = &initializeAllocatorSimple;
    base->destruct = &destructAllocatorFactorySimple;
    base->allocFcts.destruct = FUNC_ADDR(void (*)(ocrAllocator_t*), simpleDestruct);
    base->allocFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrAllocator_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                      phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), simpleSwitchRunlevel);
    base->allocFcts.allocate = FUNC_ADDR(void* (*)(ocrAllocator_t*, u64, u64), simpleAllocate);
    //base->allocFcts.free = FUNC_ADDR(void (*)(void*), simpleDeallocate);
    base->allocFcts.reallocate = FUNC_ADDR(void* (*)(ocrAllocator_t*, void*, u64), simpleReallocate);
    return base;
}

#endif /* ENABLE_ALLOCATOR_SIMPLE */
