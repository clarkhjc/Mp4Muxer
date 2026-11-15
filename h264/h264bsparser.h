#ifndef _H264_BS_PARSER_H_
#define _H264_BS_PARSER_H_

#include "bsreader.h"

typedef struct AVRational {
    int num; ///< Numerator
    int den; ///< Denominator
} AVRational;

/**
 * @brief record informations of a whole h264 packet:
 * 
 * for example sei/sps/pps/idr nalus for IDR packets;
 * non-idr nalu for non-IDR packets.
 * 
 * informations include packet type(IDR or non-IDR), packet size(len)
 * , as well as the offset of this packet in mp4 file.
 */
typedef struct H264PacketInfo {
    u32 type;
    i64 len;
    off_t file_offset;
} H264PacketInfo;

/**
 * @brief h264 parse context
 * store all of h264 packets;
 * sps data and pps data;
 * as well as syntax element's values of sps which parsed from sps.
 */
typedef struct H264ParserContext {
    H264PacketInfo *pkts;
    u32 pkt_counts;

    u8 *sps_buf;
    i32 sps_len;

    u8 *pps_buf;
    i32 pps_len;

    i32 width;
    i32 height;
    i32 packet_nums;

    /* ported from ffmpeg*/
    u8 id;
    u8 profile_idc;
    u8 level_idc;
    u8 constraint_set_flags;
    u8 chroma_format_idc;
    u8 bit_depth_luma;
    u8 bit_depth_chroma;
    u8 frame_mbs_only_flag;
    AVRational sar;
} H264ParserContext;

int ff_avc_decode_sps(H264ParserContext *sps, const u8 * const buf, int buf_size);

#endif