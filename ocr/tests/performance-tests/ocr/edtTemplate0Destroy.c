#include "perfs.h"
#include "ocr.h"

// DESC: Creates NB_INSTANCES edt templates, destroy them.
// TIME: Destruction of all edt templates
// FREQ: Done 'NB_ITERS' times.
//
// VARIABLES:
// - NB_INSTANCES
// - NB_ITERS

#define TIME_CREATION 0
#define TIME_DESTRUCTION 1
#define CLEAN_UP_ITERATION 1

#include "edtTemplate0.ctpl"
