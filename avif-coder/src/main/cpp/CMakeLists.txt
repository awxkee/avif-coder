cmake_minimum_required(VERSION 3.22.1)

project("coder")

add_library(coder SHARED JniEncoder.cpp JniException.cpp SizeScaler.cpp
        icc/cmsalpha.c icc/cmscam02.c
        icc/cmscgats.c icc/cmscnvrt.c icc/cmserr.c icc/cmsgamma.c
        icc/cmsgmt.c icc/cmshalf.c icc/cmsintrp.c icc/cmsio0.c
        icc/cmsio1.c icc/cmslut.c icc/cmsmd5.c icc/cmsmtrx.c icc/cmsnamed.c
        icc/cmsopt.c icc/cmspack.c icc/cmspcs.c icc/cmsplugin.c
        icc/cmsps2.c icc/cmssamp.c icc/cmssm.c icc/cmstypes.c icc/cmsvirt.c
        icc/cmswtpnt.c icc/cmsxform.c colorspace/colorspace.cpp
        imagebits/RgbaF16bitToNBitU16.cpp imagebits/RgbaF16bitNBitU8.cpp imagebits/Rgb1010102.cpp
        imagebits/CopyUnalignedRGBA.cpp JniDecoder.cpp imagebits/Rgba8ToF16.cpp
        imagebits/Rgb565.cpp JniBitmap.cpp ReformatBitmap.cpp Support.cpp IccRecognizer.cpp
        HardwareBuffersCompat.cpp imagebits/half.cpp
        imagebits/half.hpp imagebits/RgbaU16toHF.cpp
        imagebits/RGBAlpha.cpp
        colorspace/Trc.cpp YuvConversion.cpp
        colorspace/Rec2408ToneMapper.cpp colorspace/LogarithmicToneMapper.cpp
        colorspace/ColorMatrix.cpp imagebits/ScanAlpha.cpp imagebits/Rgba16.cpp
        AvifDecoderController.cpp HeifImageDecoder.cpp JniAnimatedController.cpp)

add_library(libkvazaar SHARED IMPORTED)
add_library(libheif SHARED IMPORTED)
add_library(libyuv STATIC IMPORTED)
add_library(libde265 SHARED IMPORTED)
add_library(libdav1d SHARED IMPORTED)
add_library(libsharpyuv STATIC IMPORTED)
add_library(aom SHARED IMPORTED)
add_library(avifweaver SHARED IMPORTED)

set_target_properties(libkvazaar PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libkvazaar.so)
set_target_properties(libheif PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libheif.so)
set_target_properties(libyuv PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libyuv.a)
set_target_properties(libde265 PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libde265.so)
set_target_properties(libdav1d PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libdav1d.so)
set_target_properties(libsharpyuv PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libsharpyuv.a)
set_target_properties(aom PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libaom.so)
set_target_properties(avifweaver PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libavifweaver.so)

add_library(cpufeatures STATIC ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c)
target_include_directories(cpufeatures PUBLIC ${ANDROID_NDK}/sources/android/cpufeatures)
target_link_libraries(cpufeatures dl)
list(APPEND EXTRA_LIBS cpufeatures)

include(CheckCXXCompilerFlag)
if (ANDROID_ABI STREQUAL arm64-v8a)
    add_definitions("-DHAVE_NEON=1")
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=neon -mfloat-abi=hard -ftree-vectorize")
    endif ()
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DHWY_COMPILE_ONLY_STATIC)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -ffp-mode=fast")
endif ()
add_definitions(-DCMS_NO_REGISTER_KEYWORD -DSTB_IMAGE_RESIZE_IMPLEMENTATION)

set(CMAKE_ANDROID_API_MIN 24)

target_include_directories(coder PRIVATE ${CMAKE_SOURCE_DIR}/libheif
        ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/libyuv
        ${CMAKE_SOURCE_DIR}/colorspace ${CMAKE_SOURCE_DIR}/algo)

find_library( # Sets the name of the path variable.
        log-lib
        log)

find_library(
        android-lib
        android)

include_directories(icc)

add_subdirectory(avif)

target_link_libraries( # Specifies the target library.
        coder PRIVATE
        ${log-lib} libheif cpufeatures libyuv -ljnigraphics libkvazaar aom avif_shared
        libde265 libdav1d libsharpyuv ${android-lib} avifweaver)