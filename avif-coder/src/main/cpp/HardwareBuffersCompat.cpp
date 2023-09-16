//
// Created by Radzivon Bartoshyk on 16/09/2023.
//

#include "HardwareBuffersCompat.h"

#include <mutex>
#include <dlfcn.h>
#include "JniException.h"
#include "jni.h"

std::mutex dlMutex;

bool alreadyHdBuffersLoaded = false;

AHardwareBufferAllocateFunc AHardwareBuffer_allocate_compat = nullptr;
AHardwareBufferIsSupportedFunc AHardwareBuffer_isSupported_compat = nullptr;
AHardwareBufferUnlockFunc AHardwareBuffer_unlock_compat = nullptr;
AHardwareBufferReleaseFunc AHardwareBuffer_release_compat = nullptr;
AHardwareBufferLockFunc AHardwareBuffer_lock_compat = nullptr;
AHardwareBufferToHardwareBufferFunc AHardwareBuffer_toHardwareBuffer_compat = nullptr;
AHardwareBufferDescribeFunc AHardwareBuffer_describe_compat = nullptr;

bool loadAHardwareBuffersAPI() {
    // Bitmap doesn't support wrapping before 29 API
    if (androidOSVersion() < 29) {
        return false;
    }
    std::lock_guard guard(dlMutex);
    if (alreadyHdBuffersLoaded) {
        return AHardwareBuffer_allocate_compat != nullptr
               && AHardwareBuffer_isSupported_compat != nullptr
               && AHardwareBuffer_unlock_compat != nullptr
               && AHardwareBuffer_release_compat != nullptr
               && AHardwareBuffer_lock_compat != nullptr
               && AHardwareBuffer_toHardwareBuffer_compat != nullptr
               && AHardwareBuffer_describe_compat != nullptr;
    }
    alreadyHdBuffersLoaded = true;
    void *hhl = dlopen("libandroid.so", RTLD_NOW);
    if (!hhl) {
        return false;
    }

    AHardwareBuffer_allocate_compat = (AHardwareBufferAllocateFunc) dlsym(hhl,
                                                                          "AHardwareBuffer_allocate");
    if (AHardwareBuffer_allocate_compat == nullptr) {
        return false;
    }

    AHardwareBuffer_isSupported_compat = (AHardwareBufferIsSupportedFunc) dlsym(hhl,
                                                                                "AHardwareBuffer_isSupported");
    if (AHardwareBuffer_isSupported_compat == nullptr) {
        return false;
    }

    AHardwareBuffer_unlock_compat = (AHardwareBufferUnlockFunc) dlsym(hhl,
                                                                      "AHardwareBuffer_unlock");
    if (AHardwareBuffer_unlock_compat == nullptr) {
        return false;
    }

    AHardwareBuffer_release_compat = (AHardwareBufferReleaseFunc) dlsym(hhl,
                                                                        "AHardwareBuffer_release");
    if (AHardwareBuffer_release_compat == nullptr) {
        return false;
    }

    AHardwareBuffer_lock_compat = (AHardwareBufferLockFunc) dlsym(hhl,
                                                                  "AHardwareBuffer_lock");
    if (AHardwareBuffer_lock_compat == nullptr) {
        return false;
    }

    AHardwareBuffer_toHardwareBuffer_compat = (AHardwareBufferToHardwareBufferFunc) dlsym(hhl,
                                                                                          "AHardwareBuffer_toHardwareBuffer");
    if (AHardwareBuffer_toHardwareBuffer_compat == nullptr) {
        return false;
    }

    AHardwareBuffer_describe_compat = (AHardwareBufferDescribeFunc) dlsym(hhl,
                                                                          "AHardwareBuffer_describe");
    if (AHardwareBuffer_describe_compat == nullptr) {
        return false;
    }

    return true;
}