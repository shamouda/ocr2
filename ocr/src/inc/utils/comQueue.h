/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __COMQUEUE_H__
#define __COMQUEUE_H__

#include "ocr-config.h"
#include "ocr-types.h"
#include "ocr-policy-domain.h"

typedef enum _comQueueStatus {
    COMQUEUE_WRITEABLE, /**< The slot is empty but reserved for writing */
    COMQUEUE_RESERVED,  /**< Slot is reserved by a writer but not readable */
    COMQUEUE_FULL,      /**< Slot is full (readable) */
    COMQUEUE_READING,   /**< Slot is being read */
    COMQUEUE_EMPTY      /**< Slot is empty (and can't be written to) */
} comQueueStatus;

// Properties for communication slots
#define COMQUEUE_FREE_PTR 0x1 /**< When the slot is emptied, the message pointer needs to be freed */
#define COMQUEUE_EMPTY_PENDING 0x2 /**< The slot is pending being emptied. This
                                    * value is mostly for verification purposes */

typedef struct _comQueueSlot_t {
    volatile u32 status; // It is a comQueueStatus but we need to cmpswap on this so using u32
    u32 properties;
    ocrPolicyMsg_t *msgPtr;
    ocrPolicyMsg_t msgBuffer;
} comQueueSlot_t;

typedef struct _comQueue_t {
    /**< Indices into the queue:
     *   - first unread is in readIdx
     *   - next slot to write is in writeIdx
     */
    volatile u32 readIdx;
    volatile u32 writeIdx;
    u32 size;                       /**< Number of elements in the queue */
    comQueueSlot_t *slots;          /**< Slots in the queue */
} comQueue_t;

/**
 * @brief Initialize the queue
 *
 * The queue and the slots need to already
 * be allocated (to leave the method of allocation
 * up to the caller). This call is not thread safe
 *
 * @param[in] queue Queue to initialize
 * @param[in] size  Queue size
 * @param[in] slots Pointer to a memory holding
 *                  at least size*sizeof(comQueueSlot_t)
 *                  bytes
 * @return 0 on success or an error code:
 *   - OCR_EINVAL: Invalid value for queue, size of slots
 */
u8 comQueueInit(comQueue_t* queue, u32 slot, comQueueSlot_t* slots);

/**
 * @brief Reserve a slot to write data in the queue
 *
 * Returns the slot ID in 'slot'. This call is
 * thread safe
 *
 * @param[in] queue Queue to modify
 * @param[out] slot Slot reserved
 * @return 0 on success and:
 *   - OCR_EAGAIN if the queue is full
 *   - OCR_ENOMEM if queue is of size 0
 */
u8 comQueueReserveSlot(comQueue_t *queue, u32 *slot);

/**
 * @brief Makes the slot 'slot' available for writing again
 *
 * This call is thread safe provided it is only
 * made with a slot that has previously been reserved
 * successfully. It allows the caller to "change his mind"
 * about needing the slot to write.
 *
 * @param[in] queue Queue to modify
 * @param[in] slot  Slot to make free again
 * @return 0 on success and:
 *   - OCR_EPERM if you do not have permissions to validate the slot (currently unused)
 */
u8 comQueueUnreserveSlot(comQueue_t *queue, u32 slot);

/**
 * @brief Makes the slot 'slot' available for reading
 *
 * This call is thread safe provided it is only
 * made with a slot that has previously been reserved
 * successfully.
 *
 * @param[in] queue Queue to modify
 * @param[in] slot  Slot to make available
 * @return 0 on success and:
 *   - OCR_EPERM if you do not have permissions to validate the slot (currently unused)
 */
u8 comQueueValidateSlot(comQueue_t *queue, u32 slot);

/**
 * @brief Gets a slot to read
 *
 * The value of the slot to read is returned in 'slot'
 * This call is thread-safe with writers but only
 * one thread may try to get a slot to read
 * at any given time (single reader). This
 * could be changed in the future
 *
 * @param[in] queue Queue to read from
 * @param[out] slot Slot that can be read
 * @return 0 on success and:
 *   - OCR_EAGAIN if no slot is readable
 *   - OCR_ENOMEM if queue is of size 0
 */
u8 comQueueReadSlot(comQueue_t *queue, u32 *slot);

/**
 * @brief Returns a slot to the EMPTY state
 *
 * The thread safety of this call is the same
 * as for comQueueReadSlot()
 *
 * @param[in] queue Queue to modify
 * @param[in] slot  Slot to mark as empty
 * The memory for the slot can be reused after this call
 * @return 0 on success and:
 *   - OCR_EPERM if you do not have permissions to empty the slot (currently unused)
 */
u8 comQueueEmptySlot(comQueue_t *queue, u32 slot);

#endif /* __COMQUEUE_H__ */


