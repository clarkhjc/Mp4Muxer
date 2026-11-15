#include "fmp4.h"

extern FILE *g_out_file;
extern u8 g_ex_type[16];
extern StreamParams g_params;
extern H264ParserContext *g_h264_params;
extern AACParserContext  *g_aac_params;
extern u32 g_fmp4_seq_num;

off_t g_trun_offset;
u32   g_moof_size;

u32 g_audio_segment_duration = 3000; //3 seconds

void get_audio_clusters(size_t start_idx) {
    for (size_t i = start_idx; i < g_aac_params->pkt_counts; )
    {
        g_params.a_cluster_counts++;
        g_params.a_clusters = (AVCluster *)realloc(g_params.a_clusters, sizeof(AVCluster) * g_params.a_cluster_counts);
        g_params.a_clusters[g_params.a_cluster_counts - 1].is_video = AUDIO_TRACK;
        g_params.a_clusters[g_params.a_cluster_counts - 1].fragment_duration = 0;
        g_params.a_clusters[g_params.a_cluster_counts - 1].sample_count = 0;

        g_params.a_clusters[g_params.a_cluster_counts - 1].start_sample_idx = start_idx;

        double audio_duration = 0;
        for (size_t j = start_idx; j < g_aac_params->pkt_counts; j++)
        {
            if (audio_duration >= g_audio_segment_duration) {
                g_params.a_clusters[g_params.a_cluster_counts - 1].sample_count = 
                    j - g_params.a_clusters[g_params.a_cluster_counts - 1].start_sample_idx;
                
                g_params.a_clusters[g_params.a_cluster_counts - 1].fragment_duration = audio_duration;
                i += g_params.a_clusters[g_params.a_cluster_counts - 1].sample_count;

                start_idx += g_params.a_clusters[g_params.a_cluster_counts - 1].sample_count;
                break;
            }
            audio_duration += 1000.0 * AAC_FRAME_SIZE / g_params.audio_sample_rate; // 1024: aac frame size
        }
        // audio shorter than video
        if (0 == g_params.a_clusters[g_params.a_cluster_counts - 1].sample_count) {
            g_params.a_clusters[g_params.a_cluster_counts - 1].sample_count = 
                    g_aac_params->pkt_counts - g_params.a_clusters[g_params.a_cluster_counts - 1].start_sample_idx;
            i += g_params.a_clusters[g_params.a_cluster_counts - 1].sample_count;
        }
    }

    // audio shorter than video
    if (0 == g_params.a_clusters[g_params.a_cluster_counts - 1].sample_count) {
        g_params.a_clusters[g_params.a_cluster_counts - 1].sample_count = 
                g_aac_params->pkt_counts - g_params.a_clusters[g_params.a_cluster_counts - 1].start_sample_idx;
    }
}

/**
 * @brief Get the data clusters
 * 
 */
void get_av_clusters(int has_video, int has_audio) {
    if (has_video) {
        for (size_t i = 0; i < g_h264_params->pkt_counts; i++)
        {
            if (g_h264_params->pkts[i].type == 5) {
                // update sample_count of preivous cluster
                if (g_params.v_cluster_counts) {
                    u32 start_idx = g_params.v_clusters[g_params.v_cluster_counts - 1].start_sample_idx;
                    g_params.v_clusters[g_params.v_cluster_counts - 1].sample_count = i - start_idx;
                }

                g_params.v_cluster_counts++;
                g_params.v_clusters = (AVCluster *)realloc(g_params.v_clusters, sizeof(AVCluster) * g_params.v_cluster_counts);
                g_params.v_clusters[g_params.v_cluster_counts - 1].is_video = VIDEO_TRACK;
                g_params.v_clusters[g_params.v_cluster_counts - 1].start_sample_idx = i;
                g_params.v_clusters[g_params.v_cluster_counts - 1].fragment_duration = 0;
            }
            g_params.v_clusters[g_params.v_cluster_counts - 1].fragment_duration += 1000.0 * g_params.framerate_num / g_params.framerate_den;
        }
        // update sample_count of last cluster
        if (g_params.v_cluster_counts) {
            u32 start_idx = g_params.v_clusters[g_params.v_cluster_counts - 1].start_sample_idx;
            g_params.v_clusters[g_params.v_cluster_counts - 1].sample_count = g_h264_params->pkt_counts - start_idx;
        }
    }

    if (has_audio) {
        if (has_video) {
            for (size_t i = 0; i < g_params.v_cluster_counts; i++)
            {
                double video_duration = g_params.v_clusters[i].fragment_duration;

                g_params.a_cluster_counts++;
                g_params.a_clusters = (AVCluster *)realloc(g_params.a_clusters, sizeof(AVCluster) * g_params.a_cluster_counts);
                g_params.a_clusters[g_params.a_cluster_counts - 1].is_video = AUDIO_TRACK;
                g_params.a_clusters[g_params.a_cluster_counts - 1].fragment_duration = 0;
                g_params.a_clusters[g_params.a_cluster_counts - 1].sample_count = 0;

                size_t j = i > 0 ? g_params.a_clusters[g_params.a_cluster_counts - 2].start_sample_idx + g_params.a_clusters[g_params.a_cluster_counts - 2].sample_count : 0;
                g_params.a_clusters[g_params.a_cluster_counts - 1].start_sample_idx = j;

                double audio_duration = 0;
                for (; j < g_aac_params->pkt_counts; j++)
                {
                    if (audio_duration >= video_duration) {
                        g_params.a_clusters[g_params.a_cluster_counts - 1].sample_count = 
                            j - g_params.a_clusters[g_params.a_cluster_counts - 1].start_sample_idx;
                        
                        g_params.a_clusters[g_params.a_cluster_counts - 1].fragment_duration = audio_duration;
                        break;
                    }

                    audio_duration += 1000.0 * AAC_FRAME_SIZE / g_params.audio_sample_rate; // 1024: aac frame size
                }
                // audio shorter than video
                if (0 == g_params.a_clusters[g_params.a_cluster_counts - 1].sample_count) {
                    g_params.a_clusters[g_params.a_cluster_counts - 1].sample_count = 
                            g_aac_params->pkt_counts - g_params.a_clusters[g_params.a_cluster_counts - 1].start_sample_idx;
                }
            }

            // Determine whether the audio processing is complete
            u32 last_audio_start_idx    = g_params.a_clusters[g_params.a_cluster_counts - 1].start_sample_idx;
            u32 last_audio_sample_count = g_params.a_clusters[g_params.a_cluster_counts - 1].sample_count;
            if ((last_audio_start_idx + last_audio_sample_count) < g_aac_params->pkt_counts) {
                get_audio_clusters(last_audio_start_idx + last_audio_sample_count);
            }
        } else {
            get_audio_clusters(0);
        }
    }
}

/**
 * @brief write movie fragment header box
 * refer to 8.8.5 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class MovieFragmentHeaderBox extends FullBox(‘mfhd’, 0, 0) {
        unsigned int(32)  sequence_number;
    }
 * @return u64 size of box
 */
u64 write_mfhd() {
    BOX_START

    write_full_box(-1, uchars2uint("mfhd"), g_ex_type, 0, 0, &size_offset, &size_bytes);
    fwrite_u32(g_fmp4_seq_num++);

    BOX_END("mfhd")
}

/**
 * @brief 
    bit(4) reserved=0;
    unsigned int(2) is_leading;
    unsigned int(2) sample_depends_on;
    unsigned int(2) sample_is_depended_on; 
    unsigned int(2) sample_has_redundancy;
    bit(3) sample_padding_value;
    bit(1) sample_is_non_sync_sample;
    unsigned int(16) sample_degradation_priority;
 * @return u32 
 */
u32 make_sample_flags(
    u8 is_leading, 
    u8 sample_depends_on, 
    u8 sample_is_depended_on,
    u8 sample_has_redundancy,
    u8 sample_padding_value,
    u8 sample_is_non_sync_sample,
    u16 sample_degradation_priority) 
{
    return ((u32)is_leading & 0x3) << 26 |
           ((u32)sample_depends_on & 0x3) << 24 | 
           ((u32)sample_is_depended_on & 0x3) << 22 | 
           ((u32)sample_has_redundancy & 0x3) << 20 |
           ((u32)sample_padding_value & 0x7) << 17 | 
           ((u32)sample_is_non_sync_sample & 0x1) << 16 |
           sample_degradation_priority;
}

/**
 * @brief write track fragment header box
 * refer to 8.8.7 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class TrackFragmentHeaderBox extends FullBox(‘tfhd’, 0, tf_flags) {
        unsigned int(32) track_ID;
        // all the following are optional fields 
        unsigned int(64) base_data_offset; 
        unsigned int(32) sample_description_index; 
        unsigned int(32) default_sample_duration; 
        unsigned int(32) default_sample_size; 
        unsigned int(32) default_sample_flags
    }
 * 
 * @param cluster 
 * @return u64 
 */
u64 write_tfhd(AVCluster cluster) {
    BOX_START

    // 0x000002: sample-description-index-present
    // 0x000008: default-sample-duration-present
    // 0x000020: default-sample-flags-present
    // 0x020000: default-base-is-moof
    u32 tf_flags = 0x2002a;
    write_full_box(-1, uchars2uint("tfhd"), g_ex_type, 0, tf_flags, &size_offset, &size_bytes);

    // track_ID
    if (cluster.is_video == VIDEO_TRACK) {
        fwrite_u32(g_params.video_track_id);
    } else {
        fwrite_u32(g_params.audio_track_id);
    }

    // sample_description_index
    fwrite_u32(1);

    // default-sample-duration
    if (cluster.is_video == VIDEO_TRACK) {
        u32 video_sample_duration = g_params.video_time_scale * g_params.framerate_num / g_params.framerate_den;
        fwrite_u32(video_sample_duration);
    } else {
        fwrite_u32(AAC_FRAME_SIZE);
    }

    // default-sample-flags
    if (cluster.is_video == VIDEO_TRACK) {
        u32 video_sample_flags = make_sample_flags(0, 1, 0, 0, 0, 1, 0);
        fwrite_u32(video_sample_flags);
    } else {
        fwrite_u32(0);
    }

    BOX_END("tfhd")
}

/**
 * @brief write Track fragment decode time box
 * refer to 8.8.12 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class TrackFragmentBaseMediaDecodeTimeBox extends FullBox(‘tfdt’, version, 0) {
        if (version==1) {
            unsigned int(64) baseMediaDecodeTime;
        } else { // version==0
            unsigned int(32) baseMediaDecodeTime;
        }
    }
 * 
 * @param pcluster 
 * @return u64 
 */
u64 write_tfdt(AVCluster *pcluster) {
    BOX_START

    u8 version = 1;
    write_full_box(-1, uchars2uint("tfdt"), g_ex_type, version, 0, &size_offset, &size_bytes);

    u64 base_media_decode_time = 0;
    if (VIDEO_TRACK == pcluster->is_video) {
        base_media_decode_time = pcluster->start_sample_idx * 
                                 (g_params.video_time_scale * g_params.framerate_num / g_params.framerate_den);
    } else {
        base_media_decode_time = pcluster->start_sample_idx * AAC_FRAME_SIZE;
    }

    pcluster->start_time = base_media_decode_time;
    if (1 == version) {
        fwrite_u64(base_media_decode_time);
    } else {
        fwrite_u32((u32)base_media_decode_time);
    }

    BOX_END("tfdt")
}

/**
 * @brief write Track Fragment Run Box
 * refer to 8.8.8 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class TrackRunBox extends FullBox(‘trun’, version, tr_flags) {
        unsigned int(32)  sample_count;
        // the following are optional fields
        signed int(32) data_offset;
        unsigned int(32)  first_sample_flags;
        // all fields in the following array are optional
        {
            unsigned int(32)  sample_duration;
            unsigned int(32)  sample_size;
            unsigned int(32)  sample_flags
            if (version == 0)
                { unsigned int(32) sample_composition_time_offset; }
            else
                { signed int(32)   sample_composition_time_offset; }
        } [ sample_count ]
    }
 * 
 * @param cluster 
 * @return u64 
 */
u64 write_trun_fake(AVCluster cluster) {
    BOX_START

    g_trun_offset = ftello(g_out_file);
    u8 version = 1;

    // 0x000200: sample-size-present
    // 0x000001: data-offset-present
    // 0x000004: first-sample-flags-present
    u32 tr_flags;
    if (VIDEO_TRACK == cluster.is_video) {
        tr_flags = 0x000205;
    } else {
        tr_flags = 0x000201;
    }
    write_full_box(-1, uchars2uint("trun"), g_ex_type, version, tr_flags, &size_offset, &size_bytes);

    // sample_count
    fwrite_u32(cluster.sample_count);

    // data_offset
    fwrite_u32(0);

    // first_sample_flags
    if (VIDEO_TRACK == cluster.is_video) {
        fwrite_u32(0);
    } 

    for (size_t i = 0; i < cluster.sample_count; i++)
    {
        // sample_size
        fwrite_u32(0);
    }
    
    BOX_END("trun_fake")
}

/**
 * @brief write track fragment box
 * refer to 8.8.6 in ISO/IEC 14496-12
 * 
 * @param pcluster 
 * @return u64 size of box
 */
u64 write_traf(AVCluster *pcluster) {
    BOX_START

    write_box(-1, uchars2uint("traf"), g_ex_type, &size_offset, &size_bytes);

    write_tfhd(*pcluster);

    write_tfdt(pcluster);

    write_trun_fake(*pcluster);

    BOX_END("traf")
}

/**
 * @brief write movie fragment box
 * refer to 8.8.4 in ISO/IEC 14496-12
 * 
 * @param pcluster 
 * @return u64 size of box
 */
u64 write_moof(AVCluster *pcluster) {
    BOX_START
    pcluster->moof_offset = ftello(g_out_file);

    write_box(-1, uchars2uint("moof"), g_ex_type, &size_offset, &size_bytes);

    write_mfhd();

    write_traf(pcluster);

    BOX_END("moof")
}

/**
 * @brief write media data box
 * 
 * @param pcluster 
 * @param pf_video
 * @param pf_audio
 * @return u64 size of box
 */
u64 write_moof_mdat(AVCluster *pcluster, FILE *pf_video, FILE *pf_audio) {
    BOX_START

    // size
    write_box(-1, uchars2uint("mdat"), g_ex_type, &size_offset, &size_bytes);

    pcluster->sample_size = (u32 *)malloc(pcluster->sample_count * sizeof(u32));

    for (size_t i = pcluster->start_sample_idx; i < pcluster->start_sample_idx + pcluster->sample_count; i++)
    {
        if (VIDEO_TRACK == pcluster->is_video) {
            H264PacketInfo packet_info = g_h264_params->pkts[i];
            fseeko(pf_video, packet_info.file_offset, SEEK_SET);
            fread(g_params.in_buf, sizeof(u8), packet_info.len, pf_video);
            u32 ret = annexb2mp4(g_params.in_buf, packet_info.len, g_params.out_buf);
            if (ret) {
                fwrite(g_params.out_buf, sizeof(u8), ret, g_out_file);
                pcluster->sample_size[i - pcluster->start_sample_idx] = ret;
            }
        } else {
            u32 frame_size = g_aac_params->pkts[i].frame_size;

            fseeko(pf_audio, g_aac_params->pkts[i].file_offset, SEEK_SET);
            fread(g_params.in_buf, sizeof(u8), frame_size, pf_audio);
            fwrite(g_params.in_buf, sizeof(u8), frame_size, g_out_file);
            pcluster->sample_size[i - pcluster->start_sample_idx] = frame_size;
        }
    }

    BOX_END("mdat")
}

/**
 * @brief update track fragment run box
 * 
 * @param cluster 
 * @return u64 size of box
 */
u64 update_trun(AVCluster cluster) {
    BOX_START

    u8 version = 1;

    // 0x000200: sample-size-present
    // 0x000001: data-offset-present
    // 0x000004: first-sample-flags-present
    u32 tr_flags;
    if (VIDEO_TRACK == cluster.is_video) {
        tr_flags = 0x000205;
    } else {
        tr_flags = 0x000201;
    }
    write_full_box(-1, uchars2uint("trun"), g_ex_type, version, tr_flags, &size_offset, &size_bytes);

    // sample_count
    fwrite_u32(cluster.sample_count);

    // data_offset
    fwrite_u32(g_moof_size + 8); // 8: mdat header size

    // first_sample_flags
    if (VIDEO_TRACK == cluster.is_video) {
        u32 video_sample_flags = make_sample_flags(0, 2, 0, 0, 0, 0, 0);
        fwrite_u32(video_sample_flags);
    } 

    for (size_t i = 0; i < cluster.sample_count; i++)
    {
        // sample_size
        fwrite_u32(cluster.sample_size[i]);
    }

    BOX_END("update trun")
}

/**
 * @brief write Track Fragment Random Access Box 
 * refer to 8.8.10 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class TrackFragmentRandomAccessBox  extends FullBox(‘tfra’, version, 0) {
        unsigned int(32) track_ID;
        const unsigned int(26) reserved = 0;
        unsigned int(2) length_size_of_traf_num;
        unsigned int(2) length_size_of_trun_num;
        unsigned int(2) length_size_of_sample_num
        unsigned int(32) number_of_entry;
        for(i=1; i <= number_of_entry; i++) {
            if(version==1) {
                unsigned int(64) time;
                unsigned int(64) moof_offset;
            } else {
                unsigned int(32) time;
                unsigned int(32) moof_offset;
            }
            unsigned int((length_size_of_traf_num+1) * 8) traf_number;
            unsigned int((length_size_of_trun_num+1) * 8) trun_number;
            unsigned int((length_size_of_sample_num+1) * 8) sample_number;
        }
    }
 * @param is_video
 * @return u64 size of box
 */
u64 write_tfra(int is_video) {
    BOX_START

    u8 version = 1;
    write_full_box(-1, uchars2uint("tfra"), g_ex_type, version, 0, &size_offset, &size_bytes);

    // track_ID
    if (VIDEO_TRACK == is_video) {
        fwrite_u32(g_params.video_track_id);
    } else {
        fwrite_u32(g_params.audio_track_id);
    }

    // reserved + length_size_of_traf_num + length_size_of_trun_num + length_size_of_sample_num
    fwrite_u32(0);

    // number_of_entry
    u32 number_of_entry = (VIDEO_TRACK == is_video ? g_params.v_cluster_counts : g_params.a_cluster_counts);
    fwrite_u32(number_of_entry);

    for (size_t i = 0; i < number_of_entry; i++)
    {
        if (VIDEO_TRACK == is_video) {
            // time and moof_offset
            if (1 == version) {
                fwrite_u64(g_params.v_clusters[i].start_time);
                fwrite_u64(g_params.v_clusters[i].moof_offset);
            } else {
                fwrite_u64((u32)(g_params.v_clusters[i].start_time));
                fwrite_u64((u32)(g_params.v_clusters[i].moof_offset));
            }

            // traf_number
            u8 traf_number = 1;
            fwrite(&traf_number, 1, 1, g_out_file);

            // trun_number
            u8 trun_number = 1;
            fwrite(&trun_number, 1, 1, g_out_file);

            // sample_number
            u8 sample_number = 1;
            fwrite(&sample_number, 1, 1, g_out_file);
        } else {
            if (1 == version) {
                fwrite_u64(g_params.a_clusters[i].start_time);
                fwrite_u64(g_params.a_clusters[i].moof_offset);
            } else {
                fwrite_u64((u32)(g_params.a_clusters[i].start_time));
                fwrite_u64((u32)(g_params.a_clusters[i].moof_offset));
            }

            // traf_number
            u8 traf_number = 1;
            fwrite(&traf_number, 1, 1, g_out_file);

            // trun_number
            u8 trun_number = 1;
            fwrite(&trun_number, 1, 1, g_out_file);

            // sample_number
            u8 sample_number = 1;
            fwrite(&sample_number, 1, 1, g_out_file);
        }
    }
    
    BOX_END("tfra")
}

/**
 * @brief write Movie Fragment Random Access Offset Box
 * refer to 8.8.11 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class MovieFragmentRandomAccessOffsetBox extends FullBox(‘mfro’, version, 0) { 
        unsigned int(32) size; 
    }
 * @param is_video
 * @return u64 size of box
 */
u64 write_mfro() {
    BOX_START

    write_full_box(-1, uchars2uint("mfro"), g_ex_type, 0, 0, &size_offset, &size_bytes);

    // size
    fwrite_u32(0);

    BOX_END("mfro")
}

/**
 * @brief write Movie Fragment Random Access Box
 * refer to 8.8.9 in ISO/IEC 14496-12
 * @param has_video 
 * @param has_audio
 * @return u64 size of box
 */
u64 write_mfra(int has_video, int has_audio) {
    BOX_START

    write_box(-1, uchars2uint("mfra"), g_ex_type, &size_offset, &size_bytes);

    if (has_video) {
        write_tfra(VIDEO_TRACK);
    }
    if (has_audio) {
        write_tfra(AUDIO_TRACK);
    }
    write_mfro();

    BOX_END("mfra")
}

void write_moofs()
{
    int has_video = 0, has_audio = 0;
    FILE *pf_video = NULL, *pf_audio = NULL;

    if (file_exist((const char*)g_params.video_path)) {
        has_video = 1;
        pf_video = fopen((const char*)g_params.video_path, "rb");
    }   

    if (file_exist((const char*)g_params.audio_path)) {
        has_audio = 1;
        pf_audio = fopen((const char*)g_params.audio_path, "rb");
    }

    get_av_clusters(has_video, has_audio);

    int audio_count = 0;
    if (has_video && has_audio) {
        for (size_t i = 0; i < g_params.v_cluster_counts; i++)
        {
            g_moof_size = write_moof(&(g_params.v_clusters[i]));
            write_moof_mdat(&(g_params.v_clusters[i]), pf_video, pf_audio);
            
            off_t cur_pos = ftello(g_out_file);
            fseeko(g_out_file, g_trun_offset, SEEK_SET);
            update_trun(g_params.v_clusters[i]);
            fseeko(g_out_file, cur_pos, SEEK_SET);

            // audio
            if (i < g_params.a_cluster_counts) {
                g_moof_size = write_moof(&(g_params.a_clusters[i]));
                write_moof_mdat(&(g_params.a_clusters[i]), pf_video, pf_audio);
                
                off_t cur_pos = ftello(g_out_file);
                fseeko(g_out_file, g_trun_offset, SEEK_SET);
                update_trun(g_params.a_clusters[i]);
                fseeko(g_out_file, cur_pos, SEEK_SET);
                audio_count++;
            }
        }
        while (audio_count < g_params.a_cluster_counts) {
            g_moof_size = write_moof(&(g_params.a_clusters[audio_count]));
            write_moof_mdat(&(g_params.a_clusters[audio_count]), pf_video, pf_audio);

            off_t cur_pos = ftello(g_out_file);
            fseeko(g_out_file, g_trun_offset, SEEK_SET);
            update_trun(g_params.a_clusters[audio_count]);
            fseeko(g_out_file, cur_pos, SEEK_SET);
            audio_count++;
        }
    } else if (has_video) {
        for (size_t i = 0; i < g_params.v_cluster_counts; i++)
        {
            g_moof_size = write_moof(&(g_params.v_clusters[i]));
            write_moof_mdat(&(g_params.v_clusters[i]), pf_video, pf_audio);
            
            off_t cur_pos = ftello(g_out_file);
            fseeko(g_out_file, g_trun_offset, SEEK_SET);
            update_trun(g_params.v_clusters[i]);
            fseeko(g_out_file, cur_pos, SEEK_SET);
        }
    } else if (has_audio) {
        for (size_t i = 0; i < g_params.a_cluster_counts; i++)
        {
            g_moof_size = write_moof(&(g_params.a_clusters[i]));
            write_moof_mdat(&(g_params.a_clusters[i]), pf_video, pf_audio);
            
            off_t cur_pos = ftello(g_out_file);
            fseeko(g_out_file, g_trun_offset, SEEK_SET);
            update_trun(g_params.a_clusters[i]);
            fseeko(g_out_file, cur_pos, SEEK_SET);
        }
    }

    // mfra
    u64 mfra_size = write_mfra(has_video, has_audio);
    fseeko(g_out_file, -4, SEEK_END);
    fwrite_u32((u32)mfra_size);

    // close files
    if (pf_video) {
        fclose(pf_video);
    }
    if (pf_audio) {
        fclose(pf_audio);
    }
}