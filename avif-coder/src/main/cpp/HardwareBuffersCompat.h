//
// Created by Radzivon Bartoshyk on 16/09/2023.
//

#ifndef AVIF_HARDWAREBUFFERSCOMPAT_H
#define AVIF_HARDWAREBUFFERSCOMPAT_H

#include <android/bitmap.h>
#include <android/hardware_buffer.h>
#include <android/hardware_buffer_jni.h>

bool loadAHardwareBuffersAPI();

typedef int (*AHardwareBufferAllocateFunc)(
        const AHardwareBuffer_Desc *_Nonnull desc, AHardwareBuffer *_Nullable *_Nonnull outBuffer
);

typedef int (*AHardwareBufferIsSupportedFunc)(
        const AHardwareBuffer_Desc *_Nonnull desc
);

typedef int (*AHardwareBufferReleaseFunc)(
        AHardwareBuffer *_Nonnull buffer
);

typedef int (*AHardwareBufferUnlockFunc)(
        AHardwareBuffer *_Nonnull buffer, int32_t *_Nullable fence
);

typedef int (*AHardwareBufferLockFunc)(
        AHardwareBuffer *_Nonnull buffer, uint64_t usage, int32_t fence,
        const ARect *_Nullable rect, void *_Nullable *_Nonnull outVirtualAddress
);

typedef void (*AHardwareBufferDescribeFunc)(
        const AHardwareBuffer *_Nonnull buffer, AHardwareBuffer_Desc *_Nonnull outDesc
);

typedef jobject (*AHardwareBufferToHardwareBufferFunc)(
        JNIEnv *env, AHardwareBuffer *hardwareBuffer
);

extern AHardwareBufferAllocateFunc AHardwareBuffer_allocate_compat;
extern AHardwareBufferIsSupportedFunc AHardwareBuffer_isSupported_compat;
extern AHardwareBufferUnlockFunc AHardwareBuffer_unlock_compat;
extern AHardwareBufferReleaseFunc AHardwareBuffer_release_compat;
extern AHardwareBufferLockFunc AHardwareBuffer_lock_compat;
extern AHardwareBufferToHardwareBufferFunc AHardwareBuffer_toHardwareBuffer_compat;
extern AHardwareBufferDescribeFunc AHardwareBuffer_describe_compat;

#endif //AVIF_HARDWAREBUFFERSCOMPAT_H
