#!/bin/bash
set -e

export NDK=$NDK_PATH

destination_directory=x265
if [ ! -d "$destination_directory" ]; then
    git clone https://github.com/videolan/x265
else
    echo "Destination directory '$destination_directory' already exists. Cloning skipped."
fi

cd x265

if [ -z "$INCLUDE_X86" ]; then
  ABI_LIST="armeabi-v7a arm64-v8a x86_64"
  echo "X86 won't be included into a build"
else
  ABI_LIST="armeabi-v7a arm64-v8a x86 x86_64"
fi

for abi in ${ABI_LIST}; do
  rm -rf "build-${abi}"
  mkdir "build-${abi}"
  cd "build-${abi}"
 ARCH_OPTIONS=""
  case ${abi} in
      arm-v7a | arm-v7a-neon | armeabi-v7a)
         ARCH_OPTIONS="-DENABLE_ASSEMBLY=0 -DCROSS_COMPILE_ARM=1"
      ;;
      arm64-v8a)
         ARCH_OPTIONS="-DENABLE_ASSEMBLY=0 -DCROSS_COMPILE_ARM=1"
      ;;
      x86)
         ARCH_OPTIONS="-DENABLE_ASSEMBLY=0 -DCROSS_COMPILE_ARM=0"
      ;;
      x86_64)
         ARCH_OPTIONS="-DENABLE_ASSEMBLY=0 -DCROSS_COMPILE_ARM=0"
      ;;
  esac

  echo $ARCH_OPTIONS

  cmake ../source \
    -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake -DANDROID_ABI=${abi} -DCMAKE_ANDROID_ARCH_ABI=${abi} \
    -DANDROID_NDK=${NDK_PATH} \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_SYSTEM_NAME=Generic \
    -DCMAKE_ANDROID_STL_TYPE=c++_shared \
    -DCMAKE_SYSTEM_NAME=Android \
    -DCMAKE_THREAD_PREFER_PTHREAD=TRUE \
    -DTHREADS_PREFER_PTHREAD_FLAG=TRUE \
    $ARCH_OPTIONS \
    -DBUILD_STATIC_LIBS=ON
  sed -i '' 's/-lpthread/-pthread/' CMakeFiles/cli.dir/link.txt
  sed -i '' 's/-lpthread/-pthread/' CMakeFiles/x265-shared.dir/link.txt
  sed -i '' 's/-lpthread/-pthread/' CMakeFiles/x265-static.dir/link.txt
  cmake --build .
  cd ..
done

for abi in ${ABI_LIST}; do
  mkdir -p "../avif-coder/src/main/cpp/lib/${abi}"
  cp -r "build-${abi}/libx265.a" "../avif-coder/src/main/cpp/lib/${abi}/libx265.a"
  echo "build-${abi}/libx265.a was successfully copied to ../avif-coder/src/main/cpp/lib/${abi}/libx265.a!"
done