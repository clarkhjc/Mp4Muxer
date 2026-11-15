#include "bsreader.h"

static u32 av_bswap32(u32 x)
{
    return AV_BSWAP32C(x);
}

void skip_bits(GetBitContext *s, int n)
{
    // s->index = s->index + n;

    unsigned int re_index = s->index;
    unsigned int re_size_plus8 = s->size_in_bits_plus8;

    re_index = FFMIN(re_size_plus8, re_index + n);

    s->index = re_index;
}

void skip_bits1(GetBitContext *s)
{
    skip_bits(s, 1);
}

unsigned int get_bits1(GetBitContext *s)
{
    unsigned int index = s->index;
    u8 result = s->buffer[index >> 3];
    result <<= index & 7;
    result >>= 7;

    if (s->index < s->size_in_bits_plus8)
        index++;
    s->index = index;

    return result;
}

/**
    B3                      | B2                      | B1                     | B0
    a7 a6 a5 a4 a3 a2 a1 a0 | b7 b6 b5 b4 b3 b2 b1 b0 | c7 c6 c5 c4 c3 c2 1 c0 | e7 e6 e5 e4 e3 e2 e1 e0
    *perform av_bswap32*
    B0                      | B1                      | B2                      | B3
    e7 e6 e5 e4 e3 e2 e1 e0 | c7 c6 c5 c4 c3 c2 c1 c0 | b7 b6 b5 b4 b3 b2 b1 b0 | a7 a6 a5 a4 a3 a2 a1 a0
    *assume re_index = 0, n = 9(we want 9bits)*
    *perform right shift(>> 32 - n)*
    B0                      | B1                     | B2                      | B3
    00 00 00 00 00 00 00 00 | 00 00 00 00 00 0 00 00 | 00 00 00 00 00 00 00 e7 | e6 e5 e4 e3 e2 e1 e0 c7

    for example:
    u32 input    = 0x000080FF;
    u32 re_cache = av_bswap32(input) << (0 & 7);
    u32 tmp      = re_cache >> 23;
    we got 0x1FF.
 */
unsigned int get_bits(GetBitContext *s, int n)
{
    register unsigned int tmp;
    unsigned int re_index = s->index;
    unsigned int re_cache;
    unsigned int re_size_plus8 = s->size_in_bits_plus8;

    re_cache = av_bswap32(((const av_alias32*)(s->buffer + (re_index >> 3)))->u32) << (re_index & 7);

    tmp = ((u32)re_cache) >> (32 - (n));

    re_index = FFMIN(re_size_plus8, re_index + n);

    s->index = re_index;
    return tmp;
}

int get_bitsz(GetBitContext *s, int n)
{
    return n ? get_bits(s, n) : 0;
}

int get_ue_golomb(GetBitContext *gb) {
    // get leading zero numbers m
    int m;
    for (m = 0; m < 32 && !get_bits1(gb); m++)
        ;
    // codeNum = 2^m - 2^0 + bits[0...m-1]
    return get_bitsz(gb, m) + (1 << m) - 1;
}

int get_se_golomb(GetBitContext *gb) {
    int v = get_ue_golomb(gb) + 1;
    int sign = -(v & 1);
    return ((v >> 1) ^ sign) - sign;
}

/**
 * Initialize GetBitContext.
 * @param buffer bitstream buffer, must be AV_INPUT_BUFFER_PADDING_SIZE bytes
 *        larger than the actual read bits because some optimized bitstream
 *        readers read 32 or 64 bit at once and could read over the end
 * @param bit_size the size of the buffer in bits
 * @return 0 on success, -1 if the buffer_size would overflow.
 */
int init_get_bits(GetBitContext *s, const u8 *buffer,
                                int bit_size)
{
    int buffer_size;
    int ret = 0;

    if (bit_size >= INT_MAX - FFMAX(7, AV_INPUT_BUFFER_PADDING_SIZE*8) || bit_size < 0 || !buffer) {
        bit_size    = 0;
        buffer      = NULL;
        ret         = -1;
    }

    buffer_size = (bit_size + 7) >> 3;

    s->buffer             = buffer;
    s->size_in_bits       = bit_size;
    s->size_in_bits_plus8 = bit_size + 8;
    s->buffer_end         = buffer + buffer_size;
    s->index              = 0;

    return ret;
}

/**
 * Initialize GetBitContext.
 * @param buffer bitstream buffer, must be AV_INPUT_BUFFER_PADDING_SIZE bytes
 *        larger than the actual read bits because some optimized bitstream
 *        readers read 32 or 64 bit at once and could read over the end
 * @param byte_size the size of the buffer in bytes
 * @return 0 on success, AVERROR_INVALIDDATA if the buffer_size would overflow.
 */
int init_get_bits8(GetBitContext *s, const u8 *buffer,
                                 int byte_size)
{
    if (byte_size > INT_MAX / 8 || byte_size < 0)
        byte_size = -1;
    return init_get_bits(s, buffer, byte_size * 8);
}