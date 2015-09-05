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
#define OCR_SCHEDULER_OBJECT_PROP_ITERATE                  0x003
#define OCR_SCHEDULER_OBJECT_PROP_COUNT                    0x004
#define OCR_SCHEDULER_OBJECT_PROP_MAPPING                  0x005

// Insert
// specialized properties for lists
// #define SCHEDULER_OBJECT_INSERT_LIST                    0x011
#define SCHEDULER_OBJECT_INSERT_LIST_FRONT                 0x111
#define SCHEDULER_OBJECT_INSERT_LIST_BACK                  0x211
#define SCHEDULER_OBJECT_INSERT_LIST_BEFORE                0x311
#define SCHEDULER_OBJECT_INSERT_LIST_AFTER                 0x411

// specialized properties for map
// #define SCHEDULER_OBJECT_INSERT_MAP                     0x021
#define SCHEDULER_OBJECT_INSERT_MAP_PUT                    0x121
#define SCHEDULER_OBJECT_INSERT_MAP_TRY_PUT                0x221
#define SCHEDULER_OBJECT_INSERT_MAP_CONC_PUT               0x321
#define SCHEDULER_OBJECT_INSERT_MAP_CONC_TRY_PUT           0x421

// specialized properties for dbnode
// #define SCHEDULER_OBJECT_INSERT_DBNODE                  0x031
#define SCHEDULER_OBJECT_INSERT_DBNODE_PHASE               0x131

// Remove
// specialized properties for deq
// #define SCHEDULER_OBJECT_REMOVE_DEQ                     0x012
#define SCHEDULER_OBJECT_REMOVE_DEQ_POP                    0x112
#define SCHEDULER_OBJECT_REMOVE_DEQ_STEAL                  0x212

// specialized properties for lists
// #define SCHEDULER_OBJECT_REMOVE_LIST                    0x022
#define SCHEDULER_OBJECT_REMOVE_LIST_FRONT                 0x122
#define SCHEDULER_OBJECT_REMOVE_LIST_BACK                  0x222
#define SCHEDULER_OBJECT_REMOVE_LIST_CURRENT               0x322
#define SCHEDULER_OBJECT_REMOVE_LIST_BEFORE                0x422
#define SCHEDULER_OBJECT_REMOVE_LIST_AFTER                 0x522

// specialized properties for map
// #define SCHEDULER_OBJECT_REMOVE_MAP                     0x032
#define SCHEDULER_OBJECT_REMOVE_MAP_NON_CONC               0x132
#define SCHEDULER_OBJECT_REMOVE_MAP_CONC                   0x232

// Iterate
// Common iterator properties
#define SCHEDULER_OBJECT_ITERATE_BEGIN                     0x103
#define SCHEDULER_OBJECT_ITERATE_END                       0x203
#define SCHEDULER_OBJECT_ITERATE_CURRENT                   0x303
#define SCHEDULER_OBJECT_ITERATE_NEXT                      0x403
#define SCHEDULER_OBJECT_ITERATE_PREV                      0x503

// specialized properties for lists
// #define SCHEDULER_OBJECT_ITERATE_LIST                   0x013
#define SCHEDULER_OBJECT_ITERATE_LIST_HEAD_PEEK            0x613
#define SCHEDULER_OBJECT_ITERATE_LIST_TAIL_PEEK            0x713

// specialized properties for map
// #define SCHEDULER_OBJECT_ITERATE_MAP                    0x023
#define SCHEDULER_OBJECT_ITERATE_MAP_GET_NON_CONC          0x123
#define SCHEDULER_OBJECT_ITERATE_MAP_GET_CONC              0x223

// Count
#define SCHEDULER_OBJECT_COUNT_NONEMPTY                    0x014
#define SCHEDULER_OBJECT_COUNT_RECURSIVE                   0x024
#define SCHEDULER_OBJECT_COUNT_EDT                         0x044
#define SCHEDULER_OBJECT_COUNT_DB                          0x084
#define SCHEDULER_OBJECT_COUNT_ARRAY                       0x104

//Mapping
#define SCHEDULER_OBJECT_CREATE_IF_ABSENT                  0x015
#define SCHEDULER_OBJECT_MAPPING_WST                       0x105

typedef enum {
    OCR_SCHEDULER_OBJECT_MAPPING_POTENTIAL,    /* schedulerObjects are potentially mapped to certain locations or policy domains but not placed yet. Potential mappings can change. */
    OCR_SCHEDULER_OBJECT_MAPPING_MAPPED,       /* mappings are identified and they are currently in the process of being placed at specific locations */
    OCR_SCHEDULER_OBJECT_MAPPING_PINNED,       /* schedulerObjects are already placed at specific locations */
    OCR_SCHEDULER_OBJECT_MAPPING_WORKER,       /* schedulerObjects are mapped to a specific worker in current PD location */
    OCR_SCHEDULER_OBJECT_MAPPING_UNDEFINED,
} ocrSchedulerObjectMappingKind;

/*! \brief OCR schedulerObject kinds
 *   This table should be extended whenever a new kind of schedulerObject is implemented.
 *   LS 4 Bits (0-3)   : Scheduler object attributes (regular, root, iterator, etc.)
 *   Next 4 Bits (4-7) : Scheduler object type
 *   MS Bits + (4-7)   : Scheduler object kind
 */
typedef enum {
    OCR_SCHEDULER_OBJECT_UNDEFINED         =0x000,

    //singleton schedulerObjects:
    //    These are the basic elements that are part of aggregate schedulerObjects.
    //    They do not have their own schedulerObject factories and hence they do not
    //    support the functions in ocrSchedulerObjectFcts_t.
    //    They are managed as part of aggregate schedulerObjects.
    OCR_SCHEDULER_OBJECT_SINGLETON         =0x010,
    OCR_SCHEDULER_OBJECT_EDT               =0x110,
    OCR_SCHEDULER_OBJECT_DB                =0x210,

    //aggregate schedulerObjects:
    //    These schedulerObjects can hold other schedulerObjects, both singleton and aggregate.
    //    These have associated schedulerObject factories and support ocrSchedulerObjectFcts_t.
    OCR_SCHEDULER_OBJECT_AGGREGATE         =0x020,
    OCR_SCHEDULER_OBJECT_DEQUE             =0x320,
    OCR_SCHEDULER_OBJECT_WST               =0x420,
    OCR_SCHEDULER_OBJECT_ARRAY             =0x520, // BUG #613: No implementation of this yet
    OCR_SCHEDULER_OBJECT_LIST              =0x620,
    OCR_SCHEDULER_OBJECT_MAP               =0x720,
    OCR_SCHEDULER_OBJECT_DBNODE            =0x820, //Scheduler object to manage the scheduling of DBs
    OCR_SCHEDULER_OBJECT_DOMAIN            =0x920, //Scheduler object that manages a policy domain

    //root schedulerObjects:
    //    These are special aggregate schedulerObjects that act as the single top-level
    //    schedulerObject for each scheduler. There is one ROOT schedulerObject per scheduler.
    //    They are persistent for the lifetime of the scheduler.
    OCR_SCHEDULER_OBJECT_ROOT              =0x021,
    OCR_SCHEDULER_OBJECT_WST_ROOT          =(OCR_SCHEDULER_OBJECT_WST    | OCR_SCHEDULER_OBJECT_ROOT),
    OCR_SCHEDULER_OBJECT_DOMAIN_ROOT       =(OCR_SCHEDULER_OBJECT_DOMAIN | OCR_SCHEDULER_OBJECT_ROOT),

    //iterator schedulerObjects:
    //    These are special schedulerObjects that can be used to iterate over
    //    other aggregate objects.
    OCR_SCHEDULER_OBJECT_ITERATOR          =0x022,
    OCR_SCHEDULER_OBJECT_LIST_ITERATOR     =(OCR_SCHEDULER_OBJECT_LIST | OCR_SCHEDULER_OBJECT_ITERATOR),
    OCR_SCHEDULER_OBJECT_MAP_ITERATOR      =(OCR_SCHEDULER_OBJECT_MAP  | OCR_SCHEDULER_OBJECT_ITERATOR),

    //void* schedulerObjects:
    //    This type of scheduler object is used to pass void* data from/to another scheduler object
    //    It is the caller/callee's agreement how to handle the pointer.
    //    The pointer is passed through the ocrFatGuid_t metaDataPtr.
    OCR_SCHEDULER_OBJECT_VOID_PTR          =0x030,

} ocrSchedulerObjectKind;

#define SCHEDULER_OBJECT_KIND(kind)                (kind & ~0xF)
#define IS_SCHEDULER_OBJECT_TYPE_SINGLETON(kind)   ((kind & 0xF0) == OCR_SCHEDULER_OBJECT_SINGLETON)
#define IS_SCHEDULER_OBJECT_TYPE_AGGREGATE(kind)   ((kind & 0xF0) == OCR_SCHEDULER_OBJECT_AGGREGATE)
#define IS_SCHEDULER_OBJECT_TYPE_ROOT(kind)        ((kind & 0xFF) == OCR_SCHEDULER_OBJECT_ROOT)
#define IS_SCHEDULER_OBJECT_TYPE_ITERATOR(kind)    ((kind & 0xFF) == OCR_SCHEDULER_OBJECT_ITERATOR)

/****************************************************/
/* PARAMETER LISTS                                  */
/****************************************************/

typedef struct _paramListSchedulerObjectFact_t {
    ocrParamList_t base;
    ocrSchedulerObjectKind kind;
} paramListSchedulerObjectFact_t;

typedef struct _paramListSchedulerObject_t {
    ocrParamList_t base;
    ocrSchedulerObjectKind kind;    // Kind of scheduler object
    bool guidRequired;              // If object will stay local in the PD, then guid is not required
} paramListSchedulerObject_t;

/****************************************************/
/* OCR SCHEDULER OBJECT                             */
/****************************************************/

/*! \brief OCR schedulerObject data structures.
 */
typedef struct _ocrSchedulerObject_t {
    ocrFatGuid_t guid;                      /**< GUID for this schedulerObject
                                                 The metadata ptr points to the allocation of the scheduler object.
                                                 For singleton schedulerObjects this field is used to carry the
                                                 element guid and its metadata ptr */
    ocrSchedulerObjectKind kind;            /**< Kind of schedulerObject */
    u32 fctId;                              /**< ID determining factory; Not used for singleton schedulerObjects. */
    ocrLocation_t loc;                      /**< Current location mapping for this schedulerObject.
                                                 The mapping can be updated by the scheduler
                                                 during the lifetime of the schedulerObject. */
    ocrSchedulerObjectMappingKind mapping;  /**< Mapping kind */
} ocrSchedulerObject_t;

/****************************************************/
/* OCR SCHEDULER OBJECT ITERATOR                    */
/****************************************************/

#define ITERATOR_ARG_NAME(name) _arg_##name
#define ITERATOR_ARG_FIELD(type) data.ITERATOR_ARG_NAME(type)

/*! \brief OCR schedulerObject Iterator data structure
 */
typedef struct _ocrSchedulerObjectIterator_t {
    ocrSchedulerObject_t base;              /**< Scheduler object base */
    union {
        struct {
            void *el;                       /**< List node data element */
        } ITERATOR_ARG_NAME(OCR_SCHEDULER_OBJECT_LIST);
        struct {
            void *key, *value;              /**< Hash map key and value */
        } ITERATOR_ARG_NAME(OCR_SCHEDULER_OBJECT_MAP);
    } data;
} ocrSchedulerObjectIterator_t;

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
    u8 (*destroy)(struct _ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self);

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

    /** @brief Iterate over the scheduler object
     *
     *  @param[in] fact         Pointer to this schedulerObject factory
     *  @param[in] self         Scheduler object to iterate over
     *  @param[in] iterator     Iterator object
     *  @param[in] properties   Properties of the operation
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*iterate)(struct _ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectIterator_t *iterator, u32 properties);

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

    /*! \brief Instantiates a SchedulerObject from the factory and returns its corresponding pointer
     *         If it does not recognize the kind, then it returns NULL.
     */
    ocrSchedulerObject_t* (*instantiate)(struct _ocrSchedulerObjectFactory_t * factory, ocrParamList_t *perInstance);

    /*! \brief SchedulerObject interface functions
     */
    ocrSchedulerObjectFcts_t fcts;

} ocrSchedulerObjectFactory_t;

/****************************************************/
/* OCR SCHEDULER OBJECT ROOT FACTORY                */
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

    /**
     * @brief Switch runlevel
     *
     * @param[in] self         Pointer to this object
     * @param[in] PD           Policy domain this object belongs to
     * @param[in] runlevel     Runlevel to switch to
     * @param[in] phase        Phase for this runlevel
     * @param[in] properties   Properties (see ocr-runtime-types.h)
     * @param[in] callback     Callback to call when the runlevel switch
     *                         is complete. NULL if no callback is required
     * @param[in] val          Value to pass to the callback
     *
     * @return 0 if the switch command was successful and a non-zero error
     * code otherwise. Note that the return value does not indicate that the
     * runlevel switch occured (the callback will be called when it does) but only
     * that the call to switch runlevel was well formed and will be processed
     * at some point
     */
    u8 (*switchRunlevel)(ocrSchedulerObject_t *self, struct _ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                         phase_t phase, u32 properties, void (*callback)(struct _ocrPolicyDomain_t*, u64), u64 val);

    void (*destruct)(ocrSchedulerObject_t *self);

    /*! \brief Creates an action set of with "count" number of actions
     */
    ocrSchedulerObjectActionSet_t* (*newActionSet)(ocrSchedulerObject_t *self, struct _ocrPolicyDomain_t *PD, u32 count);

    /*! \brief Destroys an action set
     */
    void (*destroyActionSet)(ocrSchedulerObject_t *self, struct _ocrPolicyDomain_t *PD, ocrSchedulerObjectActionSet_t *actionSet);

} ocrSchedulerObjectRootFcts_t;

/*! \brief Abstract factory class to create OCR schedulerObject root
 *
 */
typedef struct _ocrSchedulerObjectRootFactory_t {
    struct _ocrSchedulerObjectFactory_t base;
    ocrSchedulerObjectRootFcts_t fcts;
} ocrSchedulerObjectRootFactory_t;

#endif /* __OCR_SCHEDULER_OBJECT_H_ */
