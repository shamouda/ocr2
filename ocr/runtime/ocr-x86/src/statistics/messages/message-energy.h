#include "messages-macros.h"

#define MESSAGE_NAME ENERGY
START_MESSAGE
u64 time_usecs;
ocrGuid_t worker;
const char * edt_name;
ocrGuid_t edt_guid;
u64 db_size;
ocrGuid_t db_guid;
u64 energy_pJ;
END_MESSAGE
#undef MESSAGE_NAME
