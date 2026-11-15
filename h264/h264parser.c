#include "h264parser.h"

/**
 * @brief init h264 parser context
 * 
 * @param ppctx h264 parser context
 */
int h264_init(H264ParserContext **ppctx) {
    u32 size = sizeof(H264ParserContext);

    H264ParserContext *p = (H264ParserContext *)calloc(size, 1);
    *ppctx = p;

    return 0;
}

/**
 * @brief parse h264 bitstreams then get basic informations and packet sizes 
 * 
 * @param file_path h264 file path
 * @param pctx      store the parsed informations
 */
int h264_parse(const char *file_path, H264ParserContext *pctx) {
    FILE *pf = fopen(file_path, "rb");
    if (!pf) {
        fprintf(stderr, "open %s failed.\n", file_path);
        return -1;
    }

    u32 nalu_type, next_24bits;
    u8 nalu_header, code;
    i64 last_pos = 0;
    off_t prev_nalu_pos = 0;
    i32 packet_nums = 0;

    while(!feof(pf)) {
        // find start codes 0x000001
        next_24bits = 0xFFFFFFFF;
        u8 start_code_nums = 0;
        do {
            fread(&code, 1, 1, pf);
            start_code_nums++;
            next_24bits = ((next_24bits << 8) | code) & 0x00FFFFFF;
        } while((next_24bits != 0x000001) && !feof(pf));

        fread(&nalu_header, 1, 1, pf);
        nalu_type = (u32)(nalu_header & 0x1F);
        /*
        if (1 == nalu_type) {
            fprintf(stderr, "%-4d nalu type: non-IDR ", packet_num);
        } else if (5 == nalu_type) {
            fprintf(stderr, "%-4d nalu type: IDR ", packet_num);
        } else if (6 == nalu_type) {
            fprintf(stderr, "nalu type: SEI ");
        } else if (7 == nalu_type) {
            fprintf(stderr, "nalu type: SPS ");
        } else if (8 == nalu_type) {
            fprintf(stderr, "nalu type: PPS ");
        } else {
            fprintf(stderr, "nalu type: %d ", nalu_type);
        }
        fprintf(stderr, "start code numbers: %d ", start_code_nums);
        */

        // find next start codes 0x000001 to calc size of nalu
        next_24bits = 0xFFFFFFFF;
        do {
            fread(&code, 1, 1, pf);
            next_24bits = ((next_24bits << 8) | code) & 0x00FFFFFF;
        } while((next_24bits != 0x000001) && !feof(pf));

        if (!feof(pf)) {
            fseeko(pf, -4, SEEK_CUR);

            // read one byte results in moving the file pointer forward one byte
            fread(&code, 1, 1, pf);

            // start codes are 0x00 00 00 01
            if (0 == code) {
                // go back one more byte, point to **first byte of next nalu**
                fseeko(pf, -1, SEEK_CUR); 
            }

            // fetch sps and pps data
            if (7 == nalu_type) {
                off_t sps_pos = ftello(pf);
                pctx->sps_len = sps_pos - prev_nalu_pos - (i32)start_code_nums; // skip start codes
                pctx->sps_buf = (u8 *)calloc(pctx->sps_len, sizeof(u8));

                fseeko(pf, -1 * pctx->sps_len, SEEK_CUR);
                fread(pctx->sps_buf, 1, pctx->sps_len, pf);

                // parse sps (input buffer not contain nalu type byte)
                int ret = ff_avc_decode_sps(pctx, (const u8 * const)(pctx->sps_buf + 1), pctx->sps_len - 1);
            }

            if (8 == nalu_type) {
                off_t pps_pos = ftello(pf);
                pctx->pps_len = pps_pos - prev_nalu_pos - (i32)start_code_nums; // skip start codes
                pctx->pps_buf = (u8 *)calloc(pctx->pps_len, sizeof(u8));

                fseeko(pf, -1 * pctx->pps_len, SEEK_CUR);
                fread(pctx->pps_buf, pctx->pps_len, 1, pf);
            }

            prev_nalu_pos = ftello(pf);

            // only calculate IDR and non-IDR packet size
            if (1 == nalu_type || 5 == nalu_type) {
                off_t new_pos = ftello(pf);

                u32 new_cnts = pctx->pkt_counts + 1;
                pctx->pkts = realloc(pctx->pkts, new_cnts * sizeof(H264PacketInfo));
                pctx->pkts[pctx->pkt_counts].type = nalu_type;
                pctx->pkts[pctx->pkt_counts].len  = new_pos - last_pos;
                pctx->pkts[pctx->pkt_counts].file_offset = last_pos;
                pctx->pkt_counts = new_cnts;

                last_pos = new_pos;
                packet_nums++;
            }
        } else {
            // last packet
            off_t new_pos = ftello(pf);

            u32 new_cnts = pctx->pkt_counts + 1;
            pctx->pkts = realloc(pctx->pkts, new_cnts * sizeof(H264PacketInfo));
            pctx->pkts[pctx->pkt_counts].type = nalu_type;
            pctx->pkts[pctx->pkt_counts].len  = new_pos - last_pos;
            pctx->pkts[pctx->pkt_counts].file_offset = last_pos;
            pctx->pkt_counts = new_cnts;
            packet_nums++;
        }
    }
    pctx->packet_nums = packet_nums;

    if (pf) {
        fclose(pf);
    }
    return 0;
}

/**
 * @brief free h264 parser context
 * 
 * @param ppctx h264 parser context
 */
int h264_free(H264ParserContext **ppctx) {
    if (*ppctx) {
        if ((*ppctx)->pkts) {
            free((*ppctx)->pkts);
        }
        if ((*ppctx)->sps_buf) {
            free((*ppctx)->sps_buf);
        }
        if ((*ppctx)->pps_buf) {
            free((*ppctx)->pps_buf);
        }
        free(*ppctx);
    }
    return 0;
}

u32 annexb2mp4(const u8 *src, u32 src_len, u8 *dst)
{
    u32 next_24bit;
    u32 dst_len = 0;
    const u8 *bs_cur, *bs_end;
    const u8 *cur_nalu_start;
    u8 code;

    bs_cur = src;
    bs_end = src + src_len;
    while (bs_cur < bs_end) {
        cur_nalu_start = bs_cur;

        i32 start_code_bytes = 0;
        // find start code
        next_24bit = 0xFFFFFFFF;
        do {
            code = *bs_cur;
            next_24bit = ((next_24bit << 8) | code) & 0xFFFFFF;
            bs_cur++;
            start_code_bytes++;
        } while((next_24bit != 0x000001) && (bs_cur < bs_end));

        // find next start code
        next_24bit = 0xFFFFFFFF;
        do {
            code = *bs_cur;
            next_24bit = ((next_24bit << 8) | code) & 0xFFFFFF;
            bs_cur++;
        } while((next_24bit != 0x000001) && (bs_cur < bs_end));


        if (bs_cur < bs_end) {
            bs_cur -= 3;
            if(*(bs_cur - 1) == 0) // bs_cur points to first byte of next nalu
            {
                bs_cur -= 1;
            }
        } else {
            bs_cur = bs_end;
        }

        u32 nalu_len = bs_cur - cur_nalu_start - start_code_bytes;
        //fprintf(stderr, "nalu size: %d\n", nalu_len);
        u32 be_nalu_len = tobig(nalu_len);

        memcpy(dst + dst_len, &be_nalu_len, sizeof(u32));
        dst_len += sizeof(u32);

        memcpy(dst + dst_len, cur_nalu_start + start_code_bytes, nalu_len);
        dst_len += nalu_len;

    }
    return dst_len;
}
