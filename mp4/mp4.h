#ifndef _MP4_H_
#define _MP4_H_

#include <stdio.h>
#include "util.h"
#include "h264parser.h"
#include "aacparser.h"

u64 write_ftyp();
u64 write_mdat(const char *video_file, const char *audio_file);
u64 write_moov();

#endif