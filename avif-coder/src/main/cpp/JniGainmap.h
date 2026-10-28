#pragma once
#include <jni.h>
#include "ImageFrame.h"

// Attach only on OS versions that expose android.graphics.Gainmap.
void attachGainmap(JNIEnv *env, jobject bitmap, const AvifImageFrame &frame);
