# Introduction

The main purpose of this project is to learn the mp4 protocol.
It now supports h264 and aac.

# Usage

cmake is used for build.

```shell
mkdir build
cd build
cmake ..
make
./Mp4Muxer ../input.cfg
```

before you execute Mp4Muxer, you must modify input.cfg which contains video and audio parameters,
It looks like this:

```
# video bitstream file path
videopath=../test/5s.h264

# video width
width=1920

# video height
height=1080

# video frame rate
framerate=25/1

# video duration unit of second
videoduration=5

# audio bitstream file path
audiopath=../test/5s.aac

# audio channel count
channelcount=2

# audio sample rate
samplerate=48000

# audio duration unit of second
audioduration=5

# output mp4 file path
outputpath=output.mp4

# fragment
fragment=1
```

`fragment=1` will generate fragment mp4.

For 1.h264 and 1.aac we can get them by:

```shell
ffmpeg -i input -an -r 25 -c:v libx264 -bf 0 -t 00:05:00 1.h264
ffmpeg -i input -vn -ar 48000 -ac 2 -t 00:05:00 1.aac
```

For 5s.h264 and 5s.aac we can get them by:
```shell
ffmpeg -i 1.h264 -c:v libx264 -x264-params keyint=25:no-scenecut=1 -bf 0 -t 00:00:05 5s.h264
ffmpeg -i 1.aac -t 00:00:05 5s.aac
```

# normal boxes

![mp4-box](box.png)

# fragment boxes

![fmp4-box](fmp4-box.png)
