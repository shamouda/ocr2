#include "perfs.h"
#include "ocr.h"
#include "extensions/ocr-affinity.h"

// DESC:
// TIME: Time calls to ocrAddDependence
// FREQ: Do NB_ITERS calls
//
// VARIABLES
// - NB_ITERS (framework)

#define EVENT_TYPE  OCR_EVENT_STICKY_T
#define LOCAL_EVT  1
#define LOCAL_EDT 1
#define FRWK_REMOTE_SETUP 0

#include "evtToEdtAddDep.ctpl"
