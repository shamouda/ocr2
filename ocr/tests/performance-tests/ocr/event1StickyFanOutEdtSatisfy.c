#include "perfs.h"
#include "ocr.h"

// DESC: Create a producer event and 'FAN_OUT' consumer EDTs depending on it.
// TIME: Satisfy the producer event
// FREQ: Done 'NB_ITERS' times
// NOTE: The driver EDT is a finish EDT to collect created EDTs

#ifndef CUSTOM_BOUNDS
#define CUSTOM_BOUNDS
#define NB_ITERS 10
#define FAN_OUT 1000
#endif

#define PRODUCER_EVENT_TYPE  OCR_EVENT_STICKY_T

#define TIME_SATISFY 1
#define TIME_CONSUMER_CREATE 0
#define TIME_ADD_DEP 0

#include "event1FanOutEdt.ctpl"
