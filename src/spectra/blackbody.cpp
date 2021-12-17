#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _spectrum-blackbody:

sRGB D65 spectrum (:monosp:`blackbody`)
---------------------------------------

This is a black body radiation spectrum for a specified temperature
And therefore takes a single :monosp:`float`-valued parameter :paramtype:`temperature` (in Kelvins).

This is the only spectrum type that needs to be explicitly instantiated in its full XML description:

.. code-block:: xml

    <shape type=".. shape type ..">
        <emitter type="area">
            <spectrum type="blackbody" name="radiance">
                <float name="temperature" value="5000"/>
            </spectrum>
        </emitter>
    </shape>

This spectrum type only makes sense for specifying emission and is unavailable
in non-spectral rendering modes.

Note that attaching a black body spectrum to the intensity property of a emitter introduces
physical units into the rendering process of Mitsuba 2, which is ordinarily a unitless system.
Specifically, the black body spectrum has units of power (:math:`W`) per unit area (:math:`m^{-2}`)
per steradian (:math:`sr^{-1}`) per unit wavelength (:math:`nm^{-1}`). As a consequence,
your scene should be modeled in meters for this plugin to work properly.

 */

template <typename Float, typename Spectrum>
class BlackBodySpectrum final : public Texture<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Texture)

    // A few natural constants
    constexpr static ScalarFloat c = ScalarFloat(2.99792458e+8);   /// Speed of light
    constexpr static ScalarFloat h = ScalarFloat(6.62607004e-34);  /// Planck constant
    constexpr static ScalarFloat k = ScalarFloat(1.38064852e-23);  /// Boltzmann constant

    /// First and second radiation static constants
    constexpr static ScalarFloat c0 = 2 * h * c * c;
    constexpr static ScalarFloat c1 = h * c / k;

    BlackBodySpectrum(const Properties &props) : Texture(props) {
        m_temperature = props.get<ScalarFloat>("temperature");
        m_wavelength_range = ScalarVector2f(
            props.get<ScalarFloat>("lambda_min", MTS_CIE_MIN),
            props.get<ScalarFloat>("lambda_max", MTS_CIE_MAX)
        );
        parameters_changed();
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        m_integral_min = cdf_and_pdf(ScalarFloat(m_wavelength_range.x())).first;
        m_integral = cdf_and_pdf(ScalarFloat(m_wavelength_range.y())).first - m_integral_min;
    }

    UnpolarizedSpectrum eval_impl(const Wavelength &wavelengths, Mask active_) const {
        if constexpr (is_spectral_v<Spectrum>) {
            /* The scale factors of 1e-9f are needed to perform a conversion between
               densities per unit nanometer and per unit meter. */
            Wavelength lambda  = wavelengths * 1e-9f,
                       lambda2 = ek::sqr(lambda),
                       lambda5 = ek::sqr(lambda2) * lambda;

            ek::mask_t<Wavelength> active = active_;
            active &= wavelengths >= m_wavelength_range.x()
                   && wavelengths <= m_wavelength_range.y();

            /* Watts per unit surface area (m^-2)
                     per unit wavelength (nm^-1)
                     per unit steradian (sr^-1) */
            UnpolarizedSpectrum P = 1e-9f * c0 / (lambda5 *
                    (ek::exp(c1 / (lambda * m_temperature)) - 1.f));

            return P & active;
        } else {
            ENOKI_MARK_USED(wavelengths);
            ENOKI_MARK_USED(active_);
            /// TODO : implement reasonable thing to do in mono/RGB mode
            Throw("Not implemented for non-spectral modes");
        }
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return eval_impl(si.wavelengths, active);
    }

    Wavelength pdf_spectrum(const SurfaceInteraction3f &si, Mask active_) const override {
        if constexpr (is_spectral_v<Spectrum>) {
            Wavelength lambda  = si.wavelengths * 1e-9f,
                       lambda2 = ek::sqr(lambda),
                       lambda5 = ek::sqr(lambda2) * lambda;

            ek::mask_t<Wavelength> active = active_;
            active &= si.wavelengths >= m_wavelength_range.x()
                   && si.wavelengths <= m_wavelength_range.y();

            // Wien's approximation to Planck's law
            Wavelength pdf = 1e-9f * c0 * ek::exp(-c1 / (lambda * m_temperature))
                / (lambda5 * m_integral);

            return pdf & active;
        } else {
            ENOKI_MARK_USED(active_);
            /// TODO : implement reasonable thing to do in mono/RGB mode
            Throw("Not implemented for non-spectral modes");
        }
    }

    template <typename Value>
    std::pair<Value, Value> cdf_and_pdf(Value lambda) const {
        Value c1_2 = ek::sqr(c1),
              c1_3 = c1_2 * c1,
              c1_4 = ek::sqr(c1_2);

        const Value K  = m_temperature,
                    K2 = ek::sqr(K),
                    K3 = K2*K;

        lambda *= 1e-9f;

        Value lambda2 = ek::sqr(lambda),
              lambda3 = lambda2 * lambda,
              lambda5 = lambda2 * lambda3;

        Value expval = ek::exp(-c1 / (K * lambda));

        Value cdf = c0 * K * expval *
                (c1_3 + 3 * c1_2 * K * lambda + 6 * c1 * K2 * lambda2 +
                 6 * K3 * lambda3) / (c1_4 * lambda3);

        Value pdf = 1e-9f * c0 * expval / lambda5;

        return { cdf, pdf };
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f & /* si */,
                    const Wavelength &sample_, Mask active_) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureSample, active_);

        using WavelengthMask = ek::mask_t<Wavelength>;

        if constexpr (is_spectral_v<Spectrum>) {
            WavelengthMask active = active_;

            Wavelength sample = ek::fmadd(sample_, Wavelength(m_integral), Wavelength(m_integral_min));

            const ScalarFloat eps        = 1e-5f,
                              eps_domain = eps * (m_wavelength_range.y() - m_wavelength_range.x()),
                              eps_value  = eps * m_integral;

            Wavelength a = m_wavelength_range.x(),
                       b = m_wavelength_range.y(),
                       t = .5f * (m_wavelength_range.x() + m_wavelength_range.y()),
                       value, deriv;

            do {
                // Fall back to a bisection step when t is out of bounds
                WavelengthMask bisect_mask = !((t > a) && (t < b));
                ek::masked(t, bisect_mask && active) = .5f * (a + b);

                // Evaluate the definite integral and its derivative (i.e. the spline)
                std::tie(value, deriv) = cdf_and_pdf(t);
                value -= sample;

                // Update which lanes are still active
                active = active && (ek::abs(value) > eps_value) && (b - a > eps_domain);

                // Stop the iteration if converged
                if (ek::none_nested(active))
                    break;

                // Update the bisection bounds
                WavelengthMask update_mask = value <= 0.f;
                ek::masked(a,  update_mask) = t;
                ek::masked(b, !update_mask) = t;

                // Perform a Newton step
                ek::masked(t, active) = t - value / deriv;
            } while (true);

            Wavelength pdf = deriv / m_integral;

            return { t, eval_impl(t, active_) / pdf };
        } else {
            ENOKI_MARK_USED(sample_);
            Throw("Not implemented for non-spectral modes");
        }
    }

    Float mean() const override {
        return m_integral / (m_wavelength_range.y() - m_wavelength_range.x());
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("temperature", m_temperature);
    }

    MTS_DECLARE_CLASS()
private:
    ScalarFloat m_temperature;
    ScalarFloat m_integral_min;
    ScalarFloat m_integral;
    ScalarVector2f m_wavelength_range;
};

MTS_IMPLEMENT_CLASS_VARIANT(BlackBodySpectrum, Texture)
MTS_EXPORT_PLUGIN(BlackBodySpectrum, "Black body spectrum")
NAMESPACE_END(mitsuba)
