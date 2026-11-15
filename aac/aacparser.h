#ifndef _AAC_PARSER_H_
#define _AAC_PARSER_H_

#include "aacbsparser.h"
#include <stdio.h>
#include <stdlib.h>

int aac_init(AACParserContext **ppctx);

int aac_parse(const char *file_path, AACParserContext *pctx);

int aac_free(AACParserContext **ppctx);

u32 compute_avg_bitrate(AACParserContext *pctx, u32 time_scale, i64 duration);

u32 create_audio_specific_config(ADTSFixedHeader f_header, u8 *dst);

#endif