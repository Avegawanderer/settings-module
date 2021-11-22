
#include <string.h>
#include <stdio.h>
#include "settings.h"
#include "settings_public.h"
#include "settings_private.h"


// Uncomment line below to use simulated ROM driver
#define __NOROM__


// RAM is private
extern uint8_t ram[SETTINGS_RAM_SIZE];

// Declare ROM driver interface
void readRom(uint32_t ramAddr, uint32_t romAddr, uint32_t count);
void writeRom(uint32_t romAddr, uint32_t ramAddr, uint32_t count);

// Global are used to store size of RAM
static uint32_t ramSize, romSize;

//-----------------------------------//
// Testbench ROM driver
#ifdef __NOROM__

#define SETTINGS_ROM_SIZE               SETTINGS_RAM_SIZE
static uint8_t rom[SETTINGS_ROM_SIZE];

void readRom(uint32_t ramAddr, uint32_t romAddr, uint32_t count)
{
    SETTINGS_ASSERT_TRUE((romAddr < SETTINGS_ROM_SIZE) && ((romAddr + count) <= SETTINGS_ROM_SIZE));
    memcpy(&ram[ramAddr], &rom[romAddr], count);
}


void writeRom(uint32_t romAddr, uint32_t ramAddr, uint32_t count)
{
    SETTINGS_ASSERT_TRUE((romAddr < SETTINGS_ROM_SIZE) && ((romAddr + count) <= SETTINGS_ROM_SIZE));
    memcpy(&rom[romAddr], &ram[ramAddr], count);
}

#else


// Reading from EEPROM and writing to EERPOM is allowed on startup only
// Settings module does not need to read EEPROM again after init is done
// Writing during normal operation should be done in queued manner to 
// avoid stalling of system task

void readRom(uint32_t ramAddr, uint32_t romAddr, uint32_t count)
{
    // TODO
}


void writeRom(uint32_t romAddr, uint32_t ramAddr, uint32_t count)
{
    // TODO
}

#endif
//-----------------------------------//

// Default text for C2 params (all)
static const char dfltC2[C2_SIZE] = "Default text";

// Callback for all B0 params
static void onB0ParamsChanged(rqType rq, uint32_t pId);

// Callback for C2
static void onC2ParamsChanged(rqType rq, uint32_t pId);


#if ENABLE_NODE_CONSTRUCTORS == 0

// Integer node:                       Access type     Storage type    Minimum                 Maximum                 Default                 Callback
// b0param_C0
static sNode_t node_C0 = u32Node (  AccessByAll,    RomStored,        0,                      100000,                   12345,                onB0ParamsChanged    );
// b0param_C1
static sNode_t node_C1 = u8Node  (  AccessByAll,    RomStored,        0,                      144,                         5,                 onB0ParamsChanged    );

// root
static node_t *hNodes_B0[b0param_Count] = {(node_t *)&node_C0, (node_t *)&node_C1};
static hNode_t hNode_B0 = hNode(hNodes_B0);

// b1param_C2i
// Char node:                          Access type     Storage type    Size        Default                 Callback
static sNode_t node_C2 = charNode (  AccessByAll,       RomStored,     C2_SIZE,    dfltC2,                  onC2ParamsChanged    );
static lNode_t lNode_B1 = lNode(C2_NODES_COUNT, (node_t *)&node_C2);

// Integer node:                   Access type     Storage type    Minimum                 Maximum                 Default                 Callback
static sNode_t node_B2 = u16Node ( AccessByAll,    NotRomStored,      1,                      1024,                  16,                     0           );

// Root (A0)
static node_t *hNodes_A0[pGroup_Count] = {(node_t *)&hNode_B0, (node_t *)&lNode_B1, (node_t *)&node_B2};
static hNode_t hNode_A0 = hNode(hNodes_A0);
hNode_t *hRoot = &hNode_A0;

#else

hNode_t *hRoot;

#endif



resultType initSettings(uint8_t useDefaults)
{
    nodeInitContext_t ctx;
    resultType result;

#if ENABLE_NODE_CONSTRUCTORS == 1
    hNode_t *hNode_B0;
    lNode_t *lNode_B1;

    hNode_B0 = createHNode(2);
    addToHList(hNode_B0, 0,           u32Node (  AccessByAll,    RomStored,        0,                      100000,                   12345,                onB0ParamsChanged));     // C0
    addToHList(hNode_B0, 1,           u32Node (  AccessByAll,    RomStored,        0,                      144,                         5,                 onB0ParamsChanged));     // C1

    lNode_B1 = createLNode(35, charNode (  AccessByAll,       RomStored,     C2_SIZE,    dfltC2,                  onC2ParamsChanged));    // C2

    hRoot = createHNode(3);             // A0
    addToHList(hRoot, 0, hNode_B0);
    addToHList(hRoot, 1, lNode_B1);
    addToHList(hRoot, 2, u16Node (  AccessByAll,    NotRomStored,      1,                      1024,                  16,                     0));   // B2
#endif

    // Create CRC16 lookup table
    makeCRC16Table();

    // Create RAM and ROM map for the whole tree
    ctx.depth = 0;
    ctx.maxDepth = 0;
    ctx.maxAllowedDepth = 10;
    // InitNode is first initialization stage, it does not actualy use RAM or ROM, only tree structure is created
    initNode((node_t *)hRoot, &ramSize, &romSize, &ctx);
    SETTINGS_ASSERT_TRUE(ramSize <= SETTINGS_RAM_SIZE);
    hRoot->ramOffset = 0;       // Start address for RAM
    hRoot->romOffset = 0;       // Start address for ROM

    SETTINGS_DEBUG("Settings total RAM: %d, ROM %d bytes, depth %d\n", ramSize, romSize, ctx.maxDepth);
#if USE_SETTINGS_MEMORY_ALLOC == 1
    SETTINGS_DEBUG("Main RAM: %d of %d bytes, descriptor RAM: %d of %d bytes\n", ramSize, SETTINGS_RAM_SIZE, getAllocMemoryUsed(), SETTINGS_ALLOC_MEMORY_SIZE);
#endif
    
    // Restore whole RAM from external ROM device
    // Non-ROM stored parameters that are read from ROM are ignored and get their values from nodes description
    // TODO
    // if (Eeprom::IsEepromOk())
    //     Eeprom::Read(0, ram, ramSize);

    // Validate values and check CRC
    result = validateNode((node_t *)hRoot, hRoot->ramOffset, hRoot->romOffset, useDefaults);
    SETTINGS_DEBUG("Validate result 0x%02X %s\n", result, (result & Result_UpdatedRom) ? "(defaults restored)" : "");

    // ROM should be updated (may take some time)
    if (result & Result_UpdatedRom)
    {
        // TODO
    }

    return (resultType)(result & Result_UpdatedRom);
}


void resetSettingsToDefaults(void)
{
    // Defaults will be restored on next system start due to wrong CRC
    invalidateNodeCrc((node_t *)hRoot, hRoot->ramOffset, hRoot->romOffset, 1);
}


void flushSettingsToRom(void)
{
    // Copy whole RAM to external ROM device
    // TODO
    //drvEeprom_EraseAll();
    //drvEeprom_Write(0, ram, ramSize);
}




static void onB0ParamsChanged(rqType rq, uint32_t pId)
{
    // Last argument is passed by pId
    switch (pId)
    {
        case 0:
            printf("C0 param changed, new value is: %d\n", (uint32_t)getCallbackCache()->i32);
            break;

        case 1:
            printf("C1 param changed, new value is: %d\n", (uint8_t)getCallbackCache()->i32);
            break;
    }
}


static void onC2ParamsChanged(rqType rq, uint32_t pId)
{
    // Last argument is passed by pId and can be also obtained by argHistory
    // In this example, we use getRequestArg(0) which is equal to pId
    // In more complex cases, getRequestArg(1), getRequestArg(2) and so on may be called
    printf("C2 param changed, index is: %d, new value is: %s\n", getRequestArg(0), getCallbackCache()->str);
}



// Convenience alias for reading a 8-bit to 32-bit integer from a group with 1 param
int32_t settings_ReadI32(uint32_t pGroup, uint32_t param)
{
    request_t rq;
    int32_t val32 = 0;
    rq.rq = rqRead;
    rq.arg[0] = pGroup;
    rq.arg[1] = param;
    rq.val.i32 = &val32;
    rq.raw = 0;
    settingsRequest(&rq);
    return val32;
}


// Convenience alias for reading a 8-bit to 32-bit integer from a group with 4 params
int32_t settings_ReadI32_4(uint32_t pGroup, uint32_t param, uint32_t param2, uint32_t param3, uint32_t param4)
{
    request_t rq;
    int32_t val32 = 0;
    rq.rq = rqRead;
    rq.arg[0] = pGroup;
    rq.arg[1] = param;
    rq.arg[2] = param2;
    rq.arg[3] = param3;
    rq.arg[4] = param4;
    rq.val.i32 = &val32;
    rq.raw = 0;
    settingsRequest(&rq);
    return val32;
}

// Convenience alias for writing a 8-bit to 32-bit integer to a group with 1 param
void settings_WriteI32(uint32_t pGroup, uint32_t param, int32_t value)
{
    request_t rq;
    rq.rq = rqWrite;
    rq.arg[0] = pGroup;
    rq.arg[1] = param;
    rq.val.i32 = &value;
    rq.raw = 0;
    settingsRequest(&rq);
}

// Convenience alias for writing a 8-bit to 32-bit integer to a group with 4 params
void settings_WriteI32_4(uint32_t pGroup, uint32_t param, uint32_t param2, uint32_t param3, uint32_t param4, int32_t value)
{
    request_t rq;
    rq.rq = rqWrite;
    rq.arg[0] = pGroup;
    rq.arg[1] = param;
    rq.arg[2] = param2;
    rq.arg[3] = param3;
    rq.arg[4] = param4;
    rq.val.i32 = &value;
    rq.raw = 0;
    settingsRequest(&rq);
}


// Convenience alias for writing a 8-bit to 32-bit integer to a group with 1 param, without callback function
void settings_WriteI32NoCbf(uint32_t pGroup, uint32_t param, int32_t value)
{
    request_t rq;
    rq.rq = rqWriteNoCb;
    rq.arg[0] = pGroup;
    rq.arg[1] = param;
    rq.val.i32 = &value;
    rq.raw = 0;
    settingsRequest(&rq);
}


// Convenience alias for reading a char array from a group with 2 params
// Output char array may or may not be 0-terminated!
void settings_ReadStr(uint32_t pGroup, uint32_t param, char *str)
{
    request_t rq;
    rq.rq = rqRead;
    rq.arg[0] = pGroup;
    rq.arg[1] = param;
    rq.raw = (uint8_t *)str;
    settingsRequest(&rq);
}

// Convenience alias for writing a char array to a group with 2 params
// Input char array may or may not be 0-terminated!
void settings_WriteStr(uint32_t pGroup, uint32_t param, char *str)
{
    request_t rq;
    rq.rq = rqWrite;
    rq.arg[0] = pGroup;
    rq.arg[1] = param;
    rq.raw = (uint8_t *)str;
    settingsRequest(&rq);
}

