#include "perfs.h"
#include "ocr.h"

// DESC: Create a producer event and 'FAN_OUT' consumer EDTs depending on it.
// TIME: Satisfy the producer event
// FREQ: Done 'NB_ITERS' times
// NOTE: The driver EDT is a finish EDT to collect created EDTs
//
// VARIABLES
// - NB_ITERS
// - FAN_OUT

#define PRODUCER_EVENT_TYPE  OCR_EVENT_STICKY_T

#define TIME_SATISFY 1
#define TIME_CONSUMER_CREATE 0
#define TIME_ADD_DEP 0

#include "event1FanOutEdt.ctpl"
