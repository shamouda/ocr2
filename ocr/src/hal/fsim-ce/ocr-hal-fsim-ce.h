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



#ifndef __OCR_HAL_FSIM_CE_H__
#define __OCR_HAL_FSIM_CE_H__

#include "ocr-types.h"

#include "mmio-table.h"
#include "tg-mmio.h"
#include "fsim-api.h"

/****************************************************/
/* OCR LOW-LEVEL MACROS                             */
/****************************************************/

// FIXME TG
//#warning HAL layer for CE needs to be reworked

/**
 * @brief Perform a memory fence
 *
 * @todo Do we want to differentiate different types
 * of fences?
 */
#define hal_fence() tg_fence_fbm()

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
#define hal_memCopy(dst, src, n, isBackground)                          \
    ({                                                                  \
        tg_dma_copyregion_async((void*)dst, (void*)src, n);             \
        if(!isBackground) tg_fence_b();                                 \
        n;                                                              \
    })


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
#define hal_memMove(destination, source, size, isBackground) do {       \
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
        u64 __tmp = tg_xchg64((u64*)atomic, newValue);                  \
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
        u64 __tmp = tg_cmpxchg64((u64*)atomic, cmpValue, newValue);     \
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
        u64 __tmp = tg_xadd64((u64*)atomic, addValue);                  \
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
#define hal_radd64(atomic, addValue) hal_xadd64(atomic, addValue)

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
        u32 __tmp = tg_xchg32((u32*)atomic, newValue);                  \
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
        u32 __tmp = tg_cmpxchg32((u32*)atomic, cmpValue, newValue);     \
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
        u32 __tmp = tg_xadd32((u32*)atomic, addValue);                  \
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
#define hal_radd32(atomic, addValue) hal_xadd32(atomic, addValue)

/**
 * @brief Convenience function that basically implements a simple
 * lock
 *
 * This will usually be a wrapper around cmpswap32. This function
 * will block until the lock can be acquired
 *
 * @param lock      Pointer to a 32 bit value
 */
#define hal_lock32(lock)                                         \
    do {                                                         \
        while(tg_cmpxchg32((u32*)lock, 0, 1) != 0) ;             \
    } while(0)

/**
 * @brief Convenience function to implement a simple
 * unlock
 *
 * @param lock      Pointer to a 32 bit value
 */
#define hal_unlock32(lock)                      \
    ({                                          \
        *(volatile u32 *)(lock) = 0;            \
    })

/**
 * @brief Convenience function to implement a simple
 * trylock
 *
 * @param lock      Pointer to a 32 bit value
 * @return 0 if the lock has been acquired and a non-zero
 * value if it cannot be acquired
 */
#define hal_trylock32(lock) tg_cmpxchg32((u32*)lock, 0, 1)

/**
 * @brief Abort the runtime
 *
 * Will crash the runtime
 */
#define hal_abort()                                             \
    __asm__ __volatile__("int %0\n\t"                           \
                         : : "i" (INT_CRASHNBURN))

/**
 * @brief Exit the runtime
 *
 * This will exit the runtime more cleanly than abort
 */
#define hal_exit(arg)                                           \
    __asm__ __volatile__("int %0\n\t"                           \
                         : : "i" (INT_QEMU_SHUTDOWN))

/**
 * @brief Pause execution
 *
 * This is used to support a primitive version of ocrWait
 * and may be deprecated in the future
 */
#define hal_pause() do {                                        \
        /* Works because there is a periodic timer interupt */  \
        __asm__ __volatile__("sti;hlt;");                       \
    } while(0)

/**
 * @brief Put the XE core to sleep
 *
 * This is used by CE to put an XE core in its block to sleep
 * @warning 'id' is the agent ID (ID_AGENT_XE0 ... ID_AGENT_XE7)
 */
#define hal_sleep(id) do {                                              \
        *(u64 *)(BR_MSR_BASE(id) + POWER_GATE_RESET*sizeof(u64)) |= 0x1ULL; \
    } while(0)

/**
 * @brief Wake the XE core from sleep
 *
 * This is used by CE to wake an XE core in its block from sleep
 * @warning 'id' is the agent ID (ID_AGENT_XE0 ... ID_AGENT_XE7)
 */
#define hal_wake(id) do {                                                    \
        *(u64 *)(BR_MSR_BASE(id) + POWER_GATE_RESET*sizeof(u64)) &= ~(0x1ULL); \
    } while(0)

/**
 * @brief On architectures (like TG) that
 * have different address "formats", canonicalize it
 * to the unique form
 */
#define hal_globalizeAddr(addr) tg_canonical(addr)

/**
 * @brief On architectures (like TG) that have
 * different address "formats", this returns the
 * smallest usable address from the global address 'addr'
 */
#define hal_localizeAddr(addr, me) tg_localize(addr, me)

// Support for abstract load and store macros
#define IS_REMOTE(addr) (__builtin_clzl(addr) < ((sizeof(u64) - 1) - MAP_AGENT_SHIFT))

// Abstraction to do a load operation from any level of the memory hierarchy
//static inline u8 AbstractLoad8(u64 addr) { return *((u8 *) addr); }
//#define GET8(temp,addr)  temp = AbstractLoad8(addr)
#define GET8(temp,addr)  ({ (temp) = *((u8 *) (addr)); })

//static inline u16 AbstractLoad16(u64 addr) { return *((u16 *) addr); }
//#define GET16(temp,addr)  temp = AbstractLoad16(addr)
#define GET16(temp,addr)  ({ (temp) = *((u16 *) (addr)); })

//static inline u32 AbstractLoad32(u64 addr) { return *((u32 *) addr); }
//#define GET32(temp,addr)  temp = AbstractLoad32(addr)
#define GET32(temp,addr)  ({ (temp) = *((u32 *) (addr)); })

//static inline u64 AbstractLoad64(u64 addr) { return *((u64 *) addr); }
//#define GET64(temp,addr)  temp = AbstractLoad64(addr)
#define GET64(temp,addr)  ({ (temp) = *((u64 *) (addr)); })

// Abstraction to do a store operation to any level of the memory hierarchy
//static inline void SET8(u64 addr, u8 value) { *((u8 *) addr) = value; }
#define  SET8(addr, value) ({ *((u8 *) (addr)) = (value); })

//static inline void SET16(u64 addr, u16 value) { *((u16 *) addr) = value; }
#define  SET16(addr, value) ({ *((u16 *) (addr)) = (value); })

//static inline void SET32(u64 addr, u32 value) { *((u32 *) addr) = value; }
#define  SET32(addr, value) ({ *((u32 *) (addr)) = (value); })

//static inline void SET64(u64 addr, u64 value) { *((u64 *) addr) = value; }
#define  SET64(addr, value) ({ *((u64 *) (addr)) = (value); })

#endif /* __OCR_HAL_FSIM_CE_H__ */
