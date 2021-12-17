#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _spectrum-uniform:

Uniform spectrum (:monosp:`uniform`)
------------------------------------

This spectrum returns a constant reflectance or emission value between 360 and 830nm.

 */

template <typename Float, typename Spectrum>
class UniformSpectrum final : public Texture<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Texture)

    UniformSpectrum(const Properties &props) : Texture(props) {
        m_value = ek::opaque<Float>(props.get<ScalarFloat>("value"));
        m_range = ScalarVector2f(props.get<ScalarFloat>("wavelength_min", MTS_CIE_MIN),
                                 props.get<ScalarFloat>("wavelength_max", MTS_CIE_MAX));
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f & /*si*/,
                             Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>)
            return UnpolarizedSpectrum(m_value);
        else
            return m_value;
    }

    Float eval_1(const SurfaceInteraction3f & /*it*/, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return m_value;
    }

    Wavelength pdf_spectrum(const SurfaceInteraction3f & /*si*/, Mask /*active*/) const override {
        NotImplementedError("pdf");
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f & /*si*/,
                    const Wavelength & sample, Mask /*active*/) const override {
        if constexpr (is_spectral_v<Spectrum>) {
            return { m_range.x() +
                         (m_range.y() - m_range.x()) * sample,
                     m_value * (m_range.y() - m_range.x()) };
        } else {
            ENOKI_MARK_USED(sample);
            return { ek::empty<Wavelength>(), m_value };
        }
    }

    Float mean() const override { return m_value; }

    ScalarVector2f wavelength_range() const override {
        return m_range;
    }

    ScalarFloat spectral_resolution() const override {
        return 0.f;
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        ek::make_opaque(m_value);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("value", m_value);
    }

    std::string to_string() const override {
        return tfm::format("UniformSpectrum[value=%f]", m_value);
    }

    MTS_DECLARE_CLASS()
private:
    Float m_value;
    ScalarVector2f m_range;
};

MTS_IMPLEMENT_CLASS_VARIANT(UniformSpectrum, Texture)
MTS_EXPORT_PLUGIN(UniformSpectrum, "Uniform spectrum")
NAMESPACE_END(mitsuba)
