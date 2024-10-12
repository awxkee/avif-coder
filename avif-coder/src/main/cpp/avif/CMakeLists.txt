cmake_minimum_required(VERSION 3.22.1)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(AVIF_CODEC_DAV1D "LOCAL" CACHE STRING "" FORCE)
set(AVIF_LIBYUV "LOCAL" CACHE STRING "" FORCE)
set(AVIF_CODEC_LIBGAV1 OFF CACHE BOOL "" FORCE)

add_library(avif_shared STATIC alpha.c avif.c codec_svt.c colr.c colrconvert.c
        diag.c exif.c gainmap.c io.c mem.c obu.c
        rawdata.c read.c reformat.c reformat_libyuv.c
        scale.c stream.c utils.c write.c)

target_compile_definitions(avif_shared PRIVATE AVIF_CODEC_SVT AVIF_LIBYUV_ENABLED)

target_include_directories(avif_shared PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/.. ${CMAKE_SOURCE_DIR}/libyuv)