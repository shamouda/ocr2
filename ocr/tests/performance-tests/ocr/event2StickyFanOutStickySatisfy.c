#include "perfs.h"
#include "ocr.h"

// DESC: Create a producer event and 'FAN_OUT' consumer event depending on it.
// TIME: Satisfying an event that has 'FAN_OUT' dependences
// FREQ: Done 'NB_ITERS' times.

#ifndef CUSTOM_BOUNDS
#define CUSTOM_BOUNDS
#define NB_ITERS 10
#define FAN_OUT 1000
#endif

#define PRODUCER_EVENT_TYPE  OCR_EVENT_STICKY_T
#define CONSUMER_EVENT_TYPE  OCR_EVENT_STICKY_T
#define CLEAN_UP_ITERATION   1

#define TIME_SATISFY 1
#define TIME_ADD_DEP 0
#define TIME_CONSUMER_CREATE 1
#define TIME_CONSUMER_DESTRUCT 1

#include "event2FanOutEvent.ctpl"
