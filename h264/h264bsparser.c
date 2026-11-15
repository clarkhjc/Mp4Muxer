#include "h264bsparser.h"

const AVRational avc_sample_aspect_ratio[17] = {
    {   0,  1 },
    {   1,  1 },
    {  12, 11 },
    {  10, 11 },
    {  16, 11 },
    {  40, 33 },
    {  24, 11 },
    {  20, 11 },
    {  32, 11 },
    {  80, 33 },
    {  18, 11 },
    {  15, 11 },
    {  64, 33 },
    { 160, 99 },
    {   4,  3 },
    {   3,  2 },
    {   2,  1 },
};

u8 *ff_nal_unit_extract_rbsp(const u8 *src, u32 src_len,
                                  u32 *dst_len, int header_len)
{
    u8 *dst;
    u32 i, len;

    dst = (u8 *)malloc(src_len + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!dst)
        return NULL;

    /* NAL unit header */
    i = len = 0;
    while (i < header_len && i < src_len)
        dst[len++] = src[i++];

    while (i + 2 < src_len)
        if (!src[i] && !src[i + 1] && src[i + 2] == 3) {
            dst[len++] = src[i++];
            dst[len++] = src[i++];
            i++; // remove emulation_prevention_three_byte
        } else
            dst[len++] = src[i++];

    while (i < src_len)
        dst[len++] = src[i++];

    memset(dst + len, 0, AV_INPUT_BUFFER_PADDING_SIZE);

    *dst_len = len;
    return dst;
}

int ff_avc_decode_sps(H264ParserContext *sps, const u8 * const buf, int buf_size)
{
    int i, j, ret, aspect_ratio_idc, pic_order_cnt_type;
    int num_ref_frames_in_pic_order_cnt_cycle;
    int delta_scale, lastScale = 8, nextScale = 8;
    int sizeOfScalingList;
    GetBitContext gb;
    u8 *rbsp_buf;
    u32 rbsp_size;

    rbsp_buf = ff_nal_unit_extract_rbsp(buf, buf_size, &rbsp_size, 0);
    if (!rbsp_buf)
        return -1;

    ret = init_get_bits8(&gb, rbsp_buf, rbsp_size);
    if (ret < 0)
        goto end;

    sps->profile_idc = get_bits(&gb, 8);
    sps->constraint_set_flags |= get_bits1(&gb) << 0; // constraint_set0_flag
    sps->constraint_set_flags |= get_bits1(&gb) << 1; // constraint_set1_flag
    sps->constraint_set_flags |= get_bits1(&gb) << 2; // constraint_set2_flag
    sps->constraint_set_flags |= get_bits1(&gb) << 3; // constraint_set3_flag
    sps->constraint_set_flags |= get_bits1(&gb) << 4; // constraint_set4_flag
    sps->constraint_set_flags |= get_bits1(&gb) << 5; // constraint_set5_flag
    skip_bits(&gb, 2); // reserved_zero_2bits
    sps->level_idc = get_bits(&gb, 8);
    sps->id = get_ue_golomb(&gb);

    if (sps->profile_idc == 100 || sps->profile_idc == 110 ||
        sps->profile_idc == 122 || sps->profile_idc == 244 || sps->profile_idc ==  44 ||
        sps->profile_idc ==  83 || sps->profile_idc ==  86 || sps->profile_idc == 118 ||
        sps->profile_idc == 128 || sps->profile_idc == 138 || sps->profile_idc == 139 ||
        sps->profile_idc == 134) {
        sps->chroma_format_idc = get_ue_golomb(&gb); // chroma_format_idc
        if (sps->chroma_format_idc == 3) {
            skip_bits1(&gb); // separate_colour_plane_flag
        }
        sps->bit_depth_luma = get_ue_golomb(&gb) + 8;
        sps->bit_depth_chroma = get_ue_golomb(&gb) + 8;
        skip_bits1(&gb); // qpprime_y_zero_transform_bypass_flag
        if (get_bits1(&gb)) { // seq_scaling_matrix_present_flag
            for (i = 0; i < ((sps->chroma_format_idc != 3) ? 8 : 12); i++) {
                if (!get_bits1(&gb)) // seq_scaling_list_present_flag
                    continue;
                lastScale = 8;
                nextScale = 8;
                sizeOfScalingList = i < 6 ? 16 : 64;
                for (j = 0; j < sizeOfScalingList; j++) {
                    if (nextScale != 0) {
                        delta_scale = get_se_golomb(&gb);
                        nextScale = (lastScale + delta_scale) & 0xff;
                    }
                    lastScale = nextScale == 0 ? lastScale : nextScale;
                }
            }
        }
    } else {
        sps->chroma_format_idc = 1;
        sps->bit_depth_luma = 8;
        sps->bit_depth_chroma = 8;
    }

    get_ue_golomb(&gb); // log2_max_frame_num_minus4
    pic_order_cnt_type = get_ue_golomb(&gb);

    if (pic_order_cnt_type == 0) {
        get_ue_golomb(&gb); // log2_max_pic_order_cnt_lsb_minus4
    } else if (pic_order_cnt_type == 1) {
        skip_bits1(&gb);    // delta_pic_order_always_zero
        get_se_golomb(&gb); // offset_for_non_ref_pic
        get_se_golomb(&gb); // offset_for_top_to_bottom_field
        num_ref_frames_in_pic_order_cnt_cycle = get_ue_golomb(&gb);
        for (i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++)
            get_se_golomb(&gb); // offset_for_ref_frame
    }

    get_ue_golomb(&gb); // max_num_ref_frames
    skip_bits1(&gb); // gaps_in_frame_num_value_allowed_flag
    sps->width = (get_ue_golomb(&gb) + 1) << 4; // pic_width_in_mbs_minus1
    sps->height= (get_ue_golomb(&gb) + 1) << 4; // pic_height_in_map_units_minus1

    sps->frame_mbs_only_flag = get_bits1(&gb);
    if (!sps->frame_mbs_only_flag)
        skip_bits1(&gb); // mb_adaptive_frame_field_flag

    skip_bits1(&gb); // direct_8x8_inference_flag

    if (get_bits1(&gb)) { // frame_cropping_flag
        get_ue_golomb(&gb); // frame_crop_left_offset
        get_ue_golomb(&gb); // frame_crop_right_offset
        get_ue_golomb(&gb); // frame_crop_top_offset
        get_ue_golomb(&gb); // frame_crop_bottom_offset
    }

    if (get_bits1(&gb)) { // vui_parameters_present_flag
        if (get_bits1(&gb)) { // aspect_ratio_info_present_flag
            aspect_ratio_idc = get_bits(&gb, 8);
            if (aspect_ratio_idc == 0xff) {
                sps->sar.num = get_bits(&gb, 16);
                sps->sar.den = get_bits(&gb, 16);
            } else if (aspect_ratio_idc < FF_ARRAY_ELEMS(avc_sample_aspect_ratio)) {
                sps->sar = avc_sample_aspect_ratio[aspect_ratio_idc];
            }
        }
    }

    if (!sps->sar.den) {
        sps->sar.num = 1;
        sps->sar.den = 1;
    }

    ret = 0;
 end:
    free(rbsp_buf);
    return ret;
}