#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef unsigned char  u8;
typedef unsigned short u16;
typedef int            i32;
typedef unsigned int   u32;
typedef long long      i64;
typedef unsigned long long u64;

#define VIDEO_TRACK 0
#define AUDIO_TRACK 1

#define AAC_FRAME_SIZE 1024

typedef struct SampleInfo {
    i32 sample_index;
    i64 size;
    off_t offset;
} SampleInfo;

typedef struct AVCluster {
    u32 seq_num;

    int is_video;

    u32 start_sample_idx;
    u32 sample_count;

    u32 *sample_size;

    double fragment_duration; // ms

    off_t moof_offset;
    u64 start_time;
} AVCluster;

typedef struct StreamParams {
    u8 *video_path;
    u8 *audio_path;
    u8 *output_path;

    // mvhd
    u32 time_scale;

    // video
    u32 width;
    u32 height;
    u32 framerate_num;
    u32 framerate_den;
    u32 video_time_scale;
    i64 video_duration;
    u32 video_track_id;

    // audio
    u32 audio_time_scale;
    i64 audio_duration;
    u32 audio_track_id;

    u16 audio_channel_count;
    u16 audio_sample_size;
    u32 audio_sample_rate;

    SampleInfo *video_sample_infos;
    u32 video_sample_counts;

    SampleInfo *audio_sample_infos;
    u32 audio_sample_counts;

    u32 *chunk_of_sample_index;
    u32 chunk_entry_count;

    u8 *in_buf;
    u32 in_buf_size;
    u8 *out_buf;
    u32 out_buf_size;

    // fragment params
    int is_fragment;
    int v_cluster_counts;
    AVCluster *v_clusters;

    int a_cluster_counts;
    AVCluster *a_clusters;
} StreamParams;

typedef struct CMapItem {
    char *key;
    u64 value;
} CMapItem;

typedef struct CMap {
    int count;
    CMapItem *items;
} CMap;

void add_item(CMap **d, char *key, u64 value);
u64 get_value(CMap *d, char *key);
void map_free(CMap **d);

/* convert u16 from little endian to big endian*/
u16 tobig_16(u16 i);

/* convert 24bits from little endian to big endian*/
u32 tobig_24(u32 i);

/* convert u32 from little endian to big endian*/
u32 tobig(u32 i);

/* convert u64 from little endian to big endian*/
u64 tobig_64(u64 l);

/* convert four chars to big endian u32*/
u32 uchars2uint(char chars[]);

/* get seconds since midnight, Jan. 1, 1904 in UTC time*/
u64 get_utc_time();

/* remove char from src*/
void remove_char(char *src, char c);

/* remove chars from src*/
void remove_chars(char *src, const char *chars);

void split_string(char *src, const char *delim, char ***dst, int *count);

int file_exist(const char*file_path);

void fwrite_u16(u16 u);
void fwrite_u24(u32 u);
void fwrite_u32(u32 u);
void fwrite_u64(u64 u);

int write_box(u32 size, u32 box_type, u8 extended_type[16], off_t *size_offset, int *size_bytes);
int write_full_box(u32 size, u32 box_type, u8 extended_type[16], u8 version, u32 flags,
                   off_t *size_offset, int *size_bytes);
void update_box_size(u64 box_size, off_t size_offset, int size_bytes);

#define BOX_START \
    off_t size_offset = 0; \
    int size_bytes  = 0; \
    off_t begin_offset = ftello(g_out_file); 

#define BOX_END(type) \
    off_t end_offset = ftello(g_out_file); \
    u64 box_size = end_offset - begin_offset; \
    fprintf(stderr, "%s size: %lld\n", type, box_size); \
    update_box_size(box_size, size_offset, size_bytes); \
    return box_size;
#endif
