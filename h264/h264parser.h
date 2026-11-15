#ifndef _H264_PARSER_H_
#define _H264_PARSER_H_

#include <stdio.h>
#include <stdlib.h>
#include "h264bsparser.h"

/*
* refer to "Rec. ITU-T H.264 when does h264 bitstream parsing"
*/

int h264_init(H264ParserContext **ppctx);

int h264_parse(const char *file_path, H264ParserContext *pctx);

int h264_free(H264ParserContext **ppctx);

u32 annexb2mp4(const u8 *src, u32 src_len, u8 *dst);

#endif