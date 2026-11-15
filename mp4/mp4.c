#include "mp4.h"

extern FILE *g_out_file;
extern u8 g_ex_type[16];
extern StreamParams g_params;
extern H264ParserContext *g_h264_params;
extern AACParserContext  *g_aac_params;

/**
 * @brief write file type box
 * refer to 4.3 and AnnexE in ISO/IEC 14496-12
 * @return u64 return box size of ftyp
 */
u64 write_ftyp() {
    BOX_START

    write_box(-1, uchars2uint("ftyp"), g_ex_type, &size_offset, &size_bytes);
    // major brand
    u32 major_brand = uchars2uint("isom");
    fwrite(&major_brand, sizeof(u32), 1, g_out_file);

    // minor_version
    fwrite_u32(200);

    // compatible_brands
    u32 c_brand = uchars2uint("isom");
    fwrite(&c_brand, sizeof(u32), 1, g_out_file);

    c_brand = uchars2uint("avc1");
    fwrite(&c_brand, sizeof(u32), 1, g_out_file);

    c_brand = uchars2uint("mp41");
    fwrite(&c_brand, sizeof(u32), 1, g_out_file);

    if (1 == g_params.is_fragment) {
        c_brand = uchars2uint("iso5");
        fwrite(&c_brand, sizeof(u32), 1, g_out_file);
    }

    BOX_END("ftyp")
}

/**
 * @brief write movie header box
 * refer to 8.2.2 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class MovieHeaderBox extends FullBox(‘mvhd’, version, 0) {
        if (version==1) {
            unsigned int(64) creation_time;
            unsigned int(64) modification_time;
            unsigned int(32) timescale;
            unsigned int(64) duration;
        } else { // version==0
            unsigned int(32) creation_time;
            unsigned int(32) modification_time;
            unsigned int(32) timescale;
            unsigned int(32) duration;
        }
        template int(32) rate   = 0x00010000; // typically 1.0
        template int(16) volume = 0x0100;     // typically, full volume
        const bit(16) reserved = 0;
        const unsigned int(32)[2] reserved = 0;
        template int(32)[9] matrix =
            { 0x00010000,0,0,0,0x00010000,0,0,0,0x40000000 };
            // Unity matrix
        bit(32)[6] pre_defined = 0;
        unsigned int(32) next_track_ID;
    }
 * @return u64 size of box
 */
u64 write_mvhd(u8 version) {
    BOX_START

    write_full_box(-1, uchars2uint("mvhd"), g_ex_type, version, 0, &size_offset, &size_bytes);

    i64 max_duration = g_params.video_duration > g_params.audio_duration ? g_params.video_duration : g_params.audio_duration;
    if (1 == version) {
        // creation_time
        fwrite_u64(get_utc_time());
        // modification_time
        fwrite_u64(get_utc_time());

        // timescale
        fwrite_u32(g_params.time_scale);
        fwrite_u64((u64)(max_duration * g_params.time_scale));
    } else {
        // creation_time
        fwrite_u32((u32)get_utc_time());
        // modification_time
        fwrite_u32((u32)get_utc_time());

        // timescale
        fwrite_u32(g_params.time_scale);
        fwrite_u32((u32)(max_duration * g_params.time_scale));
    }

    // rate
    fwrite_u32(0x00010000);

    // volume
    fwrite_u16(0x0100);

    // reserved
    fwrite_u16(0);

    // reserved
    fwrite_u32(0);
    fwrite_u32(0);

    // matrix
    fwrite_u32(0x00010000);
    fwrite_u32(0);
    fwrite_u32(0);
    fwrite_u32(0);
    fwrite_u32(0x00010000);
    fwrite_u32(0);
    fwrite_u32(0);
    fwrite_u32(0);
    fwrite_u32(0x40000000);

    // pre_defined
    fwrite_u32(0);
    fwrite_u32(0);
    fwrite_u32(0);
    fwrite_u32(0);
    fwrite_u32(0);
    fwrite_u32(0);

    // next_track_ID
    fwrite_u32(g_params.video_track_id + g_params.audio_track_id);

    BOX_END("mvhd")
}

/**
 * @brief write track header box
 * refer to 8.3.2 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class TrackHeaderBox extends FullBox(‘tkhd’, version, flags) {
        if (version==1) {
            unsigned int(64) creation_time;
            unsigned int(64) modification_time;
            unsigned int(32) track_ID;
            const unsigned int(32) reserved = 0;
            unsigned int(64) duration;
        } else { // version==0
            unsigned int(32) creation_time;
            unsigned int(32) modification_time;
            unsigned int(32) track_ID;
            const unsigned int(32) reserved = 0;
            unsigned int(32) duration;
        }
        const unsigned int(32)[2] reserved = 0;
        template int(16) layer = 0;
        template int(16) alternate_group = 0;
        template int(16) volume = {if track_is_audio 0x0100 else 0};
        const unsigned int(16) reserved = 0;
        template int(32)[9] matrix=
            { 0x00010000,0,0,0,0x00010000,0,0,0,0x40000000 };
            // unity matrix
        unsigned int(32) width;
        unsigned int(32) height;
    } 
 * @param is_video Identify whether the track is audio or video 
 * @return u64 
 */
u64 write_tkhd(int is_video)
{
    BOX_START

    u8 version = 0;
    u32 flags = 0x00000007; // Track_enabled + Track_in_movie + Track_in_preview
    write_full_box(-1, uchars2uint("tkhd"), g_ex_type, version, flags, &size_offset, &size_bytes);

    i64 duration = is_video == VIDEO_TRACK ? g_params.video_duration : g_params.audio_duration;
    if (1 == version) {
        // creation_time
        fwrite_u64(get_utc_time());
        // modification_time
        fwrite_u64(get_utc_time());
        // track_ID
        fwrite_u32(is_video == VIDEO_TRACK ? g_params.video_track_id : g_params.audio_track_id);
        // reserved
        fwrite_u32(0);
        // duration in units of mvhd timescale
        fwrite_u64((u64)(g_params.time_scale * duration));
    } else {
        // creation_time
        fwrite_u32((u32)get_utc_time());
        // modification_time
        fwrite_u32((u32)get_utc_time());
        // track_ID
        fwrite_u32(is_video == VIDEO_TRACK ? g_params.video_track_id : g_params.audio_track_id);
        // reserved
        fwrite_u32(0);
        // duration in units of mvhd's timescale
        fwrite_u32((u32)(g_params.time_scale * duration));
    }

    // reserved
    fwrite_u32(0);
    fwrite_u32(0);

    // layer
    fwrite_u16(0);

    // alternate_group
    fwrite_u16(0);

    // volume
    if (is_video == VIDEO_TRACK) {
        fwrite_u16(0);
    } else {
        fwrite_u16(0x0100);
    }

    // reserved
    fwrite_u16(0);

    // matrix
    fwrite_u32(0x00010000);
    fwrite_u32(0);
    fwrite_u32(0);
    fwrite_u32(0);
    fwrite_u32(0x00010000);
    fwrite_u32(0);
    fwrite_u32(0);
    fwrite_u32(0);
    fwrite_u32(0x40000000);

    // width and height
    if (is_video == VIDEO_TRACK) {
        fwrite_u32(g_params.width << 16);
        fwrite_u32(g_params.height << 16);
    } else {
        fwrite_u32(0);
        fwrite_u32(0);
    }

    BOX_END("tkhd")
}

/**
 * @brief write edit list box
 * refer to 8.6.6 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class EditListBox extends FullBox(‘elst’, version, 0) {
        unsigned int(32) entry_count;
        for (i=1; i <= entry_count; i++) {
            if (version==1) {
                unsigned int(64) segment_duration;
                int(64) media_time;
            } else { // version==0
                unsigned int(32) segment_duration;
                int(32) media_time;
            }
            int(16) media_rate_integer;
            int(16) media_rate_fraction = 0;
        }
    }
 * @param is_video Identify whether the track is audio or video
 * @return u64 
 */
u64 write_elst(int is_video) {
    BOX_START

    u8 version = 0;
    write_full_box(-1, uchars2uint("elst"), g_ex_type, version, 0, &size_offset, &size_bytes);

    u32 entry_count = 1;
    // entry_count
    fwrite_u32(entry_count);

    u32 duration = is_video == VIDEO_TRACK ? g_params.video_duration : g_params.audio_duration;
    // loop
    for (int i = 1; i <= entry_count; i++) {
        if (1 == version) {
            // segment_duration
            fwrite_u64((u64)(g_params.time_scale * duration));
            // media_time
            fwrite_u64(0);
        } else {
            // segment_duration
            fwrite_u32((u32)(g_params.time_scale * duration));
            // media_time
            fwrite_u32(0);
        }

        // media_rate_integer
        fwrite_u16(1);
        // media_rate_fraction
        fwrite_u16(0);
    }

    BOX_END("elst")
}

/**
 * @brief write edit box
 * refer to 8.6.5 in ISO/IEC 14496-12
 * @param is_video Identify whether the track is audio or video
 * @return u64 size of box
 */
u64 write_edts(int is_video) {
    BOX_START

    write_box(-1, uchars2uint("edts"), g_ex_type, &size_offset, &size_bytes);

    write_elst(is_video);

    BOX_END("edts")
}

/**
 * @brief write media header box
 * refer to 8.4.2 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class MediaHeaderBox extends FullBox(‘mdhd’, version, 0) {
        if (version==1) {
            unsigned int(64) creation_time;
            unsigned int(64) modification_time;
            unsigned int(32) timescale;
            unsigned int(64) duration;
        } else { // version==0
            unsigned int(32) creation_time;
            unsigned int(32) modification_time;
            unsigned int(32) timescale;
            unsigned int(32) duration;
        }
        bit(1) pad = 0;
        unsigned int(5)[3] language; // ISO-639-2/T language code
        unsigned int(16) pre_defined = 0;
    }
 * @param is_video Identify whether the track is audio or video
 * @return u64 
 */
u64 write_mdhd(int is_video) {
    BOX_START

    u8 version = 0;
    write_full_box(-1, uchars2uint("mdhd"), g_ex_type, version, 0, &size_offset, &size_bytes);

    u32 time_scale = is_video == VIDEO_TRACK ? g_params.video_time_scale : g_params.audio_time_scale;
    i64 duration   = is_video == VIDEO_TRACK ? g_params.video_duration : g_params.audio_duration;
    if (g_params.is_fragment)
        duration = 0;

    if (1 == version) {
        // creation_time
        fwrite_u64(get_utc_time());
        //modification_time
        fwrite_u64(get_utc_time());
        // timescale
        fwrite_u32(time_scale);
        // duration
        fwrite_u64((u64)(time_scale * duration));
    } else {
        // creation_time
        fwrite_u32(get_utc_time());
        //modification_time
        fwrite_u32(get_utc_time());
        // timescale
        fwrite_u32(time_scale);
        // duration
        fwrite_u32((u32)(time_scale * duration));
    }

    // language
    const char *lang = "und";
    int i, code = 0;
    for (size_t i = 0; i < 3; i++)
    {
        u8 c = lang[i];
        c -= 0x60;
        code <<= 5;
        code |= c;
    }
    fwrite_u16(code);

    // pre_defined
    fwrite_u16(0);
    
    BOX_END("mdhd")
}

/**
 * @brief write handler reference box
 * refer to 8.4.3 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class HandlerBox extends FullBox(‘hdlr’, version = 0, 0) {
        unsigned int(32) pre_defined = 0;
        unsigned int(32) handler_type;
        const unsigned int(32)[3] reserved = 0;
        string name;
    }
 * @param is_video Identify whether the track is audio or video
 * @return u64 size of box
 */
u64 write_hdlr(int is_video) {
    BOX_START

    write_full_box(-1, uchars2uint("hdlr"), g_ex_type, 0, 0, &size_offset, &size_bytes);

    // pre_defined
    fwrite_u32(0);

    // handler_type
    u32 hdlr_type = (is_video == VIDEO_TRACK) ? uchars2uint("vide") : uchars2uint("soun");
    fwrite(&hdlr_type, sizeof(u32), 1, g_out_file);

    // reserved
    fwrite_u32(0);
    fwrite_u32(0);
    fwrite_u32(0);

    // name
    if (is_video == VIDEO_TRACK) {
        const char *name = "VideoHandler";
        fwrite(name, strlen(name) + 1, 1, g_out_file);
    } else {
        const char *name = "SoundHandler";
        fwrite(name, strlen(name) + 1, 1, g_out_file);
    }

    BOX_END("hdlr")
}

/**
 * @brief write video media header box
 * refer to 8.4.5.2 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class VideoMediaHeaderBox extends FullBox(‘vmhd’, version = 0, 1) {
        template unsigned int(16) graphicsmode = 0; // copy, see below
        template unsigned int(16)[3] opcolor = {0, 0, 0};
    }
 * @return u64 size of box
 */
u64 write_vmhd() {
    BOX_START

    write_full_box(-1, uchars2uint("vmhd"), g_ex_type, 0, 1, &size_offset, &size_bytes);

    // graphicsmode
    fwrite_u16(0); // copy = 0 copy over the existing image 

    // opcolor
    fwrite_u16(0);
    fwrite_u16(0);
    fwrite_u16(0);

    BOX_END("vmhd")
}

/**
 * @brief write sound media header box
 * refer to 8.4.5.3 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class SoundMediaHeaderBox extends FullBox(‘smhd’, version = 0, 0) {
        template int(16) balance = 0;
        const unsigned int(16) reserved = 0;
    }
 * @return u64 size of box
 */
u64 write_smhd() {
    BOX_START

    write_full_box(-1, uchars2uint("smhd"), g_ex_type, 0, 0, &size_offset, &size_bytes);

    // balance
    fwrite_u16(0); // centre: 0; left: -1.0; right: 1.0

    // reserved
    fwrite_u16(0);

    BOX_END("smhd")
}

/**
 * @brief write url box
 * refer to 8.7.2 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class DataEntryUrlBox (bit(24) flags) extends FullBox(‘url ’, version = 0, flags) {
        string location;
    }
 * @param flags one flag is defined (0x000001) which means that the media
 *              data is in the same file as the Movie Box containing this data reference
 * @return u64 size of box
 */
u64 write_url(u32 flags) {
    BOX_START

    write_full_box(-1, uchars2uint("url "), g_ex_type, 0, flags, &size_offset, &size_bytes);

    // location
    // If the flag is set indicating that the data is in the same file as this box, 
    // then no string (not even an empty one) shall be supplied in the entry field.

    BOX_END("url ")
}

/**
 * @brief write data reference box
 * refer to 8.7.2 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class DataReferenceBox extends FullBox(‘dref’, version = 0, 0) {
        unsigned int(32) entry_count;
        for (i=1; i <= entry_count; i++) {
            DataEntryBox(entry_version, entry_flags) data_entry;
        }
    }
 * @return u64 size of box
 */
u64 write_dref() {
    BOX_START

    write_full_box(-1, uchars2uint("dref"), g_ex_type, 0, 0, &size_offset, &size_bytes);

    // entry_count
    u32 entry_count = 1;
    fwrite_u32(entry_count);

    // DataEntryBox
    u32 entry_flags = 1;
    for (int i = 1; i <= entry_count; i++) {
        write_url(entry_flags);
    }

    BOX_END("dref")
}

/**
 * @brief write data information box
 * refer to 8.7.1 in ISO/IEC 14496-12
 * @return u64 size of box
 */
u64 write_dinf() {
    BOX_START

    write_box(-1, uchars2uint("dinf"), g_ex_type, &size_offset, &size_bytes);

    write_dref();

    BOX_END("dinf")
}

/**
 * @brief write tag and Length field
 * Length encoding:
 * ********** ********** ********** ********** 
 * *1*length* *1*length* *1*length* *0*length*
 * ********** ********** ********** **********
 * 1  7       1  7       1  7       1  7
 * 
 * implementing:
 * size >> 21 -> bit27 ~ bit21
 * size >> 14 -> bit20 ~ bit14
 * size >> 7  -> bit13 ~ bit7
 * size&0x7F  -> bit7  ~ bit0
 * @param tag 
 * @param size 
 */
static void put_descr(int tag, unsigned int size)
{
    int i = 3;
    fwrite(&tag, 1, 1, g_out_file);
    for (; i > 0; i--) {
        u8 tmp = (size >> (7 * i)) | 0x80;
        fwrite(&tmp, 1, 1, g_out_file);
    }
    u8 tmp = size & 0x7F;
    fwrite(&tmp, 1, 1, g_out_file);
}

/**
 * @brief write esds
 * refer to Annex E in ISO/IEC 14496-1 and movenc.c: mov_write_esds_tag function in ffmpeg
 * @return u64 size of box
 */
u64 write_esds() {
    BOX_START

    u8 audio_specific_config[2] = {0};
    int vos_len = create_audio_specific_config(g_aac_params->pkts[0].header.f_header, (u8 *)audio_specific_config);

    int decoder_specific_info_len = vos_len ? 5 + vos_len : 0; // 5: tag + size
    write_full_box(-1, uchars2uint("esds"), g_ex_type, 0, 0, &size_offset, &size_bytes);

    // 3: ES_ID + stream Dependency Flag + URL Flag + OCR stream flag + stream Priority
    // DecoderConfig descriptor: 5(tag + size) + 13 + decoder_specific_info_len
    // SL descriptor: 5(tag + size) + 1
    put_descr(0x03, 3 + 5+13 + decoder_specific_info_len + 5+1);

    // ES_ID: 16b
    fwrite_u16(g_params.audio_track_id);

    // stream Dependency Flag:  1b
    // URL Flag: 1b
    // OCR stream flag: 1b
    // stream Priority: 5b
    u8 zero_byte = 0x00;
    fwrite(&zero_byte, 1, 1, g_out_file);

    // DecoderConfig descriptor
    // 13: object type indication + (stream type + upStream + reserved) + bufferSizeDB + maxBitRate + BitRate
    // decoder_specific_info_len: tag + size + vos_data
    put_descr(0x04, 13 + decoder_specific_info_len);

    // object type indication 8bits
    u8 aac_codec_tag = 0x40;
    fwrite(&aac_codec_tag, 1, 1, g_out_file);

    // stream type: 6bits (0x05 for AudioStream)
    // upStream: 1bit
    // reserved: 1bit
    u8 audio_stream_flags = 0x15;
    fwrite(&audio_stream_flags, 1, 1, g_out_file);

    // bufferSizeDB: 24bits
    // maxBitRate: 32bits
    // avg BitRate: 32bits
    u32 buffer_size = 0;
    u32 max_bit_rate = compute_avg_bitrate(g_aac_params, g_params.audio_time_scale, g_params.audio_duration * g_params.audio_time_scale);
    u32 avg_bit_rate = max_bit_rate;
    fwrite_u24(buffer_size);
    fwrite_u32(max_bit_rate);
    fwrite_u32(avg_bit_rate);

    if (vos_len) {
        // DecoderSpecific info descriptor
        put_descr(0x05, vos_len);
        fwrite(audio_specific_config, 1, vos_len, g_out_file);
    }

    // SL descriptor
    // refer to 7.3.2.3 in ISO/IEC 14496-1
    put_descr(0x06, 1);
    u8 tmp = 0x02; // predefined = 0x02: Reserved for use in MP4 files
    fwrite(&tmp, 1, 1, g_out_file);

    BOX_END("esds");
}

/**
 * @brief write audio sample entry
 * refer to 8.5.2 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) abstract class SampleEntry (unsigned int(32) format) extends Box(format){
        const unsigned int(8)[6] reserved = 0;
        unsigned int(16) data_reference_index;
    }

    class AudioSampleEntry(codingname) extends SampleEntry (codingname){
        const unsigned int(32)[2] reserved = 0;
        template unsigned int(16) channelcount = 2;
        template unsigned int(16) samplesize = 16;
        unsigned int(16) pre_defined = 0;
        const unsigned int(16) reserved = 0 ;
        template unsigned int(32) samplerate = { default samplerate of media}<<16;
    }
 * @param codingname coding name
 */
u64 write_audio_sample_entry(const char *codingname) {
    BOX_START

    write_box(-1, uchars2uint((char *)codingname), g_ex_type, &size_offset, &size_bytes);

    // reserved
    // const unsigned int(8)[6] reserved = 0;
    fwrite_u16(0);
    fwrite_u32(0);

    // data_reference_index
    fwrite_u16(1);

    // reserved
    fwrite_u32(0);
    fwrite_u32(0);

    // channelcount
    fwrite_u16(g_params.audio_channel_count);

    // samplesize
    fwrite_u16(g_params.audio_sample_size);

    // pre_defined
    fwrite_u16(0);

    // reserved
    fwrite_u16(0);

    // samplerate
    fwrite_u32(g_params.audio_sample_rate << 16);

    if (!memcmp(codingname, "mp4a", 4)) {
        // write esds
        write_esds();
    }

    BOX_END(codingname)
}

/**
 * @brief write AVCDecoderConfigurationRecord
 * refer to "ISO/IEC 14496-15 (Part 15: Advanced Video Coding (AVC) file format)" 
 * pseudocode:
    aligned(8) class AVCDecoderConfigurationRecord {
        unsigned int(8) configurationVersion = 1;
        unsigned int(8) AVCProfileIndication;
        unsigned int(8) profile_compatibility;
        unsigned int(8) AVCLevelIndication;
        bit(6) reserved = '111111'b;
        unsigned int(2) lengthSizeMinusOne;
        bit(3) reserved = '111'b;
        unsigned int(5) numOfSequenceParameterSets;
        for (i=0; i< numOfSequenceParameterSets; i++) {
            unsigned int(16) sequenceParameterSetLength ;
            bit(8*sequenceParameterSetLength) sequenceParameterSetNALUnit;
        }
        unsigned int(8) numOfPictureParameterSets;
        for (i=0; i< numOfPictureParameterSets; i++) {
            unsigned int(16) pictureParameterSetLength;
            bit(8*pictureParameterSetLength) pictureParameterSetNALUnit;
        }
        if( profile_idc == 100 || profile_idc == 110 ||
            profile_idc == 122 || profile_idc == 144 )
        {
            bit(6) reserved = '111111'b;
            unsigned int(2) chroma_format;
            bit(5) reserved = '11111'b;
            unsigned int(3) bit_depth_luma_minus8;
            bit(5) reserved = '11111'b;
            unsigned int(3) bit_depth_chroma_minus8;
            unsigned int(8) numOfSequenceParameterSetExt;
            for (i=0; i< numOfSequenceParameterSetExt; i++) {
                unsigned int(16) sequenceParameterSetExtLength;
                bit(8*sequenceParameterSetExtLength) sequenceParameterSetExtNALUnit;
            }
        }
    }
 * @return u64 
 */
u64 write_avcC() {
    BOX_START

    write_box(-1, uchars2uint("avcC"), g_ex_type, &size_offset, &size_bytes);

    // configurationVersion
    u8 version = 1;
    fwrite(&version, sizeof(u8), 1, g_out_file);

    // AVCProfileIndication
    fwrite(&g_h264_params->profile_idc, sizeof(u8), 1, g_out_file);

    // profile_compatibility
    fwrite(&g_h264_params->constraint_set_flags, sizeof(u8), 1, g_out_file);

    // AVCLevelIndication
    fwrite(&g_h264_params->level_idc, sizeof(u8), 1, g_out_file);

    // lengthSizeMinusOne
    u8 tmp = 0xff; /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
    fwrite(&tmp, sizeof(u8), 1, g_out_file);

    // numOfSequenceParameterSets
    tmp = 0xe0 | 0x01; // one sps
    fwrite(&tmp, sizeof(u8), 1, g_out_file);

    // sequenceParameterSetLength
    fwrite_u16((u16)g_h264_params->sps_len);

    // sps data
    int ret = fwrite(g_h264_params->sps_buf, sizeof(u8), g_h264_params->sps_len, g_out_file);

    // numOfPictureParameterSets
    tmp = 0x01; // one pps
    fwrite(&tmp, sizeof(u8), 1, g_out_file);

    // pictureParameterSetLength
    fwrite_u16((u16)g_h264_params->pps_len);

    // pps data
    ret = fwrite(g_h264_params->pps_buf, sizeof(u8), g_h264_params->pps_len, g_out_file);

    if (g_h264_params->profile_idc == 100 || g_h264_params->profile_idc == 110 ||
        g_h264_params->profile_idc == 122 || g_h264_params->profile_idc == 144) {

        // chroma_format
        tmp = 0xfc |  g_h264_params->chroma_format_idc;   /* 6 bits reserved (111111) + chroma_format_idc */
        fwrite(&tmp, sizeof(u8), 1, g_out_file);

        // bit_depth_luma_minus8
        tmp = 0xf8 | (g_h264_params->bit_depth_luma - 8); // /* 5 bits reserved (11111) + bit_depth_luma_minus8 */
        fwrite(&tmp, sizeof(u8), 1, g_out_file);

        // bit_depth_chroma_minus8
        tmp = 0xf8 | (g_h264_params->bit_depth_chroma - 8); // /* 5 bits reserved (11111) + bit_depth_chroma_minus8 */
        fwrite(&tmp, sizeof(u8), 1, g_out_file);

        // numOfSequenceParameterSetExt
        tmp = 0;
        fwrite(&tmp, sizeof(u8), 1, g_out_file);
    }

    BOX_END("avcC")
}

/**
 * @brief write video sample entry
 * refer to 8.5.2 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) abstract class SampleEntry (unsigned int(32) format) extends Box(format){
        const unsigned int(8)[6] reserved = 0;
        unsigned int(16) data_reference_index;
    }

    class VisualSampleEntry(codingname) extends SampleEntry (codingname){
        unsigned int(16) pre_defined = 0;
        const unsigned int(16) reserved = 0;
        unsigned int(32)[3] pre_defined = 0;
        unsigned int(16) width;
        unsigned int(16) height;
        template unsigned int(32) horizresolution = 0x00480000; // 72 dpi
        template unsigned int(32) vertresolution = 0x00480000; // 72 dpi
        const unsigned int(32) reserved = 0;
        template unsigned int(16) frame_count = 1;
        string[32] compressorname;
        template unsigned int(16) depth = 0x0018;
        int(16) pre_defined = -1;
        // other boxes from derived specifications
        CleanApertureBox clap; // optional
        PixelAspectRatioBox pasp; // optional
    }
 * @param codingname coding name
 */
u64 write_video_sample_entry(const char *codingname) {
    BOX_START

    // ---SampleEntry start
    write_box(-1, uchars2uint((char *)codingname), g_ex_type, &size_offset, &size_bytes);

    // reserved
    fwrite_u16(0);
    fwrite_u32(0);

    // data_reference_index
    fwrite_u16(1);
    // ---SampleEntry end

    // ---VisualSampleEntry start
    // pre_defined
    fwrite_u16(0);

    // reserved
    fwrite_u16(0);

    // pre_defined
    fwrite_u32(0);
    fwrite_u32(0);
    fwrite_u32(0);

    // width
    fwrite_u16(g_params.width);

    // height
    fwrite_u16(g_params.height);

    // horizresolution
    fwrite_u32(0x00480000);

    // vertresolution
    fwrite_u32(0x00480000);

    // reserved
    fwrite_u32(0);

    // frame_count
    fwrite_u16(1);

    // compressorname
    // It is formatted in a fixed 32-byte field, 
    // with the first byte set to the number of bytes to be displayed,
    // followed by that number of bytes of displayable data, 
    // and then padding to complete 32 bytes total (including the size byte).
    char *compressor_name = "Lavc60.3.100 libx264";
    int str_len = strlen(compressor_name);
    fwrite(&str_len, 1, 1, g_out_file);
    fwrite(compressor_name, 31, 1, g_out_file);

    // depth
    // 0x0018 – images are in colour with no alpha 
    fwrite_u16(0x0018);

    // pre_defined
    fwrite_u16(0xffff);
    // ---VisualSampleEntry end

    if (!memcmp(codingname, "avc1", 4)) {
        write_avcC();
    }

    BOX_END(codingname)
}

/**
 * @brief write sample description box
 * refer to 8.5.2 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class SampleDescriptionBox (unsigned int(32) handler_type) extends FullBox('stsd', 0, 0) {
        int i ;
        unsigned int(32) entry_count;
        for (i = 1 ; i <= entry_count ; i++) {
            switch (handler_type) {
                case ‘soun’: // for audio tracks
                    AudioSampleEntry();
                    break;
                case ‘vide’: // for video tracks
                    VisualSampleEntry();
                    break;
                case ‘hint’: // Hint track
                    HintSampleEntry();
                    break;
                case ‘meta’: // Metadata track
                    MetadataSampleEntry();
                    break;
            }
        }
    }
 * @param is_video Identify whether the track is audio or video
 * @return u64 size of box
 */
u64 write_stsd(int is_video) {
    BOX_START

    write_full_box(-1, uchars2uint("stsd"), g_ex_type, 0, 0, &size_offset, &size_bytes);

    // entry_count
    int entry_count = 1;
    fwrite_u32(entry_count);

    // handler
    for (size_t i = 1; i <= entry_count; i++) {
        if (AUDIO_TRACK == is_video) {
            write_audio_sample_entry("mp4a");
        } else if (VIDEO_TRACK == is_video) {
            write_video_sample_entry("avc1");
        }
    }

    BOX_END("stsd")
}

/**
 * @brief write decoding time to sample box
 * refer to 8.6.1.2 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class TimeToSampleBox extends FullBox(’stts’, version = 0, 0) {
        unsigned int(32) entry_count;
        int i;
        for (i=0; i < entry_count; i++) {
            unsigned int(32) sample_count;
            unsigned int(32) sample_delta;
        }
    }
 * @param is_video Identify whether the track is audio or video
 * @return u64 size of box
 */
u64 write_stts(int is_video) {
    BOX_START

    write_full_box(-1, uchars2uint("stts"), g_ex_type, 0, 0, &size_offset, &size_bytes);

    if (0 == g_params.is_fragment) {
        // entry_count
        u32 entry_count = 1;
        fwrite_u32(entry_count);

        if (VIDEO_TRACK == is_video) {
            u32 sample_delta = g_params.video_time_scale * g_params.framerate_num / g_params.framerate_den; 

            fwrite_u32(g_h264_params->pkt_counts);
            fwrite_u32(sample_delta);
        } else if (AUDIO_TRACK == is_video) {
            fwrite_u32(g_aac_params->pkt_counts);
            fwrite_u32(AAC_FRAME_SIZE);
        }
    } else {
        fwrite_u32(0);
    }

    BOX_END("stts")
}

/**
 * @brief write sync sample box
 * refer to 8.6.2 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class SyncSampleBox extends FullBox(‘stss’, version = 0, 0) {
        unsigned int(32) entry_count;
        int i;
        for (i=0; i < entry_count; i++) {
            unsigned int(32) sample_number; //  index starts at 1
        }
    }
 * @param is_video Identify whether the track is audio or video
 * @return u64 size of box
 */
u64 write_stss(int is_video) {
    BOX_START

    write_full_box(-1, uchars2uint("stss"), g_ex_type, 0, 0, &size_offset, &size_bytes);

    int sync_video_sample_counts = 0;
    for (size_t i = 0; i < g_h264_params->pkt_counts; i++)
    {
        if (g_h264_params->pkts[i].type == 5) {
            sync_video_sample_counts++;
        }
    }

    // entry_count
    fwrite_u32(sync_video_sample_counts);

    // sample_number
    for (size_t i = 0; i < g_h264_params->pkt_counts; i++)
    {
        if (g_h264_params->pkts[i].type == 5) {
            fwrite_u32(i + 1);
        }
    }
    
    BOX_END("stss")
}

/**
 * @brief write sample to chunk box
 * refer to 8.7.4 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class SampleToChunkBox extends FullBox(‘stsc’, version = 0, 0) {
        unsigned int(32) entry_count;
        for (i=1; i <= entry_count; i++) {
            unsigned int(32) first_chunk;
            unsigned int(32) samples_per_chunk;
            unsigned int(32) sample_description_index;
        }
    }
 * @param is_video 
 * @return u64 size of box
 */
u64 write_stsc(int is_video) {
    BOX_START

    write_full_box(-1, uchars2uint("stsc"), g_ex_type, 0, 0, &size_offset, &size_bytes);

    if (0 == g_params.is_fragment) {
        if (VIDEO_TRACK == is_video) {
            off_t entry_count_pos = ftello(g_out_file);
            fwrite_u32(0);

            /**
             * entry | first-chunk | samples-per-chunk | sample-description-index
             * 0     | 1           | 25                | 1
             * 1     | 2           | 25                | 1
             * 2     | 3           | 2                 | 1
            */

            u32 entry_count = 0;
            for (size_t i = 0; i < g_h264_params->pkt_counts; )
            {
                u32 first_chunk = ++entry_count;
                u32 samples_per_chunk = (g_h264_params->pkt_counts - i) >= 25 ? 25 : (g_h264_params->pkt_counts - i);
                u32 sample_description_index = 1;

                fwrite_u32(first_chunk);
                fwrite_u32(samples_per_chunk);
                fwrite_u32(sample_description_index);

                g_params.chunk_entry_count++;
                g_params.chunk_of_sample_index = realloc(g_params.chunk_of_sample_index, sizeof(u32) * g_params.chunk_entry_count);
                g_params.chunk_of_sample_index[g_params.chunk_entry_count - 1] = i;

                i += samples_per_chunk;
            }

            off_t backup_pos = ftello(g_out_file);
            int ret = fseeko(g_out_file, entry_count_pos, SEEK_SET);
            fwrite_u32(entry_count);
            ret = fseeko(g_out_file, backup_pos, SEEK_SET);
        } else if (AUDIO_TRACK == is_video) {
            fwrite_u32(1);

            fwrite_u32(1);
            fwrite_u32(g_aac_params->pkt_counts);
            fwrite_u32(1);
        }
    } else {
        fwrite_u32(0);
    }

    BOX_END("stsc")
}

/**
 * @brief write sample size box
 * 
 * @param is_video 
 * @return u64 
 */
u64 write_stsz(int is_video) {
    BOX_START

    write_full_box(-1, uchars2uint("stsz"), g_ex_type, 0, 0, &size_offset, &size_bytes);

    if (0 == g_params.is_fragment) {
        if (VIDEO_TRACK == is_video) {
            // sample_size
            u32 sample_size = 0;
            fwrite_u32(sample_size);

            // sample_count
            fwrite_u32(g_params.video_sample_counts);

            if (0 == sample_size) {
                for (size_t i = 0; i < g_params.video_sample_counts; i++) {
                    fwrite_u32(g_params.video_sample_infos[i].size);
                }
            }
        } else if (AUDIO_TRACK == is_video) {
            // sample_size
            u32 sample_size = 0;
            fwrite_u32(sample_size);

            // sample_count
            fwrite_u32(g_aac_params->pkt_counts);

            if (0 == sample_size) {
                for (size_t i = 0; i < g_aac_params->pkt_counts; i++) {
                    fwrite_u32(g_aac_params->pkts[i].frame_size);
                }
            }
        }
    } else {
        fwrite_u32(0);
        fwrite_u32(0);
    }

    BOX_END("stsz")
}

/**
 * @brief write chunk offset box
 * refer to 8.7.5 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class ChunkOffsetBox extends FullBox(‘stco’, version = 0, 0) {
        unsigned int(32) entry_count;
        for (i=1; i <= entry_count; i++) {
            unsigned int(32) chunk_offset;
        }
    }
 * @param is_video 
 * @return u64 size of box
 */
u64 write_stco(int is_video) {
    BOX_START

    write_full_box(-1, uchars2uint("stco"), g_ex_type, 0, 0, &size_offset, &size_bytes);

    if (0 == g_params.is_fragment) {
        if (VIDEO_TRACK == is_video) {
            // entry_count
            fwrite_u32(g_params.chunk_entry_count);

            // chunk_offset
            for (int i = 0; i < g_params.chunk_entry_count; i++) {
                u32 sample_index = g_params.chunk_of_sample_index[i];
                off_t offset = g_params.video_sample_infos[sample_index].offset;
                fwrite_u32(offset);
            }
        } else if (AUDIO_TRACK == is_video) {
            // entry_count
            fwrite_u32(1);

            // chunk_offset
            off_t offset = g_params.audio_sample_infos[0].offset;
            fwrite_u32(offset);
        }
    } else {
        fwrite_u32(0);
    }

    BOX_END("stco")
}

/**
 * @brief write chunk offset box
 * refer to 8.7.5 in ISO/IEC 14496-12
    aligned(8) class ChunkLargeOffsetBox extends FullBox(‘co64’, version = 0, 0) {
        unsigned int(32) entry_count;
        for (i=1; i <= entry_count; i++) {
            unsigned int(64) chunk_offset;
        }
    }
 * @param is_video 
 * @return u64 size of box
 */
u64 write_co64(int is_video) {
    BOX_START

    write_full_box(-1, uchars2uint("co64"), g_ex_type, 0, 0, &size_offset, &size_bytes);

    if (0 == g_params.is_fragment) {
        if (VIDEO_TRACK == is_video) {
            // entry_count
            fwrite_u32(g_params.chunk_entry_count);

            // chunk_offset
            for (int i = 0; i < g_params.chunk_entry_count; i++) {
                u32 sample_index = g_params.chunk_of_sample_index[i];
                off_t offset = g_params.video_sample_infos[sample_index].offset;
                fwrite_u64(offset);
            }
        } else if (AUDIO_TRACK == is_video) {
            // entry_count
            fwrite_u32(1);

            // chunk_offset
            off_t offset = g_params.audio_sample_infos[0].offset;
            fwrite_u64(offset);
        }
    } else {
        fwrite_u32(0);
    }

    BOX_END("co64")
}

/**
 * @brief write sample table box
 * refer to 8.5.1 in ISO/IEC 14496-12
 * @param is_video Identify whether the track is audio or video
 * @return u64 size of box
 */
u64 write_stbl(int is_video) {
    BOX_START

    write_box(-1, uchars2uint("stbl"), g_ex_type, &size_offset, &size_bytes);

    write_stsd(is_video);

    write_stts(is_video);

    if (VIDEO_TRACK == is_video && 0 == g_params.is_fragment)
        write_stss(is_video);

    write_stsc(is_video);

    write_stsz(is_video);

    //write_stco(is_video);
    write_co64(is_video);

    BOX_END("stbl")
}

/**
 * @brief write media information box
 * refer to 8.4.4 in ISO/IEC 14496-12
 * @param is_video Identify whether the track is audio or video
 * @return u64 
 */
u64 write_minf(int is_video) {
    BOX_START

    write_box(-1, uchars2uint("minf"), g_ex_type, &size_offset, &size_bytes);

    if (is_video == VIDEO_TRACK) {
        write_vmhd();
    } else {
        write_smhd();
    }

    write_dinf();

    write_stbl(is_video);

    BOX_END("minf")
}

/**
 * @brief write media box
 * refer to 8.4.1 in 8.4.1 
 * @param is_video Identify whether the track is audio or video
 * @return u64 size of box
 */
u64 write_mdia(int is_video) {
    BOX_START

    write_box(-1, uchars2uint("mdia"), g_ex_type, &size_offset, &size_bytes);

    write_mdhd(is_video);

    write_hdlr(is_video);

    write_minf(is_video);

    BOX_END("mdia")
}

/**
 * @brief write track box
 * refer to 8.3.1 in ISO/IEC 14496-12
 * @param is_video Identify whether the track is audio or video 
 * @return u64 size of box
 */
u64 write_trak(int is_video) {
    BOX_START

    write_box(-1, uchars2uint("trak"), g_ex_type, &size_offset, &size_bytes);

    write_tkhd(is_video);

    if (0 == g_params.is_fragment)
        write_edts(is_video);

    write_mdia(is_video);

    BOX_END("trak")
}

/**
 * @brief write media data box
 * refer to 8.1.1 in ISO/IEC 14496-12
 * write video data and/or audio data to output file,
 * and records offset of every video sample and audio frame
 * @param video_file video file path
 * @param audio_file audio file path
 * @return u64 return box size
 */
u64 write_mdat(const char *video_file, const char *audio_file) {
    BOX_START

    // large size
    write_box(tobig(1), uchars2uint("mdat"), g_ex_type, &size_offset, &size_bytes);

    // size
    //write_box(-1, uchars2uint("mdat"), g_ex_type, &size_offset, &size_bytes);

    // write video data
    FILE *pf_video = fopen(video_file, "rb");
    if (!pf_video) {
        fprintf(stderr, "[write_mdat] open %s failed.\n", video_file);
    } else {
        for (size_t i = 0; i < g_h264_params->pkt_counts; i++)
        {
            H264PacketInfo packet_info = g_h264_params->pkts[i];
            fseeko(pf_video, packet_info.file_offset, SEEK_SET);
            fread(g_params.in_buf, sizeof(u8), packet_info.len, pf_video);
            u32 ret = annexb2mp4(g_params.in_buf, packet_info.len, g_params.out_buf);
            if (ret) {
                off_t sample_start_offset = ftello(g_out_file);
                fwrite(g_params.out_buf, sizeof(u8), ret, g_out_file);
                g_params.video_sample_counts++;
                g_params.video_sample_infos = (SampleInfo *)realloc(g_params.video_sample_infos, sizeof(SampleInfo) * g_params.video_sample_counts);
                g_params.video_sample_infos[g_params.video_sample_counts - 1].sample_index = i;
                g_params.video_sample_infos[g_params.video_sample_counts - 1].size         = ret;
                g_params.video_sample_infos[g_params.video_sample_counts - 1].offset       = sample_start_offset;
            }
        }
    }

    // write audio data
    FILE *pf_audio = fopen(audio_file, "rb");
    if (!pf_audio) {
        fprintf(stderr, "[write_mdat] open %s failed.\n", audio_file);
    } else {
        for (size_t i = 0; i < g_aac_params->pkt_counts; i++) {
            u32 frame_size = g_aac_params->pkts[i].frame_size;

            fseeko(pf_audio, g_aac_params->pkts[i].file_offset, SEEK_SET);
            fread(g_params.in_buf, sizeof(u8), frame_size, pf_audio);

            off_t sample_start_offset = ftello(g_out_file);
            fwrite(g_params.in_buf, sizeof(u8), frame_size, g_out_file);

            g_params.audio_sample_counts++;
            g_params.audio_sample_infos = (SampleInfo *)realloc(g_params.audio_sample_infos, sizeof(SampleInfo) * g_params.audio_sample_counts);
            g_params.audio_sample_infos[g_params.audio_sample_counts - 1].sample_index = i;
            g_params.audio_sample_infos[g_params.audio_sample_counts - 1].size         = frame_size;
            g_params.audio_sample_infos[g_params.audio_sample_counts - 1].offset       = sample_start_offset;
        }
    }

    goto ok;

fail:
    return -1;
    
ok:
    if (pf_video) {
        fclose(pf_video);
    }
    if (pf_audio) {
        fclose(pf_audio);
    }
    BOX_END("mdat")
}

/**
 * @brief write Movie Extends header box
 * refer to 8.8.2 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class MovieExtendsHeaderBox extends FullBox(‘mehd’, version, 0) { 
        if (version==1) {
            unsigned int(64)  fragment_duration;
        } else { // version==0
            unsigned int(32)  fragment_duration;
        }
    }
 * The fragment_duration corresponds to the duration of the longest track, including movie fragments
 * 
 * @return u64 
 */
u64 write_mehd() {
    BOX_START

    u8 version = 0;
    write_full_box(-1, uchars2uint("mehd"), g_ex_type, version, 0, &size_offset, &size_bytes);

    i64 max_duration = g_params.video_duration > g_params.audio_duration ? g_params.video_duration : g_params.audio_duration;
    if (0 == version) {
        fwrite_u32((u32)(max_duration * g_params.time_scale));
    } else {
        fwrite_u64((u64)(max_duration * g_params.time_scale));
    }

    BOX_END("mehd")
}

/**
 * @brief write track extends box
 * refer to 8.8.3 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class TrackExtendsBox extends FullBox(‘trex’, 0, 0) { 
        unsigned int(32) track_ID;
        unsigned int(32) default_sample_description_index;
        unsigned int(32) default_sample_duration;
        unsigned int(32) default_sample_size;
        unsigned int(32) default_sample_flags
    }
    default_sample_flags has following structure:
        bit(4) reserved=0;
        unsigned int(2) is_leading;
        unsigned int(2) sample_depends_on;
        unsigned int(2) sample_is_depended_on; unsigned int(2) sample_has_redundancy;
        bit(3) sample_padding_value;
        bit(1) sample_is_non_sync_sample;
        unsigned int(16) sample_degradation_priority;
 * @param is_video Identify whether the track is audio or video 
 * @return u64 size of box
 */
u64 write_trex(int is_video) {
    BOX_START

    write_full_box(-1, uchars2uint("trex"), g_ex_type, 0, 0, &size_offset, &size_bytes);

    // track_ID
    if (VIDEO_TRACK == is_video) {
        fwrite_u32(g_params.video_track_id);
    } else {
        fwrite_u32(g_params.audio_track_id);
    }

    // default_sample_description_index
    fwrite_u32(1);

    // default_sample_duration
    fwrite_u32(0);

    // default_sample_size
    fwrite_u32(0);

    // default_sample_flags
    fwrite_u32(0);

    BOX_END("trex")
}

/**
 * @brief write Movie Extends Box
 * refer to 8.8.1 in ISO/IEC 14496-12
 * @return u64 size of box
 */
u64 write_mvex() {
    BOX_START

    write_box(-1, uchars2uint("mvex"), g_ex_type, &size_offset, &size_bytes);

    write_mehd();

    if (file_exist((const char*)g_params.video_path))
        write_trex(VIDEO_TRACK);  // video track Extends

    if (file_exist((const char*)g_params.audio_path))
        write_trex(AUDIO_TRACK);  // audio track Extends

    BOX_END("mvex")
}

/**
 * @brief write movie box
 * refer to 8.2.1 in ISO/IEC 14496-12
 * @return u64 return size of movie box
 */
u64 write_moov() {
    BOX_START

    write_box(-1, uchars2uint("moov"), g_ex_type, &size_offset, &size_bytes);

    u8 mvhd_version = 0;
    write_mvhd(mvhd_version);

    if (file_exist((const char*)g_params.video_path))
        write_trak(VIDEO_TRACK);  // video track

    if (file_exist((const char*)g_params.audio_path))
        write_trak(AUDIO_TRACK);  // audio track

    if (g_params.is_fragment) {
        write_mvex();
    }
    BOX_END("moov")
}