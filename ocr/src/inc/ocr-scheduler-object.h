/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_SCHEDULER_OBJECT_H_
#define __OCR_SCHEDULER_OBJECT_H_

#include "ocr-types.h"
#include "ocr-runtime-types.h"
#include "ocr-runtime-hints.h"
#include "ocr-guid-kind.h"
#include "utils/ocr-utils.h"

struct _ocrSchedulerObjectFactory_t;
struct _ocrSchedulerObjectRoot_t;
struct _ocrScheduler_t;

/****************************************************/
/* OCR SCHEDULER OBJECT PROPERTIES AND KINDS               */
/****************************************************/

/*!
 * The schedulerObject properties are based on specific usage
 * types. The usage types are encoded in the LS 4 bits.
 * Properties for each usage follows this way with
 * recursive bit encoding on specific types.
 */
#define OCR_SCHEDULER_OBJECT_PROP_TYPE                     0x00F
#define OCR_SCHEDULER_OBJECT_PROP_INSERT                   0x001
#define OCR_SCHEDULER_OBJECT_PROP_REMOVE                   0x002
#define OCR_SCHEDULER_OBJECT_PROP_COUNT                    0x003
#define OCR_SCHEDULER_OBJECT_PROP_MAPPING                  0x004

#define SCHEDULER_OBJECT_REMOVE_DEQ                        0x012
#define SCHEDULER_OBJECT_REMOVE_DEQ_POP                    0x112
#define SCHEDULER_OBJECT_REMOVE_DEQ_STEAL                  0x213

#define SCHEDULER_OBJECT_COUNT_NONEMPTY                    0x013
#define SCHEDULER_OBJECT_COUNT_RECURSIVE                   0x023
#define SCHEDULER_OBJECT_COUNT_EDT                         0x043
#define SCHEDULER_OBJECT_COUNT_DB                          0x083
#define SCHEDULER_OBJECT_COUNT_ARRAY                       0x103

#define SCHEDULER_OBJECT_CREATE_IF_ABSENT                  0x014

typedef enum {
    OCR_SCHEDULER_OBJECT_MAPPING_POTENTIAL,    /* schedulerObjects are potentially mapped to certain locations or policy domains but not placed yet. Potential mappings can change. */
    OCR_SCHEDULER_OBJECT_MAPPING_MAPPED,       /* mappings are identified and they are currently in the process of being placed at specific locations */
    OCR_SCHEDULER_OBJECT_MAPPING_PINNED,       /* schedulerObjects are already placed at specific locations */
    OCR_SCHEDULER_OBJECT_MAPPING_UNDEFINED,
} ocrSchedulerObjectMappingKind;

/*! \brief OCR schedulerObject kinds
 *   This table should be extended whenever a new kind
 *   of schedulerObject is implemented.
 */
typedef enum {
    OCR_SCHEDULER_OBJECT_UNDEFINED         =0x00,

    //singleton schedulerObjects:
    //    These are the basic elements that are part of aggregate schedulerObjects.
    //    They do not have their own schedulerObject factories and hence they do not
    //    support the functions in ocrSchedulerObjectFcts_t.
    //    They are managed as part of aggregate schedulerObjects.
    OCR_SCHEDULER_OBJECT_SINGLETON         =0x01,
    OCR_SCHEDULER_OBJECT_EDT               =0x11,
    OCR_SCHEDULER_OBJECT_DB                =0x21,

    //aggregate schedulerObjects:
    //    These schedulerObjects can hold other schedulerObjects, both singleton and aggregate.
    //    These have associated schedulerObject factories and support ocrSchedulerObjectFcts_t.
    OCR_SCHEDULER_OBJECT_AGGREGATE         =0x02,
    OCR_SCHEDULER_OBJECT_DEQUE             =0x12,
    OCR_SCHEDULER_OBJECT_ARRAY             =0x22, //Implementation TODO.

    //root schedulerObjects:
    //    These are derived aggregate schedulerObjects that act as the single top-level
    //    schedulerObject for each scheduler. There is one ROOT schedulerObject per scheduler.
    //    They are persistent for the lifetime of the scheduler.
    OCR_SCHEDULER_OBJECT_ROOT              =0x03,
    OCR_SCHEDULER_OBJECT_ROOT_WST          =0x13,
} ocrSchedulerObjectKind;

#define SCHEDULER_OBJECT_KIND_TYPE(kind)           (kind & 0xF)
#define IS_SCHEDULER_OBJECT_SINGLETON_KIND(kind)   (SCHEDULER_OBJECT_KIND_TYPE(kind) == OCR_SCHEDULER_OBJECT_SINGLETON)
#define IS_SCHEDULER_OBJECT_AGGREGATE_KIND(kind)   (SCHEDULER_OBJECT_KIND_TYPE(kind) == OCR_SCHEDULER_OBJECT_AGGREGATE)
#define IS_SCHEDULER_OBJECT_ROOT_KIND(kind)        (SCHEDULER_OBJECT_KIND_TYPE(kind) == OCR_SCHEDULER_OBJECT_ROOT)

/****************************************************/
/* PARAMETER LISTS                                  */
/****************************************************/

typedef struct _paramListSchedulerObjectFact_t {
    ocrParamList_t base;
    ocrSchedulerObjectKind kind;
} paramListSchedulerObjectFact_t;

typedef struct _paramListSchedulerObject_t {
    ocrParamList_t base;
    ocrSchedulerObjectKind kind;
    u32 count;
} paramListSchedulerObject_t;

/****************************************************/
/* OCR SCHEDULER OBJECT                             */
/****************************************************/

/*! \brief OCR schedulerObject data structures.
 */
typedef struct _ocrSchedulerObject_t {
    ocrGuid_t guid;                     /**< GUID for this schedulerObject
                                             For singleton schedulerObjects this field is
                                             used to carry the element guid */
    ocrSchedulerObjectKind kind;              /**< Kind of schedulerObject */
    u32 fctId;                          /**< ID determining factory; Not used for singleton schedulerObjects. */
    ocrLocation_t loc;                  /**< Current location mapping for this schedulerObject.
                                             The mapping can be updated by the scheduler
                                             during the lifetime of the schedulerObject. */
    ocrSchedulerObjectMappingKind mapping;    /**< Mapping kind */
} ocrSchedulerObject_t;

/****************************************************/
/* OCR SCHEDULER OBJECT ACTIONS                     */
/****************************************************/

typedef enum {
    OCR_SCHEDULER_OBJECT_ACTION_INSERT,
    OCR_SCHEDULER_OBJECT_ACTION_REMOVE,
} ocrSchedulerObjectActionKind;

/**
 * When a scheduler invokes a scheduler heuristic during any operation,
 * the scheduler heuristic returns a set of actions to the scheduler
 * to be performed on the schedulerObjects. The scheduler heuristic does not
 * modify the schedulerObjects but leaves it to the scheduler to do it.
 */
typedef struct _ocrSchedulerObjectAction_t {
    ocrSchedulerObjectActionKind actionKind;
    u32 properties;
    void *tracker;                              /* used to track traversal progress */
    union {
        struct {
            ocrSchedulerObject_t *schedulerObject;          /* schedulerObject to insert into */
            ocrSchedulerObject_t *el;                 /* element to insert */
            u32 properties;                     /* insert properties */
        } insert;
        struct {
            ocrSchedulerObject_t *schedulerObject;          /* schedulerObject to remove from */
            ocrSchedulerObjectKind schedulerObjectKind;     /* kind of element to remove */
            u32 count;                          /* number of elements to remove */
            ocrSchedulerObject_t *dst;                /* schedulerObject to hold the removed elements */
            ocrSchedulerObject_t *el;                 /* (optional) specific elements to remove */
            u32 properties;                     /* remove properties */
        } remove;
    } args;
} ocrSchedulerObjectAction_t;

typedef struct _ocrSchedulerObjectActionSet_t {
    u32 actionCount;                            /* Number of actions */
    ocrSchedulerObjectAction_t *actions;              /* Array of actions */
} ocrSchedulerObjectActionSet_t;

/****************************************************/
/* OCR SCHEDULER OBJECT FUNCTIONS                   */
/****************************************************/

typedef struct _ocrSchedulerObjectFcts_t {

    /** @brief Create a new schedulerObject at runtime
     *
     *  @param[in] fact         Pointer to this schedulerObject factory
     *  @param[in] params       Params for the create
     *
     *  @returns the pointer to the instantiated schedulerObject
     */
    ocrSchedulerObject_t* (*create)(struct _ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params);

    /** @brief Dynamic destruction of schedulerObjects
     *
     *  @param[in] fact         Pointer to this schedulerObject factory
     *  @param[in] self         Pointer to the schedulerObject to be destroyed
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*destruct)(struct _ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self);

    /** @brief Insert an element into this(self) schedulerObject
     *
     *  @param[in] fact         Pointer to this schedulerObject factory
     *  @param[in] self         Pointer to this schedulerObject
     *  @param[in] element      SchedulerObject to insert
     *  @param[in] properties   Properties of the operation
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*insert)(struct _ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObject_t *element, u32 properties);

    /** @brief Removes an element from this(self) schedulerObject
     *
     *  This will remove an element from this schedulerObject set.
     *
     *  @param[in] fact          Pointer to this schedulerObject factory
     *  @param[in] self          Pointer to this schedulerObject (schedulerObject to remove from)
     *  @param[in] kind          Kind of schedulerObject to remove
     *  @param[in] count         Number of elements to remove
     *  @param[in] dst           SchedulerObject that will hold removed elements
     *  @param[in] elToRemove    [Optional] Specific element to remove
     *  @param[in] properties    Properties of the operation
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*remove)(struct _ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectKind kind, u32 count, ocrSchedulerObject_t *dst, ocrSchedulerObject_t *elToRemove, u32 properties);

    /** @brief Get the number of elements
     *
     *  This will return the number of elements in the schedulerObject
     *  The implementation may expose this as a function that
     *  simply responds as empty/non-empty.
     *
     *  @param[in] fact         Pointer to this schedulerObject factory
     *  @param[in] self         Pointer to this schedulerObject
     *  @param[in] properties   Properties for the count.
     *
     *  if SCHEDULER_OBJECT_COUNT_NONE (i.e no property) is defined,
     *      then count returns the immediate level element count.
     *  if SCHEDULER_OBJECT_COUNT_NONEMPTY is defined,
     *      then, count's value is 0(empty) or 1(at least one element) or 2(full).
     *  if SCHEDULER_OBJECT_COUNT_RECURSIVE is defined,
     *      then, count elements recursively for each schedulerObject element,
     *      i.e. count elements in elements that are schedulerObjects
     *
     *  If none or all of the following 3 properties are defined,
     *      then everything is counted.
     *
     *  if SCHEDULER_OBJECT_COUNT_EDT is defined,
     *      then, specifically count EDTs
     *  if SCHEDULER_OBJECT_COUNT_DB is defined,
     *      then, specifically count DBs
     *  if SCHEDULER_OBJECT_COUNT_ARRAY is defined,
     *      then, specifically count schedulerObjects
     *
     *  @return count, the number of elements in this schedulerObject
     */
    u64 (*count)(struct _ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 properties);

    //Location specific API

    /** @brief Get the schedulerObjects mapped to a specific location
     *
     *  Based on kind, these may be schedulerObjects that are "mapped"
     *  to potentially execute or be pinned to a specific location.
     *  The scheduler may update the location of a mapped schedulerObject
     *  multiple times during the lifetime of the schedulerObject but the
     *  location of a pinned schedulerObject remains constant until a
     *  "done" call is made to the scheduler for that schedulerObject.
     *
     *  If multiple schedulerObjects are found to be present, then a new
     *  schedulerObject is created and returned such that it encapsulates
     *  all those schedulerObjects.
     *
     *  If no schedulerObject is found then by default NULL is returned.
     *  However, if the properties flag is SCHEDULER_OBJECT_CREATE_IF_ABSENT,
     *  then an empty schedulerObject is created and returned.
     *
     *  @param[in] fact         Pointer to this schedulerObject factory
     *  @param[in] self         Pointer to this schedulerObject
     *  @param[in] loc          Location to query with
     *  @param[in] mapping      Mapping kind
     *  @param[in] properties   Properties of the operation
     *                          Can be 0 or SCHEDULER_OBJECT_CREATE_IF_ABSENT
     *
     *  @return pointer to schedulerObject on success and NULL on failure
     */
    ocrSchedulerObject_t* (*getSchedulerObjectForLocation)(struct _ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping, u32 properties);

    /** @brief Set the location this schedulerObject is mapped to
     *
     *  @param[in] self         Pointer to this schedulerObject
     *  @param[in] loc          Location mapping
     *  @param[in] mapping      Mapping kind
     *  @param[in] factories    All the schedulerObject factories
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*setLocationForSchedulerObject)(struct _ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping);
} ocrSchedulerObjectFcts_t;

/****************************************************/
/* OCR SCHEDULER OBJECT FACTORY                     */
/****************************************************/

/*! \brief Abstract factory class to create OCR schedulerObjects.
 *
 *  This class provides an interface to create SchedulerObject instances with a non-static create function
 *  to allow runtime implementers to choose to have state in their derived SchedulerObjectFactory classes.
 */
typedef struct _ocrSchedulerObjectFactory_t {
    u32 factoryId;                      /**< Corresponds to fctId in SchedulerObject */
    ocrSchedulerObjectKind kind;                /**< Kind of schedulerObject factory */
    struct _ocrPolicyDomain_t *pd;      /**< Policy domain of the schedulerObject factories */

    /*! \brief Virtual destructor for the SchedulerObjectFactory interface
     */
    void (*destruct)(struct _ocrSchedulerObjectFactory_t * factory);

    /*! \brief Instantiates a SchedulerObject and returns its corresponding pointer
     *         If it does not recognize the kind, then it returns NULL.
     */
    ocrSchedulerObject_t* (*instantiate)(struct _ocrSchedulerObjectFactory_t * factory, ocrParamList_t *perInstance);

    /*! \brief SchedulerObject interface functions
     */
    ocrSchedulerObjectFcts_t fcts;

} ocrSchedulerObjectFactory_t;

/****************************************************/
/* OCR SCHEDULER OBJECT ROOT                        */
/****************************************************/
/*
 * A "schedulerObject root" is a special kind of schedulerObject
 * that encapsulates all the schedulerObjects maintained by
 * a specific scheduler. Every scheduler maintains one
 * schedulerObject root which will contain multiple schedulerObjects
 * inside it. A scheduler heuristic is handed this schedulerObject
 * root in order to invoke its heuristics for give and take.
 */

/* Functions for maintaining the top-level schedulerObject collection root */
typedef struct _ocrSchedulerObjectRootFcts_t {
    void (*begin)(struct _ocrSchedulerObjectRoot_t *self);

    void (*start)(struct _ocrSchedulerObjectRoot_t *self);

    void (*stop)(struct _ocrSchedulerObjectRoot_t *self, ocrRunLevel_t newRl, u32 action);

    void (*destruct)(struct _ocrSchedulerObjectRoot_t *self);

    /*! \brief Creates an action set of with "count" number of actions
     */
    ocrSchedulerObjectActionSet_t* (*newActionSet)(struct _ocrSchedulerObjectRoot_t *self, u32 count);

    /*! \brief Destroys an action set
     */
    void (*destroyActionSet)(struct _ocrSchedulerObjectRoot_t *self, ocrSchedulerObjectActionSet_t *actionSet);

} ocrSchedulerObjectRootFcts_t;

typedef struct _ocrSchedulerObjectRoot_t {
    ocrSchedulerObject_t base;
    struct _ocrScheduler_t *scheduler;    /**< Scheduler of this schedulerObject root */
    ocrSchedulerObjectRootFcts_t fcts;       /**< root specific functions */
} ocrSchedulerObjectRoot_t;

/****************************************************/
/* OCR SCHEDULER OBJECT ROOT FACTORY                */
/****************************************************/

/*! \brief Abstract factory class to create OCR schedulerObject root
 *
 */
typedef struct _ocrSchedulerObjectRootFactory_t {
    struct _ocrSchedulerObjectFactory_t base;
    ocrSchedulerObjectRootFcts_t fcts;
} ocrSchedulerObjectRootFactory_t;

#endif /* __OCR_SCHEDULER_OBJECT_H_ */
