#include "perfs.h"
#include "ocr.h"

// DESC: Create a producer event and 'FAN_OUT' consumer event depending on it.
// TIME: Setting up the dependence between producer and consumer
// FREQ: 'FAN_OUT' dependences done NB_ITERS' times.

#ifndef CUSTOM_BOUNDS
#define CUSTOM_BOUNDS
#define NB_ITERS 10
#define FAN_OUT 1000
#endif

#define PRODUCER_EVENT_TYPE  OCR_EVENT_ONCE_T
#define CONSUMER_EVENT_TYPE  OCR_EVENT_ONCE_T
#define CLEAN_UP_ITERATION   1

#define TIME_SATISFY 0
#define TIME_ADD_DEP 1
#define TIME_CONSUMER_CREATE 0
#define TIME_CONSUMER_DESTRUCT 0

#include "event2FanOutEvent.ctpl"
