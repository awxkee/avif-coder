cmake_minimum_required(VERSION 3.22.1)

project("coder")

add_library(coder SHARED JniEncoder.cpp JniException.cpp SizeScaler.cpp
        colorspace/colorspace.cpp
        imagebits/RgbaF16bitToNBitU16.cpp imagebits/Rgb1010102.cpp
        imagebits/CopyUnalignedRGBA.cpp JniDecoder.cpp imagebits/Rgba8ToF16.cpp
        imagebits/Rgb565.cpp JniBitmap.cpp ReformatBitmap.cpp Support.cpp
        HardwareBuffersCompat.cpp imagebits/half.cpp
        imagebits/half.hpp
        imagebits/RGBAlpha.cpp
        imagebits/Rgba16.cpp
        AvifDecoderController.cpp JniAnimatedController.cpp
)

add_library(libyuv STATIC IMPORTED)
add_library(libdav1d SHARED IMPORTED)
add_library(avifweaver STATIC IMPORTED)

set_target_properties(libyuv PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libyuv.a)
set_target_properties(libdav1d PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libdav1d.so)
set_target_properties(avifweaver PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libavifweaver.a)

target_link_options(coder PRIVATE "-Wl,-z,max-page-size=16384")

add_library(cpufeatures STATIC ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c)
target_include_directories(cpufeatures PUBLIC ${ANDROID_NDK}/sources/android/cpufeatures)
target_link_libraries(cpufeatures dl)
list(APPEND EXTRA_LIBS cpufeatures)

include(CheckCXXCompilerFlag)
if (ANDROID_ABI STREQUAL arm64-v8a)
    add_definitions("-DHAVE_NEON=1")
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DHWY_COMPILE_ONLY_STATIC)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif ()
add_definitions(-DCMS_NO_REGISTER_KEYWORD)

set(CMAKE_ANDROID_API_MIN 24)

target_include_directories(coder PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/libyuv
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
        coder
        ${log-lib} cpufeatures libyuv -ljnigraphics avif_shared
        libdav1d ${android-lib} avifweaver m c)