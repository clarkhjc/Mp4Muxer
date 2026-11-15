#ifndef _AAC_BS_PARSER_H_
#define _AAC_BS_PARSER_H_

#include "bsreader.h"

#define ADTS_HEADER_SIZE 7

typedef struct ADTSFixedHeader {
    u16 syncword;           // 12bits  bslbf
    u8  id;                 // 1 bits  bslbf
    u8  layer;              // 2 bits  uimsbf
    u8  protection_absent;  // 1 bits  bslbf
    u8  profile;            // 2 bits  uimsbf
    u8  sampling_freq_index;// 4 bits  uimsbf
    u8  private_bit;        // 1 bits  bslbf
    u8  chan_configuration; // 3 bits  uimsbf
    u8  original_copy;      // 1 bits  bslbf
    u8  home;               // 1 bits  bslbf
} ADTSFixedHeader;

typedef struct ADTSVariableHeader {
    u8  copyright_id_bit;   // 1 bits  bslbf
    u8  copyright_id_start; // 1 bits  bslbf
    u16 aac_frame_length;   // 13bits  bslbf
    u16 adts_buf_fulless;   // 11bits  bslbf
    u8  number_aac_frames;  // 2 bits  uimsbf
} ADTSVariableHeader;

typedef struct ADTSHeader {
    ADTSFixedHeader f_header;
    ADTSVariableHeader v_header;
    u16 CRC;
} ADTSHeader;

typedef struct AACPacketInfo {
    ADTSHeader header;
    u32 frame_size;
    off_t file_offset;
} AACPacketInfo;

typedef struct AACParserContext {
    AACPacketInfo *pkts;
    u32 pkt_counts;
} AACParserContext;

int parse_adts_header(const u8 *src, ADTSHeader *header);
#endif