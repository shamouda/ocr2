 /*
  * This file is subject to the license agreement located in the file LICENSE
  * and cannot be distributed without it. This notice cannot be
  * removed or modified.
  */

#ifndef __TRACE_EVENTS_H__
#define __TRACE_EVENTS_H__

//Strings to identify user/runtime created objects
const char *evt_type[] = {
    "RUNTIME",
    "USER",
};

//Strings for traced OCR objects
const char *obj_type[] = {
    "EDT",
    "EVENT",
    "DATABLOCK"
};

//Strings for traced OCR events
const char *action_type[] = {
    "CREATE",
    "DESTROY",
    "RUNNABLE",
    "ADD_DEP",
    "SATISFY",
    "EXECUTE",
    "FINISH",
};

#endif /* __TRACE_EVENTS_H__ */
