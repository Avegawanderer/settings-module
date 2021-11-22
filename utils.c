

#include "utils.h"



int32_t clip_i32(int32_t x, int32_t min, int32_t max)
{
    if (x < min)
        x = min;
    else if (x > max)
        x = max;
    return x;
}


int16_t clip_i16(int16_t x, int16_t min, int16_t max)
{
    if (x < min)
        x = min;
    else if (x > max)
        x = max;
    return x;
}

uint8_t clip_u8(uint8_t x, uint8_t min, uint8_t max)
{
    if (x < min)
        x = min;
    else if (x > max)
        x = max;
    return x;
}


uint8_t incAndWrapU8(uint8_t x, uint8_t min, uint8_t max)
{
    if (x == max)
        x = min;
    else 
        x++;
    return x;
}


uint8_t decAndWrapU8(uint8_t x, uint8_t min, uint8_t max)
{
    if (x == min)
        x = max;
    else 
        x--;
    return x;
}


double clip_d(double x, double min, double max)
{
    if (x < min)
        x = min;
    else if (x > max)
        x = max;
    return x;
}


float clip_f(float x, float min, float max)
{
    if (x < min)
        x = min;
    else if (x > max)
        x = max;
    return x;
}



void dec2bcdStr(uint32_t dec, char *bcd, uint32_t num_bcd_chars)
{
    uint32_t c;
    int32_t i,j;
    for (i=num_bcd_chars-1; i>=0; i--)
    {
        bcd[i] = 0;
        for (j=0; j<2; j++)
        {
            if (dec)
            {
                c = (dec % 10);
                dec /= 10;
                if (j == 1)
                    c <<= 4;
                bcd[i] |= c;
            }
        }
    }
}


uint32_t bcdStr2dec(char *bcd, uint32_t num_bcd_chars)
{
    uint32_t m = 1;
    int32_t i, j;
    uint32_t dec = 0;
    char c;
    for (i=num_bcd_chars-1; i>=0; i--)
    {
        c = bcd[i];
        for (j=0; j<2; j++)
        {
            dec += (c & 0x0F) * m;
            c >>= 4;
            m *= 10;
        }
    }
    return dec;
}

// ASCII                    BCD
// "0x31 0x32 0x33 0x34" -> "0x12 0x34"
void ascii2bcdStr(char *ascii, char *bcd, uint32_t num_ascii_chars)
{
    uint32_t i;
    char bcd_dig_mask = 0xF0;
    char c;
    for (i=0; i<num_ascii_chars; i++)
    {
        c = ascii[i];
        c -= 0x30;
        c &= 0x0F;
        *bcd &= ~bcd_dig_mask;
        *bcd |= (i & 0x01) ? c : c << 4;
        bcd_dig_mask = ~bcd_dig_mask;
        if (i & 0x01)
            bcd++;
    }
}

// BCD            ASCII
// "0x05 0x67" -> "0x30 0x35 0x36 0x37"
void bcdStr2ascii(char *bcd, char *ascii, uint32_t num_ascii_chars)
{
    uint32_t i;
    char c;
    for (i=0; i<num_ascii_chars; i++)
    {
        if ((i & 0x01) == 0)
        {
            c = *bcd++;
        }
        ascii[i] = (((i & 0x01) == 0) ? c >> 4 : c) & 0x0F;
        ascii[i] += 0x30;
    }
}


// Unpack packed into bytes nibbles to separate bytes
uint32_t bytes2nibbles(const uint8_t *src_bytes, uint8_t *dst_nibbles, uint32_t nibble_count)
{
    uint8_t temp8u;
    uint32_t nibbles_used = 0;
    uint8_t shift = 0;
    while(nibble_count--)
    {
        if (shift == 0)
        {
            temp8u = *src_bytes++;
            *dst_nibbles++ = (temp8u >> 4) & 0x0F;
        }
        else
        {
            *dst_nibbles++ = (temp8u) & 0x0F;
        }
        nibbles_used++;
        shift ^= 0x01;
    }
    return nibbles_used;
}


uint32_t nibbles2bytes(const uint8_t *src_nibbles, uint8_t *dst_bytes, uint32_t nibble_count)
{
    uint8_t temp8u;
    uint8_t shift = 0;
    uint32_t bytes_used = 0;
    while (nibble_count--)
    {
        temp8u = *src_nibbles++;
        if (shift == 0)
        {
            *dst_bytes = ((temp8u & 0x0F) << 4);
            bytes_used++;
        }
        else
        {
            *dst_bytes++ |= (temp8u & 0x0F);
        }
        shift ^= 0x01;
    }
    return bytes_used;
}


void bin2ascii(const uint8_t *src_bytes, char *dst_bytes, uint32_t count)
{
    uint8_t c;
    while(count--)
    {
        c = *src_bytes++ & 0x0F;
        *dst_bytes++ = c + ((c <= 9) ? 0x30 : 0x37);
    }
}

void ascii2bin(const char *src_bytes, uint8_t *dst_bytes, uint32_t count)
{
    uint8_t c;
    while(count--)
    {
        c = (uint8_t)*src_bytes++;
        if ((c >= 0x30) && (c <= 0x39))			// 0 - 9
        	c -= 0x30;
        else if ((c >= 0x41) && (c <= 0x46))	// A - F
        	c -= 0x37;
      	else if ((c >= 0x61) && (c <= 0x66))	// a - f
        	c -= 0x57;
        *dst_bytes++ = c;
    }
}

uint32_t getRand8bit(void)
{
    
    uint32_t rand = 42;//OSTimeGet();
    return rand & 0xFF;
}


// Converting 32-bit word into byte array
// First byte is the least significant
// count [bytes]
void u32toBytesLsbFirst(uint32_t *number, uint8_t *bytes, uint32_t count)
{
    uint32_t temp32u = *number++;
    uint8_t byteCnt = 4;
    while(count--)
    {
        if (byteCnt == 0)
        {
            byteCnt = 4;
            temp32u = *number++;
        }
        byteCnt--;
        *bytes++ = (temp32u >> (8 * (3 - byteCnt))) & 0xFF;
    }
}


// Converting byte array into 32-bit word
// First byte is the least significant
// count [bytes]
void bytesToU32LsbFirst(uint8_t *bytes, uint32_t *number, uint32_t count)
{
    uint32_t temp32u = 0;
    uint8_t byteCnt = 4;
    while (count--)
    {
        if (byteCnt == 0)
        {
            byteCnt = 4;
            *number++ = temp32u;
            temp32u = 0;
        }
        byteCnt--;
        temp32u |= ((uint32_t)*bytes++) << (8 * (3 - byteCnt));
    }
    *number = temp32u;
}


// Converting 32-bit word into byte array
// First byte is the most significant
// count [bytes]
void u32toBytesMsbFirst(uint32_t *number, uint8_t *bytes, uint32_t count)
{
    uint32_t temp32u = *number++;
    uint8_t byteCnt = ((count & 0x3) == 0) ? 4 : count & 0x3;
    while(count--)
    {
        if (byteCnt == 0)
        {
            byteCnt = 4;
            temp32u = *number++;
        }
        byteCnt--;
        *bytes++ = (temp32u >> (byteCnt * 8)) & 0xFF;
    }
}


// Converting byte array into 32-bit word
// First byte is the most significant
// count [bytes]
void bytesToU32MsbFirst(uint8_t *bytes, uint32_t *number, uint32_t count)
{
    uint32_t temp32u = 0;
    uint8_t byteCnt = ((count & 0x3) == 0) ? 4 : count & 0x3;
    while (count--)
    {
        if (byteCnt == 0)
        {
            byteCnt = 4;
            *number++ = temp32u;
            temp32u = 0;
        }
        byteCnt--;
        temp32u |= ((uint32_t)*bytes++) << (8 * byteCnt);
    }
    *number = temp32u;
}

