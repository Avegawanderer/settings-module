#ifndef SETTINGS_H
#define SETTINGS_H

#include "settings_public.h"



enum ParamGroups {
    pGroup_B0,
    pGroup_B1,
    pGroup_B2,
    pGroup_Count
};


enum pGroup_B0 {
    b0param_C0,
    b0param_C1,
    b0param_Count,
};


// Length of C2 params
#define C2_SIZE         20

// Size of C2 param array
#define C2_NODES_COUNT  35




#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

    // Init and common functions
    resultType initSettings(uint8_t useDefaults);
    void resetSettingsToDefaults(void);
    void flushSettingsToRom(void);

    // Functions below are provided by settings_private
    resultType settingsRequest(request_t *rqst);
    uint32_t getRequestArg(uint32_t historyIndex);
    callbackCache_t *getCallbackCache(void);

    // Convenience aliases
    int32_t settings_ReadI32(uint32_t pGroup, uint32_t param);
    int32_t settings_ReadI32_4(uint32_t pGroup, uint32_t param, uint32_t param2, uint32_t param3, uint32_t param4);
    void settings_WriteI32(uint32_t pGroup, uint32_t param, int32_t value);
    void settings_WriteI32_4(uint32_t pGroup, uint32_t param, uint32_t param2, uint32_t param3, uint32_t param4, int32_t value);
    void settings_WriteI32NoCbf(uint32_t pGroup, uint32_t param, int32_t value);

    void settings_ReadStr(uint32_t pGroup, uint32_t param, char *str);
    void settings_WriteStr(uint32_t pGroup, uint32_t param, char *str);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // SETTINGS_H
