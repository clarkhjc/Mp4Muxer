#ifndef _BS_READER_H_
#define _BS_READER_H_

#include <limits.h>
#include "util.h"

/*********below codes are ported from ffmpeg****************/

#define AV_INPUT_BUFFER_PADDING_SIZE 64
#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define av_assert2(cond) ((void)0)

#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

#define AV_BSWAP16C(x) (((x) << 8 & 0xff00)  | ((x) >> 8 & 0x00ff))
#define AV_BSWAP32C(x) (AV_BSWAP16C(x) << 16 | AV_BSWAP16C((x) >> 16))

typedef union {
    u32 u32;
    u16 u16[2];
    u8  u8 [4];
    float f32;
} av_alias32;

typedef struct GetBitContext {
    const u8 *buffer, *buffer_end;
    int index;
    int size_in_bits;
    int size_in_bits_plus8;
} GetBitContext;

void skip_bits(GetBitContext *s, int n);
void skip_bits1(GetBitContext *s);
unsigned int get_bits1(GetBitContext *s);
unsigned int get_bits(GetBitContext *s, int n);
int get_bitsz(GetBitContext *s, int n);
int get_ue_golomb(GetBitContext *gb) ;
int get_se_golomb(GetBitContext *gb);
int init_get_bits(GetBitContext *s, const u8 *buffer, int bit_size);
int init_get_bits8(GetBitContext *s, const u8 *buffer, int byte_size);

#endif