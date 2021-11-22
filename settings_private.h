/******************************************************************************
    Private header of settings module
    Contains internal types and data structures

    This file may be modified for configuration reasons
******************************************************************************/
#ifndef SETTINGS_PRIVATE_H
#define SETTINGS_PRIVATE_H

#include "stdint.h"
#include "settings_public.h"

//-------------------------------------------------------//
// Configuration

// Set amount of memory for storing all the serialized data
// Amount of memory actually used must be checked after InitNode() call
#define SETTINGS_RAM_SIZE                   4096

// Define option to 1 to enable assertion for validate result
// Hepls to discover errors
#define ERROR_ON_VALIDATE_FAILED            1

// Define option to 1 to generate error for uninitialized nodes
// If option is set to 0, such nodes are skipped from processing
#define ERROR_ON_UNITIALIZED_NODE           0

// Define option to 1 to enable simple descriptor constructors
// If option is set to 0, descriptors should be created statically or using another memory allocation mechanism
#define ENABLE_NODE_CONSTRUCTORS           1

#if ENABLE_NODE_CONSTRUCTORS == 1

// Define option to 1 to use local memory manager
// If option is set to 0, stdlib functions are used
#define USE_SETTINGS_MEMORY_ALLOC           0

#if USE_SETTINGS_MEMORY_ALLOC == 1

// Set the memory size (bytes) for local memory manager
// This memory will be used to store parameter descriptors, not the data, thus is totally different from SETTINGS_RAM_SIZE
#define SETTINGS_ALLOC_MEMORY_SIZE          5200

#endif  // USE_SETTINGS_MEMORY_ALLOC

#endif  // ENABLE_NODE_CONSTRUCTORS

//-------------------------------------------------------//


//-------------------------------------------------------//
// Debug configuration

// Assertions defined externally to reduce cross-links
// Any check function may be used, including standard functions provided by RTOS/CMSIS/etc as desired
#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
    void assert_true(int x);
#ifdef __cplusplus
}
#endif // __cplusplus

#define SETTINGS_ASSERT_TRUE(x)             assert_true((int)(x))
#define SETTINGS_ASSERT_NEVER_EXECUTE()     assert_true(0)


// Debug printf
#if 1

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
    // Declare your custom printf-like function here
    //void printf(const char *format, ...);
#ifdef __cplusplus
}
#endif // __cplusplus
    #define SETTINGS_DEBUG(fmt, ...)            printf(fmt, __VA_ARGS__)

#else

    // No debug printing
    #define SETTINGS_DEBUG(fmt, ...)

#endif

//-------------------------------------------------------//




// Forward declaration
struct sNode_t;

// Request handler type
typedef resultType (*requestHandler)(rqType rq, struct sNode_t *node, uint32_t nodeRamBase, uint32_t nodeRomBase, request_t *rqst);


// Using 16-bit CRC
#define NODE_CRC_SIZE       2
#define NODE_CRC_SEED       0xFFFF


// Node type
typedef enum {
    sNode,          // Simple (terminating) node
    hNode,          // Hierarchy node
    lNode,          // List node
} nodeType;


#define GENERIC_NODE_PATTERN            nodeType type;  \
                                        uint32_t ramOffset;     /* Used by hNode for fast indexed access */  \
                                        uint32_t romOffset;


// Generic node descriptor
struct node_t {
    GENERIC_NODE_PATTERN
};


// Hierarchy node descriptor
struct hNode_t {
    // Common
    GENERIC_NODE_PATTERN
    // Custom
    uint16_t hListSize;             // Child list size
    struct node_t **hList;          // List of child node descriptors
};


// List node descriptor
struct lNode_t {
    // Common
    GENERIC_NODE_PATTERN
    // Custom
    uint16_t hListSize;             // Count of child elements (all elements are equal)
    struct node_t *element;         // Child node descriptor (since all are equal, single descriptor is used)
    uint32_t elementRamSize;
    uint32_t elementRomSize;
};


struct u32Prm_t {
    uint32_t defaultValue;
    uint32_t minValue;
    uint32_t maxValue;
};


struct charArrayPrm_t {
    const char *defaultValue;
};


// Simple (terminating) node descriptor
struct sNode_t {
    // Common
    GENERIC_NODE_PATTERN
    // Custom
    uint32_t size;
    uint8_t accessLevel;
    storageType storage;
    onChangeCallback changeCallback;
    requestHandler rqHandler;
    union {
        struct u32Prm_t u32Prm;
        struct charArrayPrm_t charArrayPrm;     // not 0-terminated
    } varData;
};


typedef struct node_t node_t;
typedef struct cNode_t cNode_t;
typedef struct hNode_t hNode_t;
typedef struct lNode_t lNode_t;
typedef struct sNode_t sNode_t;


// Node init context data
struct nodeInitContext_t {
    uint32_t depth;             // Current depth for a node
    uint32_t maxDepth;          // Maximum depth for whole tree
    uint32_t maxAllowedDepth;   // Maximum alowed depth (if maxDepth esceeds this value, error is generated)
};

typedef struct nodeInitContext_t nodeInitContext_t;


// Paramater validation result
typedef enum {
    ValidateOk,
    ValidateErr
} validateResult;




    resultType initNode(node_t *node, uint32_t *ramSize, uint32_t *romSize, nodeInitContext_t *ctx);
    resultType validateNode(node_t *node, uint32_t nodeRamBase, uint32_t nodeRomBase, uint8_t useDefaults);
    resultType invalidateNodeCrc(node_t *node, uint32_t nodeRamBase, uint32_t nodeRomBase, uint8_t wholeTree);
    void makeCRC16Table(void);
    uint16_t getCRC16(uint8_t *data, uint16_t len, uint16_t crc);

    resultType settingsRequest(request_t *rqst);
    uint32_t getRequestArg(uint32_t historyIndex);
    callbackCache_t *getCallbackCache(void);

#if ENABLE_NODE_CONSTRUCTORS == 1
#if USE_SETTINGS_MEMORY_ALLOC == 1
    void *settingsAlloc(uint32_t size);
    uint32_t getAllocMemoryUsed(void);
#endif
    sNode_t *createSNode(uint16_t size);
    hNode_t *createHNode(uint16_t elementsCount);
    lNode_t *createLNode(uint16_t elementsCount, void *node);

    void addToHList(hNode_t *hnode, uint32_t index, void *node);

    sNode_t *u32Node(uint8_t accessLevel, storageType storage,
                       uint32_t defaultValue, uint32_t minValue, uint32_t maxValue,
                       onChangeCallback changeCallback);
    sNode_t *u16Node(uint8_t accessLevel, storageType storage,
                           uint32_t minValue, uint32_t maxValue, uint32_t defaultValue,
                           onChangeCallback changeCallback);
    sNode_t *u8Node(uint8_t accessLevel, storageType storage,
                           uint32_t minValue, uint32_t maxValue, uint32_t defaultValue,
                           onChangeCallback changeCallback);
    sNode_t *charNode(uint8_t accessLevel, storageType storage,
                           uint32_t size, const char *defaultValue,
                           onChangeCallback changeCallback);
#endif
    validateResult validateU32(uint32_t value, struct u32Prm_t *prm);
    resultType handleRequestU32(rqType rq, struct sNode_t *pNode, uint32_t nodeRamBase, uint32_t nodeRomBase, request_t *rqst);
    resultType handleRequestCharArray(rqType rq, struct sNode_t *pNode, uint32_t nodeRamBase, uint32_t nodeRomBase, request_t *rqst);




// Alternative way to create nodes is to use macro below.
// Less convenient, but saves a lot of memory
#if ENABLE_NODE_CONSTRUCTORS == 0

#define u8Node(accs, stor, min, max, dflt, callback)   \
    {.type = sNode, .ramOffset = 0, .romOffset = 0, .size = 1, .accessLevel = accs, .storage = stor, .changeCallback = callback, .rqHandler = handleRequestU32, \
    .varData.u32Prm = {.defaultValue = dflt, .minValue = min, .maxValue = max}}

#define u8NodeRq(accs, stor, min, max, dflt, callback, handler)   \
    {.type = sNode, .ramOffset = 0, .romOffset = 0, .size = 1, .accessLevel = accs, .storage = stor, .changeCallback = callback, .rqHandler = handler, \
    .varData.u32Prm = {.defaultValue = dflt, .minValue = min, .maxValue = max}}

#define u16Node(accs, stor, min, max, dflt, callback)   \
    {.type = sNode, .ramOffset = 0, .romOffset = 0, .size = 2, .accessLevel = accs, .storage = stor, .changeCallback = callback, .rqHandler = handleRequestU32, \
    .varData.u32Prm = {.defaultValue = dflt, .minValue = min, .maxValue = max}}

#define u16NodeRq(accs, stor, min, max, dflt, callback, handler)   \
    {.type = sNode, .ramOffset = 0, .romOffset = 0, .size = 2, .accessLevel = accs, .storage = stor, .changeCallback = callback, .rqHandler = handler, \
    .varData.u32Prm = {.defaultValue = dflt, .minValue = min, .maxValue = max}}

#define u32Node(accs, stor, min, max, dflt, callback)   \
    {.type = sNode, .ramOffset = 0, .romOffset = 0, .size = 4, .accessLevel = accs, .storage = stor, .changeCallback = callback, .rqHandler = handleRequestU32, \
    .varData.u32Prm = {.defaultValue = dflt, .minValue = min, .maxValue = max}}

#define u32NodeRq(accs, stor, min, max, dflt, callback, handler)   \
    {.type = sNode, .ramOffset = 0, .romOffset = 0, .size = 4, .accessLevel = accs, .storage = stor, .changeCallback = callback, .rqHandler = handler, \
    .varData.u32Prm = {.defaultValue = dflt, .minValue = min, .maxValue = max}}

#define charNode(accs, stor, sz, dflt, callback)   \
    {.type = sNode, .ramOffset = 0, .romOffset = 0, .size = sz, .accessLevel = accs, .storage = stor, .changeCallback = callback, .rqHandler = handleRequestCharArray, \
    .varData.charArrayPrm = {.defaultValue = dflt}}

#define charNodeRq(accs, stor, sz, dflt, callback, handler)   \
    {.type = sNode, .ramOffset = 0, .romOffset = 0, .size = sz, .accessLevel = accs, .storage = stor, .changeCallback = callback, .rqHandler = handler, \
    .varData.charArrayPrm = {.defaultValue = dflt}}

#define hNode(list) \
    {.type = hNode, .ramOffset = 0, .romOffset = 0, .hListSize = sizeof(list)/sizeof(node_t *), .hList = list}

#define lNode(count, node) \
    {.type = lNode, .ramOffset = 0, .romOffset = 0, .hListSize = count, .element = node, .elementRamSize = 0, .elementRomSize = 0}

#endif




#endif // SETTINGS_PRIVATE_H
