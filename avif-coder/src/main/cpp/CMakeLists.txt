# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.22.1)

project("coder")

add_library(coder SHARED coder.cpp JniException.cpp SizeScaler.cpp
        icc/cmsalpha.c icc/cmscam02.c
        icc/cmscgats.c icc/cmscnvrt.c icc/cmserr.c icc/cmsgamma.c
        icc/cmsgmt.c icc/cmshalf.c icc/cmsintrp.c icc/cmsio0.c
        icc/cmsio1.c icc/cmslut.c icc/cmsmd5.c icc/cmsmtrx.c icc/cmsnamed.c
        icc/cmsopt.c icc/cmspack.c icc/cmspcs.c icc/cmsplugin.c
        icc/cmsps2.c icc/cmssamp.c icc/cmssm.c icc/cmstypes.c icc/cmsvirt.c
        icc/cmswtpnt.c icc/cmsxform.c colorspace/colorspace.cpp
        imagebits/RgbaF16bitToNBitU16.cpp imagebits/RgbaF16bitNBitU8.cpp imagebits/Rgb1010102.cpp
        colorspace/HDRTransferAdapter.cpp
        imagebits/CopyUnalignedRGBA.cpp JniDecoder.cpp imagebits/Rgba8ToF16.cpp
        imagebits/Rgb565.cpp JniBitmap.cpp ReformatBitmap.cpp Support.cpp IccRecognizer.cpp
        HardwareBuffersCompat.cpp VulkanRunner.cpp imagebits/half.cpp
        imagebits/half.hpp imagebits/RgbaU16toHF.cpp
        imagebits/RGBAlpha.cpp)

add_library(libaom STATIC IMPORTED)
add_library(libx265 STATIC IMPORTED)
add_library(libheif STATIC IMPORTED)
add_library(libyuv STATIC IMPORTED)
add_library(libde265 STATIC IMPORTED)
add_library(libdav1d STATIC IMPORTED)
add_library(libhwy STATIC IMPORTED)
add_library(libsharpyuv STATIC IMPORTED)

set_target_properties(libaom PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libaom.a)
set_target_properties(libx265 PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libx265.a)
set_target_properties(libheif PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libheif.a)
set_target_properties(libyuv PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libyuv.a)
set_target_properties(libde265 PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libde265.a)
set_target_properties(libdav1d PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libdav1d.a)
set_target_properties(libhwy PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libhwy.a)
set_target_properties(libsharpyuv PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libsharpyuv.a)

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

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -ffp-mode=fast")
endif ()
add_definitions(-DCMS_NO_REGISTER_KEYWORD -DSTB_IMAGE_RESIZE_IMPLEMENTATION)

set(CMAKE_ANDROID_API_MIN 24)

target_include_directories(coder PRIVATE ${CMAKE_SOURCE_DIR}/libheif ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/libyuv ${CMAKE_SOURCE_DIR}/colorspace)

find_library( # Sets the name of the path variable.
        log-lib
        log)

find_library(
        android-lib
        android)

include_directories(icc)

target_link_libraries( # Specifies the target library.
        coder

        # Links the target library to the log library
        # included in the NDK.
        ${log-lib} libaom libx265 libheif cpufeatures libyuv -ljnigraphics
        libde265 libdav1d libhwy libsharpyuv ${android-lib})