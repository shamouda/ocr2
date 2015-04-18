#ifndef __GASNET_MESSAGE__
#define __GASNET_MESSAGE__

#include "ocr-policy-domain.h"

/*
 * @brief pushing a message into the database.
 *
 * The caller has to check the return of this function.
 * If the returns is NULL, nothing to be done,
 * but if the returns is not NULL (a memory address), the caller needs
 * to use the address to push into the incoming queue.
 *
 * @param pd : policy domain of the owner of this message. We use policy domain just
 *             for the memory allocation only.
 * @param messageID: a unique ID of the big message
 * @param message: the medium message
 * @param size: the size of this medium message
 * @param position: the position of the message in the big message
 * @param tot_msg_size: the total size of the big message
 *
 * @return the (complete) message if all messages have been completed, NULL otherwise
 */
void * gasnet_message_push(ocrPolicyDomain_t* pd, int messageID, void *message, int size,
                           int position, int tot_parts, int tot_msg_size);

/*
 * @brief Remove a message from the database
 *
 * @param messageID : the ID of the message to be removed
 */
void gasnet_message_pop(ocrPolicyDomain_t* pd, int messageID);

#endif /* __GASNET_MESSAGE__ */
