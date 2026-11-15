#include "mp4.h"
#include "fmp4.h"

// *******global variables************
FILE *g_out_file;
u8 g_ex_type[16];
StreamParams g_params;
H264ParserContext *g_h264_params;
AACParserContext  *g_aac_params;

u32 g_fmp4_seq_num = 1;

int get_stream_params(const char *cfg_path) {
    FILE *pf = fopen(cfg_path, "rb");
    if (!pf) {
        fprintf(stderr, "[get_stream_params] open %s failed.\n", cfg_path);
        return -1;
    }

    char line[1000], backup_line[1000];
    while (fgets(line, sizeof(line), pf)) {
        char *key = NULL, *value = NULL;

        if (strcspn(line, "#\n\r") == 0)
            continue;
        strncpy(backup_line, line, sizeof(backup_line));

        remove_chars(line, "\r\n");
        char *dst[50] = {0}, **dst_ptr = dst;
        int count = 0;
        split_string(line, "=", &dst_ptr, &count);
        if (2 == count) {
            key   = dst[0];
            value = dst[1];
        } else {
            fprintf(stderr, "[get_stream_params] Invalid syntax: %s\n", backup_line);
            continue;
        }

        //fprintf(stderr, "%s->%s\n", key, value);
        if (!strcmp(key, "videopath")) {
            g_params.video_path = (u8 *)malloc(strlen(value) + 1);
            memcpy(g_params.video_path, value, strlen(value) + 1);
        }
        if (!strcmp(key, "width")) {
            g_params.width = atoi(value);
        }
        if (!strcmp(key, "height")) {
            g_params.height = atoi(value);
        }
        if (!strcmp(key, "framerate")) {
            char *dst_rate[50] = {0}, **dst_rate_ptr = dst_rate;
            int count_rate = 0;
            split_string(value, "/", &dst_rate_ptr, &count_rate);

            if (2 == count_rate) {
                g_params.framerate_den = atoi(dst_rate[0]);
                g_params.framerate_num = atoi(dst_rate[1]);
            } else {
                fprintf(stderr, "[get_stream_params] framerate format error: %s\n", backup_line);
            }
        }
        if (!strcmp(key, "videoduration")) {
            g_params.video_duration = atoll(value);
        }

        if (!strcmp(key, "audiopath")) {
            g_params.audio_path = (u8 *)malloc(strlen(value) + 1);
            memcpy(g_params.audio_path, value, strlen(value) + 1);
        }
        if (!strcmp(key, "channelcount")) {
            g_params.audio_channel_count = atoi(value);
        }
        if (!strcmp(key, "samplerate")) {
            g_params.audio_sample_rate = atoi(value);
        }
        if (!strcmp(key, "audioduration")) {
            g_params.audio_duration = atoll(value);
        }

        if (!strcmp(key, "outputpath")) {
            g_params.output_path = (u8 *)malloc(strlen(value) + 1);
            memcpy(g_params.output_path, value, strlen(value) + 1);
        }
        if (!strcmp(key, "fragment")) {
            g_params.is_fragment = atoi(value);
        }
    }

    // mvhd
    g_params.time_scale = 1000;

    // video params
    g_params.video_time_scale = 12800;
    g_params.video_track_id   = 1;

    // audio params
    g_params.audio_time_scale = g_params.audio_sample_rate;
    g_params.audio_track_id   = 2;
    g_params.audio_sample_size= 16;

    // malloc
    g_params.in_buf_size = 32 * 1024 * 1024;
    g_params.in_buf = (u8 *)malloc(g_params.in_buf_size * sizeof(u8));
    if (!g_params.in_buf) {
        fprintf(stderr, "[write_mdat] malloc in buf failed.\n");
    }

    g_params.out_buf_size = g_params.in_buf_size << 1;
    g_params.out_buf = (u8 *)malloc(g_params.out_buf_size * sizeof(u8));
    if (!g_params.out_buf) {
        fprintf(stderr, "[write_mdat] malloc out buf failed.\n");
    }

    if (pf) {
        fclose(pf);
    }
    return 0;
}

void free_global_params() {
    if (g_params.video_sample_infos) {
        free(g_params.video_sample_infos);
    }
    if (g_params.audio_sample_infos) {
        free(g_params.audio_sample_infos);
    }
    if (g_params.chunk_of_sample_index) {
        free(g_params.chunk_of_sample_index);
    }
    if (g_params.in_buf) {
        free(g_params.in_buf);
    }
    if (g_params.out_buf) {
        free(g_params.out_buf);
    }
    if (g_params.video_path) {
        free(g_params.video_path);
    }
    if (g_params.audio_path) {
        free(g_params.audio_path);
    }
    if (g_params.output_path) {
        free(g_params.output_path);
    }
    if (g_params.v_clusters) {
        for (size_t i = 0; i < g_params.v_cluster_counts; i++)
        {
            if (g_params.v_clusters[i].sample_size) {
                free(g_params.v_clusters[i].sample_size);
            }
        }
        
        free(g_params.v_clusters);
    }
    if (g_params.a_clusters) {
        for (size_t i = 0; i < g_params.a_cluster_counts; i++)
        {
            if (g_params.a_clusters[i].sample_size) {
                free(g_params.a_clusters[i].sample_size);
            }
        }

        free(g_params.a_clusters);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "[main] usage: ./Mp4Muxer input.cfg\n");
        return -1;
    }

    get_stream_params(argv[1]);

    const char *video_path = (const char *)g_params.video_path;
    const char *audio_path = (const char *)g_params.audio_path;

    if (!file_exist(video_path) && !file_exist(audio_path)) {
        fprintf(stderr, "[main] you must specify a video file and/or a audio file.\n");
        return -1;
    } else if (!file_exist(video_path)) {
        fprintf(stderr, "[main] video file %s not exist.\n", video_path);
    } else if (!file_exist(audio_path)) {
        fprintf(stderr, "[main] audio file %s not exist.\n", audio_path);
    }

    if (!(g_out_file = fopen((const char *)g_params.output_path, "wb"))) {
        fprintf(stderr, "[main] open output file error: %s\n", g_params.output_path);
        return -1;
    }

    h264_init(&g_h264_params);
    h264_parse(video_path, g_h264_params);

    aac_init(&g_aac_params);
    aac_parse(audio_path, g_aac_params);

    write_ftyp();
    if (0 == g_params.is_fragment)
        write_mdat(video_path, audio_path);
    write_moov();

    if (1 == g_params.is_fragment)
        write_moofs();

    h264_free(&g_h264_params);
    aac_free(&g_aac_params);

    if (g_out_file) {
        fclose(g_out_file);
        g_out_file = NULL;
    }

    // free g_params
    free_global_params();

    fprintf(stderr, "ok\n");
    return 0;
}