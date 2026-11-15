ffmpeg -i 1.h264 -c:v libx264 -x264-params keyint=25:no-scenecut=1 -bf 0 -t 00:00:05 5s.h264
ffmpeg -i 1.aac -t 00:00:05 5s.aac

ffmpeg -i 1.aac -ac 2 -ar 48000 -f s16le s16le.pcm