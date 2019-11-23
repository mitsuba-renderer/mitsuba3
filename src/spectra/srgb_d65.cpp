#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class SRGBEmitterSpectrum final : public Texture<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(SRGBEmitterSpectrum, Texture)
    MTS_IMPORT_BASE(Texture)
    MTS_IMPORT_TYPES(Texture)

    SRGBEmitterSpectrum(const Properties &props) {
        if constexpr (is_rgb_v<Spectrum>)
            Throw("SRGBEmitterSpectrum being used in RGB mode. An SRGBSpectrum should "
                  " most likely be used instead");
        if (props.has_property("scale") && props.has_property("value"))
            Throw("Cannot specify both 'scale' and 'value'.");

        ScalarColor3f color = props.color("color");
        ScalarFloat intensity = hmax(color) * 2.f;
        color /= intensity;

        m_coeff = srgb_model_fetch(color);

        Properties props2("d65");
        ScalarFloat value =
            props.float_(props.has_property("scale") ? "scale" : "value", 1.f);
        props2.set_float("value", value * intensity);
        PluginManager *pmgr = PluginManager::instance();
        m_d65 = (Texture *) pmgr->create_object<Texture>(props2)->expand().at(0).get();
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        return m_d65->eval(si, active) *
               srgb_model_eval<UnpolarizedSpectrum>(m_coeff, si.wavelengths);
    }

#if defined(MTS_ENABLE_AUTODIFF)
    void put_parameters(DifferentiableParameters &dp) override {
        dp.put(this, "coeff", m_coeff);
    }
#endif

private:
    Vector3f m_coeff;
    ref<Texture> m_d65;
};

MTS_EXPORT_PLUGIN(SRGBEmitterSpectrum, "sRGB x D65 spectrum")
NAMESPACE_END(mitsuba)
