#ifndef __UTILS_H__
#define __UTILS_H__

#include "stdint.h"

//----------- Definitions ----------//



//----------- Prototypes -----------//

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

    int32_t clip_i32(int32_t x, int32_t min, int32_t max);
    int16_t clip_i16(int16_t x, int16_t min, int16_t max);
    uint8_t clip_u8(uint8_t x, uint8_t min, uint8_t max);
    uint8_t incAndWrapU8(uint8_t x, uint8_t min, uint8_t max);
    uint8_t decAndWrapU8(uint8_t x, uint8_t min, uint8_t max);
    double clip_d(double x, double min, double max);
    float clip_f(float x, float min, float max);
	
    
    void atomic_and_not(unsigned int *pVal, unsigned int bitMask);
    void atomic_or(unsigned int *pVal, unsigned int bitMask);
	
	void dec2bcdStr(uint32_t dec, char *bcd, uint32_t num_bcd_chars);
	uint32_t bcdStr2dec(char *bcd, uint32_t num_bcd_chars);
	void ascii2bcdStr(char *ascii, char *bcd, uint32_t num_ascii_chars);
	void bcdStr2ascii(char *bcd, char *ascii, uint32_t num_ascii_chars);

	uint32_t bytes2nibbles(const uint8_t *src_bytes, uint8_t *dst_nibbles, uint32_t nibble_count);
	uint32_t nibbles2bytes(const uint8_t *src_nibbles, uint8_t *dst_bytes, uint32_t nibble_count);

	void bin2ascii(const uint8_t *src_bytes, char *dst_bytes, uint32_t count);
	void ascii2bin(const char *src_bytes, uint8_t *dst_bytes, uint32_t count);
	
    uint32_t getRand8bit(void);

    void u32toBytesLsbFirst(uint32_t *number, uint8_t *bytes, uint32_t count);
    void bytesToU32LsbFirst(uint8_t *bytes, uint32_t *number, uint32_t count);
    void u32toBytesMsbFirst(uint32_t *number, uint8_t *bytes, uint32_t count);
    void bytesToU32MsbFirst(uint8_t *bytes, uint32_t *number, uint32_t count);

    uint32_t bitcmp(uint8_t *data1, uint8_t *data2, uint32_t byte_count);

    void swap16(uint32_t *pInData, uint32_t *pOutData, uint32_t count);

#ifdef __cplusplus
}
#endif // __cplusplus



//----------- Externals ------------//



#endif //__UTILS_H__	
