use jni::objects::{JObject, JValue};
use jni::strings::JNIString;
use jni::{Env, jni_sig, jni_str};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum NativeColorSpace {
    Pq2100,
    Hlg2100,
    DisplayP3,
    DciP3,
    LinearSrgb,
    Bt709,
    DefaultSrgb,
}

impl NativeColorSpace {
    fn as_named_str(self) -> &'static str {
        match self {
            Self::Pq2100 => "BT2020_PQ",
            Self::Hlg2100 => "BT2020_HLG",
            Self::DisplayP3 => "DISPLAY_P3",
            Self::DciP3 => "DCI_P3",
            Self::LinearSrgb => "LINEAR_SRGB",
            Self::Bt709 => "BT709",
            Self::DefaultSrgb => "SRGB",
        }
    }
}
fn search_for_named<'local>(
    env: &mut Env<'local>,
    named: &str,
) -> Result<JObject<'local>, jni::errors::Error> {
    let cs_cls = env.find_class(jni_str!("android/graphics/ColorSpace"))?;
    let cs_named_cls = env.find_class(jni_str!("android/graphics/ColorSpace$Named"))?;

    let named_enum = env
        .get_static_field(
            &cs_named_cls,
            JNIString::from(named),
            jni_sig!("Landroid/graphics/ColorSpace$Named;"),
        )?
        .l()?;

    env.call_static_method(
        &cs_cls,
        JNIString::from("get"),
        jni_sig!("(Landroid/graphics/ColorSpace$Named;)Landroid/graphics/ColorSpace;"),
        &[JValue::Object(&named_enum)],
    )?
    .l()
}

pub fn get_jni_color_space<'local>(
    env: &mut Env<'local>,
    color_space: NativeColorSpace,
) -> Result<JObject<'local>, jni::errors::Error> {
    search_for_named(env, color_space.as_named_str())
}
