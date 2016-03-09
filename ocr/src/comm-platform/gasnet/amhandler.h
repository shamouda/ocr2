#include "ocr-config.h"
#include "ocr-types.h"

#ifdef ENABLE_COMM_PLATFORM_GASNET

#ifndef amhandler_table_h
#define amhandler_table_h


//*****************************************************************************
// global include files
//*****************************************************************************

#include <gasnet.h>


//******************************************************************************
// type declarations
//******************************************************************************

typedef void (*amhandler_fn_t)();


//******************************************************************************
// macros
//******************************************************************************

#define AMHANDLER_INDEX_BASE 201

#define BITS64_HIGH(x)  (0+( (u64)x >> 32 ))
#define BITS64_LOW(x)   (0+((u32)((u64)x)))

#define BITS64(x,y) ((u64)0+( ((u64)x << 32) | y ))

//-----------------------------------------------------------------------------
// synthesize an identifier used to refer to an active message handler by index
//-----------------------------------------------------------------------------
#define PASTER(x,y) x ## y

#define AMHANDLER(handler) PASTER(handler,__handler_index)


//-----------------------------------------------------------------------------
// AMHANDLER_REGISTER(handler)
//    registers the handler.
//    this macro goes in the handler file (event.c, locks.c, etc).
//    note: use the AMHANDLER--REGISTER macro name only in the context of
//          registering a handler so as not to cause problems for grep.
//-----------------------------------------------------------------------------

#define AMH_REGISTER_FCN(handler)  handler ## _register_fcn

#define AMHANDLER_REGISTER(handler)                     \
gasnet_handler_t AMHANDLER(handler) = 0;                \
void AMH_REGISTER_FCN(handler) (void)                   \
{                                                       \
  AMHANDLER(handler) = amhandler_register(handler);     \
}


// misc GASNet utilities

#define GASNET_Safe(fncall) do {                                     \
    int _retval;                                                     \
    if ((_retval = fncall) != GASNET_OK) {                           \
        fprintf(stderr, "ERROR calling: %s\n"                        \
                " at: %s:%"PRId32"\n"                                \
                " error: %s (%s)\n",                                 \
            #fncall, __FILE__, __LINE__,                             \
            gasnet_ErrorName(_retval), gasnet_ErrorDesc(_retval));   \
        fflush(stderr);                                              \
        gasnet_exit(_retval);                                        \
    }                                                                \
} while(0)


//******************************************************************************
// interface operations
//******************************************************************************

gasnet_handlerentry_t *amhandler_table();

int amhandler_count();

gasnet_handler_t amhandler_register(amhandler_fn_t handler);

void amhandler_free_table();

/**********
 * An outlined version of BIT64 macro.
 * Somehow by outlining with this function we can avoid memory corruption.
 *
 * Without outlining, we encountered several data races in merging hi and lo bits
 ***********/
uint64_t getBits64(uint32_t hi, uint32_t lo);


#endif // amhandler_table_h
#endif //ENABLE_COMM_PLATFORM_GASNET
