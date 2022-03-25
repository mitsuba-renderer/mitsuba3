#include <mitsuba/render/volume.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name Volume implementations
// =======================================================================

MI_VARIANT Volume<Float, Spectrum>::Volume(const Properties &props) {
    m_to_local = props.get<ScalarTransform4f>("to_world", ScalarTransform4f()).inverse();
    m_channel_count = 0;
    update_bbox();
}

MI_VARIANT typename Volume<Float, Spectrum>::UnpolarizedSpectrum
Volume<Float, Spectrum>::eval(const Interaction3f &, Mask) const {
    NotImplementedError("eval");
}

MI_VARIANT Float Volume<Float, Spectrum>::eval_1(const Interaction3f &, Mask) const {
    NotImplementedError("eval_1");
}

MI_VARIANT typename Volume<Float, Spectrum>::Vector3f
Volume<Float, Spectrum>::eval_3(const Interaction3f &, Mask) const {
    NotImplementedError("eval_3");
}

MI_VARIANT dr::Array<Float, 6>
Volume<Float, Spectrum>::eval_6(const Interaction3f &, Mask) const {
    NotImplementedError("eval_6");
}

MI_VARIANT void
Volume<Float, Spectrum>::eval_n(const Interaction3f & /*it*/,
                                Float * /*out*/,
                                Mask /*active*/) const {
    NotImplementedError("eval_n");
}

MI_VARIANT std::pair<typename Volume<Float, Spectrum>::UnpolarizedSpectrum,
                      typename Volume<Float, Spectrum>::Vector3f>
Volume<Float, Spectrum>::eval_gradient(const Interaction3f & /*it*/, Mask /*active*/) const {
    NotImplementedError("eval_gradient");
}

MI_VARIANT typename Volume<Float, Spectrum>::ScalarFloat
Volume<Float, Spectrum>::max() const { NotImplementedError("max"); }

MI_VARIANT void
Volume<Float, Spectrum>::max_per_channel(ScalarFloat * /*out*/) const {
    NotImplementedError("max_per_channel");
}

MI_VARIANT typename Volume<Float, Spectrum>::ScalarVector3i
Volume<Float, Spectrum>::resolution() const {
    return ScalarVector3i(1, 1, 1);
}

//! @}
// =======================================================================

MI_IMPLEMENT_CLASS_VARIANT(Volume, Object, "volume")

MI_INSTANTIATE_CLASS(Volume)
NAMESPACE_END(mitsuba)
