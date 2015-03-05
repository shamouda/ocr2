#include "perfs.h"
#include "ocr.h"

// DESC: Creates NB_INSTANCES edt templates, do not destroy them.
// TIME: Creation of all edt templates
// FREQ: Done 'NB_ITERS' times.
//
// VARIABLES:
// - NB_INSTANCES
// - NB_ITERS

#define TIME_CREATION 1
#define TIME_DESTRUCTION 0
#define CLEAN_UP_ITERATION 0

#include "edtTemplate0.ctpl"
