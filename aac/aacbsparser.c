#include "aacbsparser.h"

int parse_adts_header(const u8 *src, ADTSHeader *header)
{
    GetBitContext gb;
    int ret = init_get_bits8(&gb, src, ADTS_HEADER_SIZE);
    if (0 == ret) {
        GetBitContext *p_gb = &gb;

        // fixed header fields
        header->f_header.syncword = get_bits(p_gb, 12);
        if (header->f_header.syncword != 0xfff) {
            fprintf(stderr, "[parse_adts_header] sync word error: %hu\n", header->f_header.syncword);
            return -1;
        }
        header->f_header.id                  = get_bits1(p_gb);
        header->f_header.layer               = get_bits(p_gb, 2);
        header->f_header.protection_absent   = get_bits1(p_gb);
        header->f_header.profile             = get_bits(p_gb, 2);
        header->f_header.sampling_freq_index = get_bits(p_gb, 4);
        header->f_header.private_bit         = get_bits1(p_gb);
        header->f_header.chan_configuration  = get_bits(p_gb, 3);
        header->f_header.original_copy       = get_bits1(p_gb);
        header->f_header.home                = get_bits1(p_gb);

        // variable header fields
        header->v_header.copyright_id_bit   = get_bits1(p_gb);
        header->v_header.copyright_id_start = get_bits1(p_gb);
        header->v_header.aac_frame_length   = get_bits(p_gb, 13);
        header->v_header.adts_buf_fulless   = get_bits(p_gb, 11);
        header->v_header.number_aac_frames  = get_bits(p_gb, 2);

        if (header->v_header.aac_frame_length < ADTS_HEADER_SIZE) {
            fprintf(stderr, "[parse_adts_header] aac_frame_length less than 7 : %hu\n", header->v_header.aac_frame_length);
            return -1;
        }
        if (0 != header->v_header.number_aac_frames) {
            fprintf(stderr, "[parse_adts_header] warning: number_aac_frames: %d\n", header->v_header.number_aac_frames + 1);
        }

        if (header->f_header.protection_absent == 0) {
            header->CRC = get_bits(p_gb, 16);
        }
    } else {
        fprintf(stderr, "[parse_adts_header] init_get_bits8 failed.");
        return -1;
    }
    return 0;
}
