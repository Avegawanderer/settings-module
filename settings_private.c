/******************************************************************************
    Implementation of settings module

    This file should not be modified for configuration reasons
******************************************************************************/

#include <string.h>
#include "settings_private.h"
#include "settings_public.h"
#if (ENABLE_NODE_CONSTRUCTORS == 1) && (USE_SETTINGS_MEMORY_ALLOC == 0)
#include <stdlib.h>
#endif


#if ENABLE_NODE_CONSTRUCTORS == 1
#if USE_SETTINGS_MEMORY_ALLOC == 1
uint8_t allocRam[SETTINGS_ALLOC_MEMORY_SIZE];
uint32_t allocRamSize = SETTINGS_ALLOC_MEMORY_SIZE;
uint32_t allocRamAddr = 0;
#define SETTINGS_ALLOCATE(x)    settingsAlloc(x)
#else
#define SETTINGS_ALLOCATE(x)    calloc(1, x)
#endif  // USE_SETTINGS_MEMORY_ALLOC
#endif  // ENABLE_NODE_CONSTRUCTORS


// Local data storage
uint8_t ram[SETTINGS_RAM_SIZE];

// Argument history may be useful for determining changed value index in multy-dimensional lists
uint32_t argHistory[SETTINGS_MAX_DEPTH];
callbackCache_t callbackCache;
static uint16_t crcTable[256];

// Root node must be defined in top module
extern hNode_t *hRoot;

// External functions

    uint16_t getCRC16(uint8_t *data, uint16_t len, uint16_t crc);
    void readRom(uint32_t ramAddr, uint32_t romAddr, uint32_t count);
    void writeRom(uint32_t romAddr, uint32_t ramAddr, uint32_t count);

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

    void u32toBytesMsbFirst(uint32_t *number, uint8_t *bytes, uint32_t count);
    void bytesToU32MsbFirst(uint8_t *bytes, uint32_t *number, uint32_t count);

#ifdef __cplusplus
}
#endif

    // Prototypes

    static void pushArg(uint32_t *argHistory, uint32_t argHistorySize, uint32_t arg);
    static void popArg(uint32_t *argHistory, uint32_t argHistorySize);
    static uint32_t getNodeCrc(node_t *node, uint32_t nodeRamBase);
    static void updateNodeCRC(node_t *node, uint32_t nodeRamBase, uint32_t nodeRomBase);
    static resultType checkNodeCRC(node_t *node, uint32_t nodeRamBase);



//-----------------------------------------------------------------//
//-----------------------------------------------------------------//
// Node control
//-----------------------------------------------------------------//
//-----------------------------------------------------------------//

resultType initNode(node_t *node, uint32_t *ramSize, uint32_t *romSize, nodeInitContext_t *ctx)
{
    hNode_t *hnode;
    lNode_t *lnode;
    sNode_t *snode;
    uint16_t i;
    uint32_t ramOffset = 0;
    uint32_t romOffset = 0;
    uint32_t nodeRamSize, nodeRomSize;
    ctx->depth++;
    if (ctx->depth > ctx->maxDepth)
    {
        ctx->maxDepth = ctx->depth;
        if (ctx->maxDepth > ctx->maxAllowedDepth)
        {
            SETTINGS_ASSERT_NEVER_EXECUTE();
            *ramSize = 0;
            *romSize = 0;
            return Result_DepthExceeded;
        }
    }
    switch (node->type)
    {
        case hNode:
            hnode = (hNode_t *)node;

            // First few bytes are used by CRC
            ramOffset += NODE_CRC_SIZE;
            romOffset += NODE_CRC_SIZE;

            // Init terminating nodes
            for (i=0; i<hnode->hListSize; i++)
            {
#if ERROR_ON_UNITIALIZED_NODE == 1
                SETTINGS_ASSERT_TRUE(hnode->hList[i]);
#else
                if (hnode->hList[i] == 0)
                    continue;
#endif
                if (hnode->hList[i]->type == sNode)
                {
                    initNode(hnode->hList[i], &nodeRamSize, &nodeRomSize, ctx);
                    hnode->hList[i]->ramOffset = ramOffset;
                    hnode->hList[i]->romOffset = romOffset;
                    ramOffset += nodeRamSize;
                    romOffset += nodeRomSize;
                }
            }

            // Here ROM offset may be page-aligned for hierarchy nodes if necessary
            // Init hierarchy nodes
            for (i=0; i<hnode->hListSize; i++)
            {
                if (hnode->hList[i] == 0)
                    continue;
                if ((hnode->hList[i]->type == hNode) || (hnode->hList[i]->type == lNode))
                {
                    initNode(hnode->hList[i], &nodeRamSize, &nodeRomSize, ctx);
                    hnode->hList[i]->ramOffset = ramOffset;
                    hnode->hList[i]->romOffset = romOffset;
                    // Here ROM offset may be page-aligned for hierarchy nodes if necessary
                    ramOffset += nodeRamSize;
                    romOffset += nodeRomSize;
                }
            }

            // Return used amount of RAM and ROM
            *ramSize = ramOffset;
            *romSize = romOffset;
            break;

        case lNode:
            lnode = (lNode_t *)node;

            // First few bytes are used by CRC
            ramOffset += NODE_CRC_SIZE;
            romOffset += NODE_CRC_SIZE;

            initNode(lnode->element, &nodeRamSize, &nodeRomSize, ctx);
            lnode->element->ramOffset = ramOffset;
            lnode->element->romOffset = romOffset;
            lnode->elementRamSize = nodeRamSize;
            lnode->elementRomSize = nodeRomSize;
            // Here ROM offset may be page-aligned for hierarchy nodes if necessary
            ramOffset += nodeRamSize * lnode->hListSize;
            romOffset += nodeRomSize * lnode->hListSize;

            // Return used amount of RAM and ROM
            *ramSize = ramOffset;
            *romSize = romOffset;
            break;

        case sNode:
            snode = (sNode_t *)node;
            // Return used amount of RAM and ROM
            *ramSize = snode->size;
            *romSize = (snode->storage == RomStored) ? snode->size : 0;
            break;

        default:
            SETTINGS_ASSERT_NEVER_EXECUTE();
            break;
    }
    ctx->depth--;
    return Result_OK;
}


resultType validateNode(node_t *node, uint32_t nodeRamBase, uint32_t nodeRomBase, uint8_t useDefaults)
{
    hNode_t *hnode;
    lNode_t *lnode;
    sNode_t *snode;
    uint16_t i;
    uint32_t ramAddr, romAddr;
    resultType result, nodeResult, snodeResult;
    resultType crcCheckResult;
    switch (node->type)
    {
        case hNode:
            hnode = (hNode_t *)node;
            result = Result_OK;
            snodeResult = Result_OK;

            // Run through all nodes, check if values are valid
            for (i=0; i<hnode->hListSize; i++)
            {
#if ERROR_ON_UNITIALIZED_NODE == 1
                SETTINGS_ASSERT_TRUE(hnode->hList[i]);
#else
                if (hnode->hList[i] == 0)
                    continue;
#endif
                pushArg(argHistory, SETTINGS_MAX_DEPTH, i);
                ramAddr = nodeRamBase + hnode->hList[i]->ramOffset;
                romAddr = nodeRomBase + hnode->hList[i]->romOffset;
                nodeResult = validateNode(hnode->hList[i], ramAddr, romAddr, useDefaults);
                popArg(argHistory, SETTINGS_MAX_DEPTH);
                if (hnode->hList[i]->type == sNode)
                    snodeResult = (resultType)(snodeResult | nodeResult);
                else
                    result = (resultType)(result | nodeResult);
            }

            if (useDefaults)
            {
                // All nodes have been restored already. Update hnode CRC
                updateNodeCRC((node_t *)hnode, nodeRamBase, nodeRomBase);
                result = (resultType)(result | Result_UpdatedRom);
            }
            else
            {
                if (snodeResult == Result_OK)
                {
                    // All snodes are valid. Restore and check hnode CRC
                    readRom(nodeRamBase, nodeRomBase, NODE_CRC_SIZE);
                    crcCheckResult = checkNodeCRC((node_t *)hnode, nodeRamBase);
                }
                if ((snodeResult != Result_OK) || (crcCheckResult != Result_OK))
                {
                    // Run through snodes again, force defaults
                    for (i=0; i<hnode->hListSize; i++)
                    {
                        if (hnode->hList[i]->type != sNode)
                            continue;
                        pushArg(argHistory, SETTINGS_MAX_DEPTH, i);
                        ramAddr = nodeRamBase + hnode->hList[i]->ramOffset;
                        romAddr = nodeRomBase + hnode->hList[i]->romOffset;
                        nodeResult = validateNode(hnode->hList[i], ramAddr, romAddr, 1);
                        popArg(argHistory, SETTINGS_MAX_DEPTH);
                    }
                    // Update hnode CRC
                    updateNodeCRC((node_t *)hnode, nodeRamBase, nodeRomBase);
                    result = (resultType)(result | Result_UpdatedRom);
                }
            }
            break;

        case lNode:
            lnode = (lNode_t *)node;
            result = Result_OK;
            snodeResult = Result_OK;

            // Run through all nodes, check if values are valid
            for (i=0; i<lnode->hListSize; i++)
            {
                // Here ROM offset may be page-aligned for hierarchy nodes if necessary
                pushArg(argHistory, SETTINGS_MAX_DEPTH, i);
                ramAddr = nodeRamBase + lnode->element->ramOffset + (lnode->elementRamSize * i);
                romAddr = nodeRomBase + lnode->element->romOffset + (lnode->elementRomSize * i);
                nodeResult = validateNode(lnode->element, ramAddr, romAddr, useDefaults);
                popArg(argHistory, SETTINGS_MAX_DEPTH);
                if (lnode->element->type == sNode)
                    snodeResult = (resultType)(snodeResult | nodeResult);
                else
                    result = (resultType)(result | nodeResult);
            }

            if (useDefaults)
            {
                // All nodes have been restored already. Update lnode CRC
                updateNodeCRC((node_t *)lnode, nodeRamBase, nodeRomBase);
                result = (resultType)(result | Result_UpdatedRom);
            }
            else
            {
                if (snodeResult == Result_OK)
                {
                    // All snodes are valid. Restore and check lnode CRC
                    readRom(nodeRamBase, nodeRomBase, NODE_CRC_SIZE);
                    crcCheckResult = checkNodeCRC((node_t *)lnode, nodeRamBase);
                }
                if ((snodeResult != Result_OK) || (crcCheckResult != Result_OK))
                {
                    for (i=0; i<lnode->hListSize; i++)
                    {
                        if (lnode->element->type != sNode)
                            break;
                        // Here ROM offset may be page-aligned for hierarchy nodes if necessary
                        pushArg(argHistory, SETTINGS_MAX_DEPTH, i);
                        ramAddr = nodeRamBase + lnode->element->ramOffset + (lnode->elementRamSize * i);
                        romAddr = nodeRomBase + lnode->element->romOffset + (lnode->elementRomSize * i);
                        nodeResult = validateNode(lnode->element, ramAddr, romAddr, 1);
                        popArg(argHistory, SETTINGS_MAX_DEPTH);
                    }
                    // Update hnode CRC
                    updateNodeCRC((node_t *)lnode, nodeRamBase, nodeRomBase);
                    result = (resultType)(result | Result_UpdatedRom);
                }
            }
            break;

        case sNode:
            snode = (sNode_t *)node;
            SETTINGS_ASSERT_TRUE(snode->rqHandler != 0);
            result = snode->rqHandler((useDefaults) ? rqRestoreDefault : rqRestoreValidate, snode, nodeRamBase, nodeRomBase, 0);
            break;

        default:
            SETTINGS_ASSERT_NEVER_EXECUTE();
            result = Result_UnknownNodeType;
            break;
    }
    return result;
}


static uint32_t getNodeCrc(node_t *node, uint32_t nodeRamBase)
{
    uint32_t crc = NODE_CRC_SEED;
    hNode_t *hnode;
    lNode_t *lnode;
    sNode_t *snode;
    uint32_t i;
    switch (node->type)
    {
        case hNode:
            hnode = (hNode_t *)node;
            for (i=0; i<hnode->hListSize; i++)
            {
#if ERROR_ON_UNITIALIZED_NODE == 1
                SETTINGS_ASSERT_TRUE(hnode->hList[i]);
#else
                if (hnode->hList[i] == 0)
                    continue;
#endif
                if (hnode->hList[i]->type == sNode)
                {
                    snode = (sNode_t *)hnode->hList[i];
                    if (snode->storage == RomStored)
                    {
                        crc = getCRC16(&ram[nodeRamBase + snode->ramOffset], snode->size, crc);
                    }
                }
            }
            break;

        case lNode:
            lnode = (lNode_t *)node;
            if (lnode->element->type == sNode)
            {
                snode = (sNode_t *)lnode->element;
                if (snode->storage == RomStored)
                {
                    crc = getCRC16(&ram[nodeRamBase + snode->ramOffset], lnode->elementRamSize * lnode->hListSize, crc);
                }
            }
            break;

        default:
            SETTINGS_ASSERT_NEVER_EXECUTE();
            break;
    }
    return crc & 0x0000FFFF;
}


static void updateNodeCRC(node_t *node, uint32_t nodeRamBase, uint32_t nodeRomBase)
{
    uint32_t crc;
    switch (node->type)
    {
        case hNode:
        case lNode:
            // Get CRC for current data
            crc = getNodeCrc(node, nodeRamBase);
            // Update stored CRC
            u32toBytesMsbFirst(&crc, &ram[nodeRamBase], NODE_CRC_SIZE);
            writeRom(nodeRomBase, nodeRamBase, NODE_CRC_SIZE);
            //SETTINGS_DEBUG("CRC update at %d", nodeRamBase + cnode->ownSize);
            break;

        default:
            SETTINGS_ASSERT_NEVER_EXECUTE();
            break;
    }
}


static resultType checkNodeCRC(node_t *node, uint32_t nodeRamBase)
{
    uint32_t storedCrc, crc;
    resultType result;
    switch (node->type)
    {
        case hNode:
        case lNode:
            // Get stored CRC
            bytesToU32MsbFirst(&ram[nodeRamBase], &storedCrc, NODE_CRC_SIZE);
            // Get CRC for current data
            crc = getNodeCrc(node, nodeRamBase);
            if (crc != storedCrc)
            {
                result = Result_ValidateError;
            }
            else
            {
                result = Result_OK;
            }
            break;

        default:
            SETTINGS_ASSERT_NEVER_EXECUTE();
            result = Result_WrongNodeType;
            break;
    }
    return result;
}


resultType invalidateNodeCrc(node_t *node, uint32_t nodeRamBase, uint32_t nodeRomBase, uint8_t wholeTree)
{
    hNode_t *hnode;
    lNode_t *lnode;
    uint16_t i;
    uint32_t ramAddr, romAddr;
    resultType result = Result_OK;
    switch (node->type)
    {
        case hNode:
            hnode = (hNode_t *)node;
            // Set invalid CRC
            memset(&ram[nodeRamBase], 0, NODE_CRC_SIZE);
            writeRom(nodeRomBase, nodeRamBase, NODE_CRC_SIZE);
            result = (resultType)(result | Result_UpdatedRom);
            if (!wholeTree)
                break;
            // Init nodes
            for (i=0; i<hnode->hListSize; i++)
            {
#if ERROR_ON_UNITIALIZED_NODE == 1
                SETTINGS_ASSERT_TRUE(hnode->hList[i]);
#else
                if (hnode->hList[i] == 0)
                    continue;
#endif
                pushArg(argHistory, SETTINGS_MAX_DEPTH, i);
                ramAddr = nodeRamBase + hnode->hList[i]->ramOffset;
                romAddr = nodeRomBase + hnode->hList[i]->romOffset;
                result = (resultType)(result | invalidateNodeCrc(hnode->hList[i], ramAddr, romAddr, wholeTree));
                popArg(argHistory, SETTINGS_MAX_DEPTH);
            }
            break;

        case lNode:
            lnode = (lNode_t *)node;
            // Set invalid CRC
            memset(&ram[nodeRamBase], 0, NODE_CRC_SIZE);
            writeRom(nodeRomBase, nodeRamBase, NODE_CRC_SIZE);
            result = (resultType)(result | Result_UpdatedRom);
            if (!wholeTree)
                break;
            // Init list nodes
            for (i=0; i<lnode->hListSize; i++)
            {
                // Here ROM offset may be page-aligned for hierarchy nodes if necessary
                pushArg(argHistory, SETTINGS_MAX_DEPTH, i);
                ramAddr = nodeRamBase + lnode->element->ramOffset + (lnode->elementRamSize * i);
                romAddr = nodeRomBase + lnode->element->romOffset + (lnode->elementRomSize * i);
                result = (resultType)(result | invalidateNodeCrc(lnode->element, ramAddr, romAddr, wholeTree));
                popArg(argHistory, SETTINGS_MAX_DEPTH);
            }
            break;

        case sNode:
            break;

        default:
            SETTINGS_ASSERT_NEVER_EXECUTE();
            break;
    }
    return result;
}


//-----------------------------------------------------------------//
//-----------------------------------------------------------------//
// CRC
//-----------------------------------------------------------------//
//-----------------------------------------------------------------//

void makeCRC16Table(void)
{
    uint16_t r;
    uint32_t i,j;
    for(i=0; i<256; i++)
    {
        r = i << 8;
        for(j=0; j<8; j++)
        {
            if (r & (1 << 15))
                r = (r << 1) ^ 0x8005;
            else
                r <<= 1;
        }
        crcTable[i] = r;
   }
}


uint16_t getCRC16(uint8_t *data, uint16_t len, uint16_t crc)
{
    while(len--)
    {
        crc = crcTable[((crc>>8)^*data++) & 0xFF] ^ (crc<<8);
    }
    return crc;
}


//-----------------------------------------------------------------//
//-----------------------------------------------------------------//
// Public
//-----------------------------------------------------------------//
//-----------------------------------------------------------------//

resultType settingsRequest(request_t *rqst)
{
    node_t *pNode = (node_t *)hRoot;
    hNode_t *nnode = 0;
    lNode_t *lnode = 0;
    node_t *pHostNode = (node_t *)hRoot;                // Should be updated before use
    uint32_t hostNodeRamOffset = hRoot->ramOffset;      // Should be updated before use
    uint32_t hostNodeRomOffset = hRoot->romOffset;      // Should be updated before use
    uint32_t currArg, argIndex = 0;
    uint32_t ramOffset = hRoot->ramOffset;
    uint32_t romOffset = hRoot->romOffset;
    resultType result = Result_OK;

    // Move through the node tree according to the argument list
    while(pNode->type != sNode)
    {
        if (argIndex >= SETTINGS_MAX_DEPTH - 1)
        {
            result = Result_DepthExceeded;
            break;
        }
        currArg = rqst->arg[argIndex++];
        pushArg(argHistory, SETTINGS_MAX_DEPTH, currArg);
        switch(pNode->type)
        {
            case hNode:
                pHostNode = pNode;                  // Save hNode for CRC update if required by sNode request handler
                hostNodeRamOffset = ramOffset;      // Save RAM address
                hostNodeRomOffset = romOffset;      // Save ROM address
                nnode = (hNode_t *)pNode;
                SETTINGS_ASSERT_TRUE(currArg < nnode->hListSize);
                SETTINGS_ASSERT_TRUE(nnode->hList);
                pNode = nnode->hList[currArg];
                SETTINGS_ASSERT_TRUE(pNode);
                ramOffset += pNode->ramOffset;
                romOffset += pNode->romOffset;
                break;

            case lNode:
                pHostNode = pNode;                  // Save lNode for CRC update if required by sNode request handler
                hostNodeRamOffset = ramOffset;      // Save RAM address
                hostNodeRomOffset = romOffset;      // Save ROM address
                lnode = (lNode_t *)pNode;
                SETTINGS_ASSERT_TRUE(currArg < lnode->hListSize);
                pNode = lnode->element;
                SETTINGS_ASSERT_TRUE(pNode);
                ramOffset += lnode->element->ramOffset + lnode->elementRamSize * currArg;
                romOffset += lnode->element->romOffset + lnode->elementRomSize * currArg;
                break;

            default:
                SETTINGS_ASSERT_NEVER_EXECUTE();
                result = Result_UnknownNodeType;
                break;
        }
    }
    if (result == Result_OK)
    {
        // Terminating node is found
        SETTINGS_ASSERT_TRUE(((sNode_t *)pNode)->rqHandler);
        result = ((sNode_t *)pNode)->rqHandler(rqst->rq, (sNode_t *)pNode, ramOffset, romOffset, rqst);
        if (result & Result_UpdatedRom)
        {
            // Hide ROM flag
            result = (resultType)(result & ~Result_UpdatedRom);
            updateNodeCRC(pHostNode, hostNodeRamOffset, hostNodeRomOffset);
        }
    }
    rqst->result = result;
    return result;
}


static void pushArg(uint32_t *argHistory, uint32_t argHistorySize, uint32_t arg)
{
    uint32_t i;
    for (i=argHistorySize-1; i>0; i--)
    {
        argHistory[i] = argHistory[i - 1];
    }
    argHistory[0] = arg;
}


static void popArg(uint32_t *argHistory, uint32_t argHistorySize)
{
    uint32_t i;
    for (i=0; i<argHistorySize-1; i++)
    {
        argHistory[i] = argHistory[i + 1];
    }
    argHistory[argHistorySize - 1] = 0;
}


// Get argument from request.
// Index of 0 returns last argument,
// index of 1 returns argument before last one and so on up to first argument
// Intended use: determining address of an element in a multydimensional list
uint32_t getRequestArg(uint32_t historyIndex)
{
    SETTINGS_ASSERT_TRUE(historyIndex < SETTINGS_MAX_DEPTH);
    return argHistory[historyIndex];
}


// Get cached values for callback functions
// Intended use: using new value for change event
// Every handler should use cache as appropriate
callbackCache_t *getCallbackCache(void)
{
    return &callbackCache;
}


//-----------------------------------------------------------------//
//-----------------------------------------------------------------//
// Public helpers
//-----------------------------------------------------------------//
//-----------------------------------------------------------------//
#if USE_SETTINGS_MEMORY_ALLOC == 1
// Simple memory management
void *settingsAlloc(uint32_t size)
{
    void *pRam = &allocRam[allocRamAddr];
    uint32_t nextAddr = allocRamAddr + size;
    if (nextAddr <= allocRamSize)
    {
        // OK
        allocRamAddr = nextAddr;
        memset(pRam, 0, size);
    }
    else
    {
        // Out of memory
        pRam = 0;
    }
    return pRam;
}


uint32_t getAllocMemoryUsed(void)
{
    return allocRamAddr;
}
#endif


#if ENABLE_NODE_CONSTRUCTORS == 1
sNode_t *createSNode(uint16_t size)
{
    sNode_t *snode = (sNode_t *)SETTINGS_ALLOCATE(sizeof(sNode_t));
    SETTINGS_ASSERT_TRUE(snode != 0);
    snode->type = sNode;
    snode->size = size;
    return snode;
}


hNode_t *createHNode(uint16_t elementsCount)
{
    hNode_t *hnode = (hNode_t *)SETTINGS_ALLOCATE(sizeof(hNode_t));
    SETTINGS_ASSERT_TRUE(hnode != 0);
    hnode->type = hNode;
    hnode->hList = (node_t **)SETTINGS_ALLOCATE(elementsCount * sizeof(node_t *));
    hnode->hListSize = elementsCount;
    SETTINGS_ASSERT_TRUE(hnode->hList != 0);
    return hnode;
}


lNode_t *createLNode(uint16_t elementsCount, void *node)
{
    lNode_t *lnode = (lNode_t *)SETTINGS_ALLOCATE(sizeof(lNode_t));
    SETTINGS_ASSERT_TRUE(lnode != 0);
    lnode->type = lNode;
    lnode->element = (node_t *)node;
    lnode->hListSize = elementsCount;
    return lnode;
}


void addToHList(hNode_t *hnode, uint32_t index, void *node)
{
    if (index < hnode->hListSize)
    {
        if (hnode->hList[index] == 0)
        {
            hnode->hList[index] = (node_t *)node;
        }
        else
        {
            SETTINGS_ASSERT_NEVER_EXECUTE();
        }
    }
    else
    {
        SETTINGS_ASSERT_NEVER_EXECUTE();
    }
}

#endif  // ENABLE_NODE_CONSTRUCTORS

//-----------------------------------------------------------------//
//-----------------------------------------------------------------//
//-----------------------------------------------------------------//
//-----------------------------------------------------------------//



validateResult validateU32(uint32_t value, struct u32Prm_t *prm)
{
    return ((prm->minValue <= value) && (value <= prm->maxValue)) ? ValidateOk : ValidateErr;
}


resultType handleRequestU32(rqType rq, struct sNode_t *pNode, uint32_t nodeRamBase, uint32_t nodeRomBase, request_t *rqst)
{
    resultType result = Result_OK;
    uint32_t val32;
    uint32_t *pVal32;
    //SETTINGS_DEBUG("U32 node rq %d, node size %d, ram %d, rom %d", rq, pNode->size, nodeRamBase, nodeRomBase);
    switch (rq)
    {
        case rqRead:
            if (rqst->raw)
            {
                memcpy(rqst->raw, &ram[nodeRamBase], pNode->size);
            }
            else
            {
                pVal32 = (uint32_t *)rqst->val.i32;
                bytesToU32MsbFirst(&ram[nodeRamBase], &val32, pNode->size);
                *pVal32 = val32;
            }
            break;

        case rqApplyNoCb:
        case rqApply:
        case rqStore:
        case rqWriteNoCb:
        case rqWrite:
            if (rqst->raw)
            {
                // Serialize to validate
                bytesToU32MsbFirst(rqst->raw, &val32, pNode->size);
            }
            else
            {
                pVal32 = (uint32_t *)rqst->val.i32;
                val32 = *pVal32;
            }
            if (rq & rqApply)
            {
                if (validateU32(val32, &pNode->varData.u32Prm) == ValidateOk)
                {
                    u32toBytesMsbFirst(&val32, &ram[nodeRamBase], pNode->size);
                    // Request arguments may be provided by GetRequestArg() if required by callback
                    // New value may be directly obtained using GetCallbackCache() if required by callback
                    callbackCache.i32 = val32;
                    if (pNode->changeCallback)
                        pNode->changeCallback(rq, argHistory[0]);
                }
                else
                {
#if ERROR_ON_VALIDATE_FAILED == 1
                    SETTINGS_ASSERT_NEVER_EXECUTE();
#endif
                    result = Result_ValidateError;
                    break;
                }
            }
            if (rq & rqStore)
            {
                if (pNode->storage == RomStored)
                {
                    writeRom(nodeRomBase, nodeRamBase, pNode->size);
                    result = (resultType)(result | Result_UpdatedRom);
                }
            }
            break;

        case rqValidate:
            if (rqst->raw)
            {
                // Serialize to validate
                bytesToU32MsbFirst(rqst->raw, &val32, pNode->size);
            }
            else
            {
                pVal32 = (uint32_t *)rqst->val.i32;
                val32 = *pVal32;
            }
            result = (validateU32(*pVal32, &pNode->varData.u32Prm) == ValidateOk) ? Result_OK : Result_ValidateError;
            break;

        case rqGetMin:
            if (rqst->raw)
            {
                u32toBytesMsbFirst(&pNode->varData.u32Prm.minValue, rqst->raw, pNode->size);
            }
            else
            {
                pVal32 = (uint32_t *)rqst->val.i32;
                *pVal32 = pNode->varData.u32Prm.minValue;
            }
            break;

        case rqGetMax:
            if (rqst->raw)
            {
                u32toBytesMsbFirst(&pNode->varData.u32Prm.maxValue, rqst->raw, pNode->size);
            }
            else
            {
                pVal32 = (uint32_t *)rqst->val.i32;
                *pVal32 = pNode->varData.u32Prm.maxValue;
            }
            break;

        case rqGetSize:
            if (rqst->raw)
            {
                u32toBytesMsbFirst(&pNode->size, rqst->raw, 4);
            }
            else
            {
                pVal32 = (uint32_t *)rqst->val.i32;
                *pVal32 = pNode->size;
            }
            break;

        case rqRestoreValidate:
            if (pNode->storage == RomStored)
            {
                readRom(nodeRamBase, nodeRomBase, pNode->size);
                bytesToU32MsbFirst(&ram[nodeRamBase], &val32, pNode->size);
                result = (validateU32(val32, &pNode->varData.u32Prm) == ValidateOk) ? Result_OK : Result_ValidateError;
            }
            else
            {
                val32 = pNode->varData.u32Prm.defaultValue;
                u32toBytesMsbFirst(&val32, &ram[nodeRamBase], pNode->size);
            }
            break;

        case rqRestoreDefault:
            val32 = pNode->varData.u32Prm.defaultValue;
            u32toBytesMsbFirst(&val32, &ram[nodeRamBase], pNode->size);
            if (pNode->storage == RomStored)
            {
                writeRom(nodeRomBase, nodeRamBase, pNode->size);
                result = (resultType)(result | Result_UpdatedRom);
            }
            break;

        default:
            result = Result_WrongRequestType;
            break;
    }
    return result;
}


resultType handleRequestCharArray(rqType rq, struct sNode_t *pNode, uint32_t nodeRamBase, uint32_t nodeRomBase, request_t *rqst)
{
    resultType result = Result_OK;
    //SETTINGS_DEBUG("Char node rq %d, node size %d, ram %d, rom %d", rq, pNode->size, nodeRamBase, nodeRomBase);
    switch (rq)
    {
        case rqRead:
            memcpy(rqst->raw, &ram[nodeRamBase], pNode->size);
            break;

        case rqApplyNoCb:
        case rqApply:
        case rqStore:
        case rqWriteNoCb:
        case rqWrite:
            if (rq & rqApply)
            {
                if (rqst->raw != 0)
                {
                    memcpy(&ram[nodeRamBase], rqst->raw, pNode->size);
                    // Request arguments may be provided by GetRequestArg() if required by callback
                    // New value may be directly obtained using GetCallbackCache() if required by callback
                    callbackCache.str = (char *)rqst->raw;
                    // Request arguments may be provided by GetRequestArg() if required by callback
                    if (pNode->changeCallback)
                        pNode->changeCallback(rq, argHistory[0]);
                }
                else
                {
#if ERROR_ON_VALIDATE_FAILED == 1
                    SETTINGS_ASSERT_NEVER_EXECUTE();
#endif
                    result = Result_ValidateError;
                    break;
                }
            }
            if (rq & rqStore)
            {
                if (pNode->storage == RomStored)
                {
                    writeRom(nodeRomBase, nodeRamBase, pNode->size);
                    result = (resultType)(result | Result_UpdatedRom);
                }
            }
            break;

        case rqValidate:
            // Char arrays are assumed to be correct
            // If validate is required for specific case, custom request handler should be used
            result = (rqst->raw != 0) ? Result_OK : Result_ValidateError;
            break;

        case rqGetMin:
        case rqGetMax:
            SETTINGS_ASSERT_NEVER_EXECUTE();
            result = Result_WrongRequestType;
            break;

        case rqGetSize:
            if (rqst->raw)
            {
                u32toBytesMsbFirst(&pNode->size, rqst->raw, 4);
            }
            else
            {
                uint32_t *pVal32 = (uint32_t *)rqst->val.i32;
                *pVal32 = pNode->size;
            }
            break;

        case rqRestoreValidate:
            if (pNode->storage == RomStored)
            {
                readRom(nodeRamBase, nodeRomBase, pNode->size);
            }
            else
            {
                if (pNode->varData.charArrayPrm.defaultValue)
                    memcpy(&ram[nodeRamBase], pNode->varData.charArrayPrm.defaultValue, pNode->size);
                else
                    memset(&ram[nodeRamBase], 0, pNode->size);
            }
            break;

        case rqRestoreDefault:
            if (pNode->varData.charArrayPrm.defaultValue)
                memcpy(&ram[nodeRamBase], pNode->varData.charArrayPrm.defaultValue, pNode->size);
            else
                memset(&ram[nodeRamBase], 0, pNode->size);
            if (pNode->storage == RomStored)
            {
                writeRom(nodeRomBase, nodeRamBase, pNode->size);
                result = (resultType)(result | Result_UpdatedRom);
            }
            break;

        default:
            result = Result_WrongRequestType;
            break;
    }
    return result;
}


#if ENABLE_NODE_CONSTRUCTORS == 1
sNode_t *u32Node(uint8_t accessLevel, storageType storage,
                       uint32_t minValue, uint32_t maxValue, uint32_t defaultValue,
                       onChangeCallback changeCallback)
{
    sNode_t *node = createSNode(4);
    node->rqHandler = handleRequestU32;
    node->accessLevel = accessLevel;
    node->storage = storage;
    node->changeCallback = changeCallback;
    node->varData.u32Prm.defaultValue = defaultValue;
    node->varData.u32Prm.minValue = minValue;
    node->varData.u32Prm.maxValue = maxValue;
    return node;
}


sNode_t *u16Node(uint8_t accessLevel, storageType storage,
                       uint32_t minValue, uint32_t maxValue, uint32_t defaultValue,
                       onChangeCallback changeCallback)
{
    sNode_t *node = createSNode(2);
    node->rqHandler = handleRequestU32;
    node->accessLevel = accessLevel;
    node->storage = storage;
    node->changeCallback = changeCallback;
    node->varData.u32Prm.defaultValue = defaultValue;
    node->varData.u32Prm.minValue = minValue;
    node->varData.u32Prm.maxValue = maxValue;
    return node;
}


sNode_t *u8Node(uint8_t accessLevel, storageType storage,
                       uint32_t minValue, uint32_t maxValue, uint32_t defaultValue,
                       onChangeCallback changeCallback)
{
    sNode_t *node = createSNode(1);
    node->rqHandler = handleRequestU32;
    node->accessLevel = accessLevel;
    node->storage = storage;
    node->changeCallback = changeCallback;
    node->varData.u32Prm.defaultValue = defaultValue;
    node->varData.u32Prm.minValue = minValue;
    node->varData.u32Prm.maxValue = maxValue;
    return node;
}


sNode_t *charNode(uint8_t accessLevel, storageType storage,
                       uint32_t size, const char *defaultValue,
                       onChangeCallback changeCallback)
{
    sNode_t *node = createSNode(size);
    node->rqHandler = handleRequestCharArray;
    node->accessLevel = accessLevel;
    node->storage = storage;
    node->changeCallback = changeCallback;
    node->varData.charArrayPrm.defaultValue = defaultValue;
    return node;
}
#endif








