cmake_minimum_required(VERSION 2.8...3.24)

project(Mp4Muxer)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")

aux_source_directory("${PROJECT_SOURCE_DIR}/util" DIR_UTIL)
aux_source_directory("${PROJECT_SOURCE_DIR}/h264" DIR_H264)
aux_source_directory("${PROJECT_SOURCE_DIR}/aac" DIR_AAC)
aux_source_directory("${PROJECT_SOURCE_DIR}/mp4" DIR_MP4)
aux_source_directory(. DIR_MAIN)

include_directories("${PROJECT_SOURCE_DIR}/util")
include_directories("${PROJECT_SOURCE_DIR}/h264")
include_directories("${PROJECT_SOURCE_DIR}/aac")
include_directories("${PROJECT_SOURCE_DIR}/mp4")

add_executable(Mp4Muxer ${DIR_UTIL} ${DIR_H264} ${DIR_AAC} ${DIR_MP4} ${DIR_MAIN})