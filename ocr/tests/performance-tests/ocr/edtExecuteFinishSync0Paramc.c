#include "perfs.h"
#include "ocr.h"

// DESC: One worker creates all the tasks that have 'PARAMC_SZ' paramc arguments.
//       Sink EDT depends on all tasks through the output-event of a finish EDT.
// TIME: Completion of all tasks
// FREQ: Create 'NB_INSTANCES' EDTs once
//
// VARIABLES:
// - NB_INSTANCES
// - PARAMC_SZ: the size of paramc for the created EDTs

#include "edtExecuteFinishSync0.ctpl"
