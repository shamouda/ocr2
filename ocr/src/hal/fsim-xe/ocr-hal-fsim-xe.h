/**
 * @brief Macros for hardware-level primitives that the
 * rest of OCR uses. These macros are mostly related
 * to memory primitives
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */



#ifndef __OCR_HAL_FSIM_XE_H__
#define __OCR_HAL_FSIM_XE_H__

#include "ocr-types.h"
#include "xe-abi.h"


/****************************************************/
/* OCR LOW-LEVEL MACROS                             */
/****************************************************/

/**
 * @brief Perform a memory fence
 *
 * @todo Do we want to differentiate different types
 * of fences?
 */

#define hal_fence()                                     \
    do { __asm__ __volatile__("fence 0xF, B\n\t"); } while(0)


/**
 * @brief Memory copy from source to destination
 *
 * @param destination[in]    A u64 pointing to where the data is to be copied
 *                           to
 * @param source[in]         A u64 pointing to where the data is to be copied
 *                           from
 * @param size[in]           A u64 indicating the number of bytes to be copied
 * @param isBackground[in]   A u8: A zero value indicates that the call will
 *                           return only once the copy is fully complete and a
 *                           non-zero value indicates the copy may proceed
 *                           in the background. A fence will then be
 *                           required to ensure completion of the copy
 * @todo Define what behavior we want for overlapping
 * source and destination
 */
#define hal_memCopy(destination, source, size, isBackground)            \
    do { __asm__ __volatile__("dma.copy %0, %1, %2, 0, 8\n\t"           \
                              :                                         \
                              : "r" ((void *)(destination)),            \
                                "r" ((void *)(source)),                 \
                                "r" (size));                            \
        if (!isBackground) hal_fence();                                 \
    } while(0)


/**
 * @brief Memory move from source to destination. As if overlapping portions
 * were copied to a temporary array
 *
 * @param destination[in]    A u64 pointing to where the data is to be copied
 *                           to
 * @param source[in]         A u64 pointing to where the data is to be copied
 *                           from
 * @param size[in]           A u64 indicating the number of bytes to be copied
 * @param isBackground[in]   A u8: A zero value indicates that the call will
 *                           return only once the copy is fully complete and a
 *                           non-zero value indicates the copy may proceed
 *                           in the background. A fence will then be
 *                           required to ensure completion of the copy
 * source and destination
 * @todo This implementation is heavily *NOT* optimized
 */
#define hal_memMove(destination, source, size, isBackground) do {\
    u64 _source = (u64)source;                                          \
    u64 _destination = (u64)destination;                                \
    u64 count = 0;                                                      \
    if(_source != _destination) {                                       \
        if(_source < _destination) {                                    \
            for(count = size -1 ; count > 0; --count)                   \
                ((char*)_destination)[count] = ((char*)_source)[count]; \
            ((char*)_destination)[0] = ((char*)_source)[0];             \
        } else {                                                        \
            for(count = 0; count < size; ++count)                       \
                ((char*)_destination)[count] = ((char*)_source)[count]; \
        }                                                               \
    }                                                                   \
    } while(0)

/**
 * @brief Atomic swap (64 bit)
 *
 * Atomically swap:
 *
 * @param atomic        u64*: Pointer to the atomic value (location)
 * @param newValue      u64: New value to set
 *
 * @return Old value of the atomic
 */
#define hal_swap64(atomic, newValue)                                    \
    ({                                                                  \
        u64 __tmp = newValue;                                           \
        __asm__ __volatile__("xchg %0, %1, 64\n\t"                      \
                             : "+r" (__tmp)                             \
                             : "r" (atomic));                           \
        __tmp;                                                          \
    })

/**
 * @brief Compare and swap (64 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - if location is cmpValue, atomically replace with
 *       newValue and return cmpValue
 *     - if location is *not* cmpValue, return value at location
 *
 * @param atomic        u64*: Pointer to the atomic value (location)
 * @param cmpValue      u64: Expected value of the atomic
 * @param newValue      u64: Value to set if the atomic has the expected value
 *
 * @return Old value of the atomic
 */
#define hal_cmpswap64(atomic, cmpValue, newValue)                       \
    ({                                                                  \
        u64 __tmp = newValue;                                           \
        __asm__ __volatile__("cmpxchg %0, %1, %2, 64\n\t"               \
                             : "+r" (__tmp)                             \
                             : "r" (atomic),                            \
                               "r" (cmpValue));                         \
        __tmp;                                                          \
    })

/**
 * @brief Atomic add (64 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - atomically increment location by addValue
 *     - return old value (before addition)
 *
 * @param atomic    u64*: Pointer to the atomic value (location)
 * @param addValue  u64: Value to add to location
 * @return Old value of the location
 */
#define hal_xadd64(atomic, addValue)                                    \
    ({                                                                  \
        u64 __tmp;                                                      \
        __asm__ __volatile__("xaddI %0, %1, %2, 64, R\n\t"              \
                             : "=r" (__tmp)                             \
                             : "r" (atomic),                            \
                               "r" (addValue));                         \
        __tmp;                                                          \
    })

/**
 * @brief Remote atomic add (64 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - atomically increment location by addValue
 *     - no value is returned (the increment will happen "at some
 *       point")
 *
 * @param atomic    u64*: Pointer to the atomic value (location)
 * @param addValue  u64: Value to add to location
 */
#define hal_radd64(atomic, addValue)                                    \
    __asm__ __volatile__("xaddI %1, %0, %1, 64, N\n\t"                  \
                         : "r" (atomic),                                \
                           "r" (addValue));

/**
 * @brief Atomic swap (32 bit)
 *
 * Atomically swap:
 *
 * @param atomic        u32*: Pointer to the atomic value (location)
 * @param newValue      u32: New value to set
 *
 * @return Old value of the atomic
 */
#define hal_swap32(atomic, newValue)                                    \
    ({                                                                  \
        u64 __tmp = newValue;                                           \
        __asm__ __volatile__("xchg %0, %1, 32\n\t"                      \
                             : "+r" (__tmp)                             \
                             : "r" (atomic));                           \
        __tmp;                                                          \
    })

/**
 * @brief Compare and swap (32 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - if location is cmpValue, atomically replace with
 *       newValue and return cmpValue
 *     - if location is *not* cmpValue, return value at location
 *
 * @param atomic        u32*: Pointer to the atomic value (location)
 * @param cmpValue      u32: Expected value of the atomic
 * @param newValue      u32: Value to set if the atomic has the expected value
 *
 * @return Old value of the atomic
 */
#define hal_cmpswap32(atomic, cmpValue, newValue)                       \
    ({                                                                  \
        u64 __tmp = newValue;                                           \
        __asm__ __volatile__("cmpxchg %0, %1, %2, 32\n\t"               \
                             : "+r" (__tmp)                             \
                             : "r" (atomic),                            \
                               "r" (cmpValue));                         \
        __tmp;                                                          \
    })

/**
 * @brief Atomic add (32 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - atomically increment location by addValue
 *     - return old value (before addition)
 *
 * @param atomic    u32*: Pointer to the atomic value (location)
 * @param addValue  u32: Value to add to location
 * @return Old value of the location
 */
#define hal_xadd32(atomic, addValue)                                    \
    ({                                                                  \
        u64 __tmp;                                                      \
        __asm__ __volatile__("xaddI %0, %1, %2, 32, R\n\t"              \
                             : "=r" (__tmp)                             \
                             : "r" (atomic),                            \
                               "r" (addValue));                         \
        __tmp;                                                          \
    })

/**
 * @brief Remote atomic add (32 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - atomically increment location by addValue
 *     - no value is returned (the increment will happen "at some
 *       point")
 *
 * @param atomic    u32*: Pointer to the atomic value (location)
 * @param addValue  u32: Value to add to location
 */
#define hal_radd32(atomic, addValue)                                    \
    __asm__ __volatile__("xaddI %1, %0, %1, 32, N\n\t"                  \
                         : "r" (atomic),                                \
                           "r" (addValue));

/**
 * @brief Convenience function that basically implements a simple
 * lock
 *
 * This will usually be a wrapper around cmpswap32. This function
 * will block until the lock can be acquired
 *
 * @param lock      Pointer to a 32 bit value
 */
#define hal_lock32(lock)                        \
    while(hal_cmpswap32(lock, 0, 1))

/**
 * @brief Convenience function to implement a simple
 * unlock
 *
 * @param lock      Pointer to a 32 bit value
 */
#define hal_unlock32(lock)                      \
    *(u32 *)(lock) = 0

/**
 * @brief Convenience function to implement a simple
 * trylock
 *
 * @param lock      Pointer to a 32 bit value
 * @return 0 if the lock has been acquired and a non-zero
 * value if it cannot be acquired
 */
#define hal_trylock32(lock)                     \
    hal_cmpswap32(lock, 0, 1)

/**
 * @brief Abort the runtime
 *
 * Will crash the runtime
 */
#define hal_abort()                             \
    __asm__ __volatile__(".quad 0\n\t")

/**
 * @brief Exit the runtime
 *
 * This will exit the runtime more cleanly than abort
 */
#define hal_exit(arg)                           \
    __asm__ __volatile__("alarm %0\n\t" : : "L" (XE_TERMINATE_ALARM))

/**
 * @brief Pause execution
 *
 * This is used to support a primitive version of ocrWait
 * and may be deprecated in the future
 */
#define hal_pause() do {                        \
        u32 _i = 1000;                          \
        while(_i > 0) --_i;                     \
    } while(0)

/**
 * @brief Put the XE core to sleep
 *
 * Currently does nothing
 */
#define hal_sleep(id) do {                       \
    } while(0)

/**
 * @brief Wake the XE core from sleep
 *
 * Currently does nothing
 */
#define hal_wake(id) do {                         \
    } while(0)

/**
 * @brief On architectures (like TG) that
 * have different address "formats", canonicalize it
 * to the unique form
 */
#define hal_globalizeAddr(addr) ({                                      \
    u64 __dest;                                                         \
    __asm__ __volatile__("lea %0, %1\n\t" : "=r" (__dest) : "r" (addr) ); \
    __dest;                                                             \
        })

/**
 * @brief On architectures (like TG) that have
 * different address "formats", this returns the
 * smallest usable address from the global address 'addr'
 */
#define hal_localizeAddr(addr) addr

// Abstraction to do a load operation from any level of the memory hierarchy
#define GET8(temp, addr)   ((temp) = *((u8*)(addr)))
#define GET16(temp, addr)  ((temp) = *((u16*)(addr)))
#define GET32(temp, addr)  ((temp) = *((u32*)(addr)))
#define GET64(temp, addr)  ((temp) = *((u64*)(addr)))
// Abstraction to do a store operation to any level of the memory hierarchy
#define SET8(addr, value)  (*((u8*)(addr))  = (u8)(value))
#define SET16(addr, value) (*((u16*)(addr)) = (u16)(value))
#define SET32(addr, value) (*((u32*)(addr)) = (u32)(value))
#define SET64(addr, value) (*((u64*)(addr)) = (u64)(value))
#endif /* __OCR_HAL_FSIM_XE_H__ */
