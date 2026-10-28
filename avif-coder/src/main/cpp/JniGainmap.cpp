#include "JniGainmap.h"
#include "JniBitmap.h"
#include "Support.h"
#include "JniException.h"
#include "avifweaver.h"

void attachGainmap(JNIEnv *env, jobject bitmap, const AvifImageFrame &frame) {
  if (!bitmap || env->ExceptionCheck() || androidOSVersion() < 34 || !frame.gainMap) return;
  const auto &gain = *frame.gainMap;
  auto bridge = env->FindClass("com/radzivon/bartoshyk/avif/coder/GainmapSupport");
  if (!bridge) return;
  auto attach = env->GetStaticMethodID(bridge,
                                       "attach",
                                       "(Landroid/graphics/Bitmap;Landroid/graphics/Bitmap;[F)V");
  auto color = env->GetStaticMethodID(bridge, "setColorSpace", "(Landroid/graphics/Bitmap;II)V");
  if (!attach || !color) {
    env->DeleteLocalRef(bridge);
    return;
  }
  // createBitmap takes mutable storage for its existing conversion interface.
  auto pixels = gain.store;
  const bool highBitDepth = gain.bitDepth > 8;
  std::string config = highBitDepth ? "RGBA_F16" : "ARGB_8888";
  const uint32_t stride = gain.width * (highBitDepth ? 8 : 4);
  if (highBitDepth) {
    weave_cvt_rgba16_to_rgba_f16(reinterpret_cast<const uint16_t *>(pixels.data()),
                                 stride,
                                 gain.bitDepth,
                                 reinterpret_cast<uint16_t *>(pixels.data()),
                                 stride,
                                 gain.width,
                                 gain.height);
  }
  auto contents =
      createBitmap(env, pixels, config, stride, gain.width, gain.height, highBitDepth, nullptr);
  if (contents && !env->ExceptionCheck()) {
    auto metadata = env->NewFloatArray(19);
    if (metadata) {
      env->SetFloatArrayRegion(metadata, 0, 19, gain.metadata.data());
      env->CallStaticVoidMethod(bridge,
                                color,
                                bitmap,
                                frame.colorPrimaries,
                                frame.transferCharacteristics);
      if (!env->ExceptionCheck())
        env->CallStaticVoidMethod(bridge,
                                  attach,
                                  bitmap,
                                  contents,
                                  metadata);
      env->DeleteLocalRef(metadata);
    }
    env->DeleteLocalRef(contents);
  }
  env->DeleteLocalRef(bridge);
}
