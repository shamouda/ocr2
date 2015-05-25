#include "perfs.h"
#include "ocr.h"

// DESC: Create a producer event and 'FAN_OUT' consumer event depending on it.
// TIME: Setting up the dependence between producer and consumer
// FREQ: 'FAN_OUT' dependences done NB_ITERS' times.
//
// VARIABLES
// - NB_ITERS
// - FAN_OUT

#define PRODUCER_EVENT_TYPE  OCR_EVENT_ONCE_T
#define CONSUMER_EVENT_TYPE  OCR_EVENT_ONCE_T

// Cannot clean-up iteration for non-persistent events
#define CLEAN_UP_ITERATION   0

#define TIME_SATISFY 0
#define TIME_ADD_DEP 1
#define TIME_CONSUMER_CREATE 0
#define TIME_CONSUMER_DESTRUCT 0

#include "event2FanOutEvent.ctpl"
