cmake_minimum_required(VERSION 3.22.1)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(AVIF_CODEC_DAV1D "LOCAL" CACHE STRING "" FORCE)
set(AVIF_LIBYUV "LOCAL" CACHE STRING "" FORCE)

add_library(avif_shared STATIC alpha.c avif.c colr.c colrconvert.c codec_aom.c
        diag.c exif.c gainmap.c io.c mem.c obu.c
        rawdata.c read.c reformat.c reformat_libyuv.c
        scale.c stream.c utils.c write.c codec_dav1d.c reformat_libsharpyuv.c)

target_compile_definitions(avif_shared PRIVATE AVIF_CODEC_AOM_ENCODE AVIF_CODEC_AOM AVIF_LIBYUV_ENABLED AVIF_CODEC_DAV1D)

target_include_directories(avif_shared PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/avif ${CMAKE_SOURCE_DIR}/libyuv)