#include "aacparser.h"

int aac_init(AACParserContext **ppctx)
{
    u32 size = sizeof(AACParserContext);

    AACParserContext *p = (AACParserContext *)calloc(size, 1);
    *ppctx = p;

    return 0;
}

int aac_parse(const char *file_path, AACParserContext *pctx)
{
    FILE *pf = fopen(file_path, "rb");
    if (!pf) {
        fprintf(stderr, "[aac_parse] open %s fail.\n", file_path);
        return -1;
    }

    u8 header_data[ADTS_HEADER_SIZE] = {0};
    while (!feof(pf)) {
        int ret = fread(header_data, 1, ADTS_HEADER_SIZE, pf);
        if (ADTS_HEADER_SIZE != ret) {
            if (0 != ret)
                fprintf(stderr, "[aac_parse] read from file error: %d\n", ret);
            goto fail;
        }
        ADTSHeader adts_header;
        ret = parse_adts_header(header_data, &adts_header);
        if (0 == ret) {
            off_t packet_pos = ftello(pf);

            u32 new_cnts = pctx->pkt_counts + 1;
            pctx->pkts = (AACPacketInfo *)realloc(pctx->pkts, new_cnts * sizeof(AACPacketInfo));
            pctx->pkts[pctx->pkt_counts].header      = adts_header;
            pctx->pkts[pctx->pkt_counts].file_offset = packet_pos;
            pctx->pkts[pctx->pkt_counts].frame_size  = adts_header.v_header.aac_frame_length - 
                         ADTS_HEADER_SIZE - (adts_header.f_header.protection_absent == 1 ? 0 : 2);
            pctx->pkt_counts = new_cnts;

            //fprintf(stderr, "[aac_parse] packet size: %hu\n", adts_header.v_header.aac_frame_length);

            off_t offset = adts_header.v_header.aac_frame_length - ADTS_HEADER_SIZE -
                           (adts_header.f_header.protection_absent == 1 ? 0 : 2);
            fseeko(pf, offset, SEEK_CUR);
        } else {
            goto fail;
        }
    }

fail:
    if (pf) {
        fclose(pf);
    }
    return 0;
}

int aac_free(AACParserContext **ppctx)
{
    if (*ppctx) {
        if ((*ppctx)->pkts) {
            free((*ppctx)->pkts);
        }
        free(*ppctx);
    }
    return 0;
}

u32 compute_avg_bitrate(AACParserContext *pctx, u32 time_scale, i64 duration)
{
    u64 size = 0;

    if (0 == duration)
        return 0;

    for (size_t i = 0; i < pctx->pkt_counts; i++)
    {
        size += pctx->pkts[i].frame_size;
    }
    
    return size * 8 * time_scale / duration;
}

u32 create_audio_specific_config(ADTSFixedHeader f_header, u8 *dst)
{
    u8 object_type    = f_header.profile + 1;
    u8 chan_config    = f_header.chan_configuration;
    u8 sampling_index = f_header.sampling_freq_index;
    
    u16 res = (object_type & 0x1F) << 11 |
              (sampling_index & 0x0F) << 7  |
              (chan_config & 0x0F) << 3;
    
    dst[0] = (res >> 8) & 0xFF;
    dst[1] = res & 0xFF;  
    return 2;
}
