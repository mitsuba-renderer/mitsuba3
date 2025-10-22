#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _spectrum-uniform:

Uniform spectrum (:monosp:`uniform`)
------------------------------------

.. pluginparameters::

 * - wavelength_min
   - |float|
   - Lower bound of the wavelength sampling range in nanometers. Default: 360 nm

 * - wavelength_max
   - |float|
   - Upper bound of the wavelength sampling range in nanometers. Default: 830 nm

 * - value
   - |float|
   - Value of the spectral function across the specified spectral range.
   - |exposed|, |differentiable|

This spectrum returns a constant reflectance or emission value over the spectral
dimension. It implements a uniform sampling method on a finite spectral range
controlled by the ``wavelength_min`` and ``wavelength_max`` parameters.

.. tabs::
    .. code-tab:: xml
        :name: uniform

        <spectrum type="uniform">
            <float name="value" value="0.1"/>
        </spectrum>

    .. code-tab:: python

        'type': 'uniform',
        'value': 0.1

 */

template <typename Float, typename Spectrum>
class UniformSpectrum final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    UniformSpectrum(const Properties &props) : Texture(props) {
        m_value = dr::opaque<Float>(props.get<ScalarFloat>("value"));
        m_range = ScalarVector2f(props.get<ScalarFloat>("wavelength_min", MI_CIE_MIN),
                                 props.get<ScalarFloat>("wavelength_max", MI_CIE_MAX));
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("value", m_value, ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        if constexpr (dr::is_jit_v<Float>)
            if (unlikely(m_value.size() != 1))
                Throw("Updated the uniform spectrum with a float of size %d", m_value.size());
        dr::make_opaque(m_value);
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f & /*si*/,
                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>)
            return UnpolarizedSpectrum(m_value);
        else
            return m_value;
    }

    Float eval_1(const SurfaceInteraction3f & /*it*/, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return m_value;
    }

    Color3f eval_3(const SurfaceInteraction3f & /*it*/, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return Color3f(m_value);
    }

    Vector2f eval_1_grad(const SurfaceInteraction3f & /*it*/, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return 0.0;
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
            DRJIT_MARK_USED(sample);
            return { dr::empty<Wavelength>(), m_value };
        }
    }

    Float mean() const override { return m_value; }

    ScalarVector2f wavelength_range() const override {
        return m_range;
    }

    ScalarFloat spectral_resolution() const override {
        return 0.f;
    }

    ScalarFloat max() const override {
        return dr::slice(dr::max(m_value));
    }

    std::string to_string() const override {
        return tfm::format("UniformSpectrum[value=%f]", m_value);
    }

    MI_DECLARE_CLASS(UniformSpectrum)
private:
    Float m_value;
    ScalarVector2f m_range;

    MI_TRAVERSE_CB(Texture, m_value)
};

MI_EXPORT_PLUGIN(UniformSpectrum)
NAMESPACE_END(mitsuba)
