/******************************************************************************
    Public header of settings module
    Contains exported types and data structures

    This file should not be modified for configuration reasons
******************************************************************************/
#ifndef SETTINGS_PUBLIC_H
#define SETTINGS_PUBLIC_H


#include "stdint.h"

// Maximum settings depth is used to constrain tree (hepls to discover errors) and sets number of stored arguments for a request
// Must be big enough to enable required tree depth
#define SETTINGS_MAX_DEPTH                  10


// Access level
typedef enum {
    AccessByAll,
    AccessByService,
    AccessByDev
} accessLevel;


// Storage type
typedef enum {
    NotRomStored,
    RomStored
} storageType;


// Request type
typedef enum {
    rqRead = 0x00,
    rqApplyNoCb = 0x01,
    rqApply = 0x03,           // Update cache only
    rqStore = 0x04,           // Write to ROM only
    rqWriteNoCb = (rqStore | rqApplyNoCb),
    rqWrite = (rqStore | rqApply),
    rqValidate = 0x08,
    rqGetMin = 0x10,
    rqGetMax = 0x20,
    rqGetSize = 0x40,
    rqRestoreValidate = 0xFE,
    rqRestoreDefault = 0xFF
} rqType;


// Result type
typedef enum {
    Result_OK,
    Result_RestoredDefaults,
    Result_UnknownNodeType,
    Result_WrongNodeType,
    Result_UnknownRequestType,
    Result_WrongRequestType,
    Result_NotEnoughArguments,
    Result_DepthExceeded,
    Result_ValidateError,
    Result_UpdatedRom = 0x80        // May be ORed with other results
} resultType;


// Request data structure
typedef struct {
    rqType rq;
    accessLevel accLevel;
    uint32_t arg[SETTINGS_MAX_DEPTH];
    union {
        int32_t *i32;
    } val;
    uint8_t *raw;                   // Raw serialized data. If set to non-zero, data must be read or written in raw serialized form
                                    // Char arrays always use raw form.
    resultType result;              // Returned request result
} request_t;


// Values cache for change callback
typedef union {
    int32_t i32;
    char *str;
} callbackCache_t;

// Callback function prototype
typedef void (*onChangeCallback)(rqType rq, uint32_t lastArg);





#endif // SETTINGS_PUBLIC_H
