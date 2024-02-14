#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _spectrum-blackbody:

Blackbody spectrum (:monosp:`blackbody`)
----------------------------------------

.. pluginparameters::

 * - wavelength_min
   - |float|
   - Minimum wavelength of the spectral range in nanometers. (Default: 360nm)

 * - wavelength_max
   - |float|
   - Maximum wavelength of the spectral range in nanometers. (Default: 830nm)

 * - temperature
   - |float|
   - Black body temperature in Kelvins.
   - |exposed|

This is a black body radiation spectrum for a specified temperature
And therefore takes a single :monosp:`float`-valued parameter :paramtype:`temperature` (in Kelvins).

This is the only spectrum type that needs to be explicitly instantiated in its full XML description:

.. tabs::
    .. code-tab:: xml
        :name: blackbody

        <shape type=".. shape type ..">
            <emitter type="area">
                <spectrum type="blackbody" name="radiance">
                    <float name="temperature" value="5000"/>
                </spectrum>
            </emitter>
        </shape>

    .. code-tab:: python

        'type': '.. shape type ..',
        'emitter': {
            'type': 'area',
            'radiance': {
                'type': 'blackbody',
                'temperature': 5000
            }
        }

This spectrum type only makes sense for specifying emission and is unavailable
in non-spectral rendering modes.

Note that attaching a black body spectrum to the intensity property of a emitter introduces
physical units into the rendering process of Mitsuba 3, which is ordinarily a unitless system.
Specifically, the black body spectrum has units of power (:math:`W`) per unit area (:math:`m^{-2}`)
per steradian (:math:`sr^{-1}`) per unit wavelength (:math:`nm^{-1}`). As a consequence,
your scene should be modeled in meters for this plugin to work properly.

 */

template <typename Float, typename Spectrum>
class BlackBodySpectrum final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    // A few natural constants
    constexpr static ScalarFloat c = ScalarFloat(2.99792458e+8);   /// Speed of light
    constexpr static ScalarFloat h = ScalarFloat(6.62607004e-34);  /// Planck constant
    constexpr static ScalarFloat k = ScalarFloat(1.38064852e-23);  /// Boltzmann constant
    constexpr static ScalarFloat b = ScalarFloat(2.89777196e-3);   /// Wien displacement constant

    /// First and second radiation static constants
    constexpr static ScalarFloat c0 = 2 * h * c * c;
    constexpr static ScalarFloat c1 = h * c / k;

    BlackBodySpectrum(const Properties &props) : Texture(props) {
        m_temperature = props.get<ScalarFloat>("temperature");
        m_wavelength_range = ScalarVector2f(
            props.get<ScalarFloat>("wavelength_min", MI_CIE_MIN),
            props.get<ScalarFloat>("wavelength_max", MI_CIE_MAX)
        );
        parameters_changed();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("temperature", m_temperature, +ParamFlags::NonDifferentiable);
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
                       lambda2 = dr::square(lambda),
                       lambda5 = dr::square(lambda2) * lambda;

            dr::mask_t<Wavelength> active = active_;
            active &= wavelengths >= m_wavelength_range.x()
                   && wavelengths <= m_wavelength_range.y();

            /* Watts per unit surface area (m^-2)
                     per unit wavelength (nm^-1)
                     per unit steradian (sr^-1) */
            UnpolarizedSpectrum P = 1e-9f * c0 / (lambda5 *
                    (dr::exp(c1 / (lambda * m_temperature)) - 1.f));

            return P & active;
        } else {
            DRJIT_MARK_USED(wavelengths);
            DRJIT_MARK_USED(active_);
            /// TODO : implement reasonable thing to do in mono/RGB mode
            Throw("Not implemented for non-spectral modes");
        }
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return eval_impl(si.wavelengths, active);
    }

    Wavelength pdf_spectrum(const SurfaceInteraction3f &si, Mask active_) const override {
        if constexpr (is_spectral_v<Spectrum>) {
            Wavelength lambda  = si.wavelengths * 1e-9f,
                       lambda2 = dr::square(lambda),
                       lambda5 = dr::square(lambda2) * lambda;

            dr::mask_t<Wavelength> active = active_;
            active &= si.wavelengths >= m_wavelength_range.x()
                   && si.wavelengths <= m_wavelength_range.y();

            // Wien's approximation to Planck's law
            Wavelength pdf = 1e-9f * c0 * dr::exp(-c1 / (lambda * m_temperature))
                / (lambda5 * m_integral);

            return pdf & active;
        } else {
            DRJIT_MARK_USED(active_);
            /// TODO : implement reasonable thing to do in mono/RGB mode
            Throw("Not implemented for non-spectral modes");
        }
    }

    template <typename Value>
    std::pair<Value, Value> cdf_and_pdf(Value lambda) const {
        Value c1_2 = dr::square(c1),
              c1_3 = c1_2 * c1,
              c1_4 = dr::square(c1_2);

        const Value K  = m_temperature,
                    K2 = dr::square(K),
                    K3 = K2*K;

        lambda *= 1e-9f;

        Value lambda2 = dr::square(lambda),
              lambda3 = lambda2 * lambda,
              lambda5 = lambda2 * lambda3;

        Value expval = dr::exp(-c1 / (K * lambda));

        Value cdf = c0 * K * expval *
                (c1_3 + 3 * c1_2 * K * lambda + 6 * c1 * K2 * lambda2 +
                 6 * K3 * lambda3) / (c1_4 * lambda3);

        Value pdf = 1e-9f * c0 * expval / lambda5;

        return { cdf, pdf };
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f & /* si */,
                    const Wavelength &sample_, Mask active_) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureSample, active_);

        using WavelengthMask = dr::mask_t<Wavelength>;

        if constexpr (is_spectral_v<Spectrum>) {
            WavelengthMask active = active_;

            Wavelength sample = dr::fmadd(sample_, Wavelength(m_integral), Wavelength(m_integral_min));

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
                dr::masked(t, bisect_mask && active) = .5f * (a + b);

                // Evaluate the definite integral and its derivative (i.e. the spline)
                std::tie(value, deriv) = cdf_and_pdf(t);
                value -= sample;

                // Update which lanes are still active
                active = active && (dr::abs(value) > eps_value) && (b - a > eps_domain);

                // Stop the iteration if converged
                if (dr::none_nested(active))
                    break;

                // Update the bisection bounds
                WavelengthMask update_mask = value <= 0.f;
                dr::masked(a,  update_mask) = t;
                dr::masked(b, !update_mask) = t;

                // Perform a Newton step
                dr::masked(t, active) = t - value / deriv;
            } while (true);

            Wavelength pdf = deriv / m_integral;

            return { t, eval_impl(t, active_) / pdf };
        } else {
            DRJIT_MARK_USED(sample_);
            Throw("Not implemented for non-spectral modes");
        }
    }

    Float mean() const override {
        return m_integral / (m_wavelength_range.y() - m_wavelength_range.x());
    }

    ScalarVector2f wavelength_range() const override {
        return m_wavelength_range;
    }

    ScalarFloat spectral_resolution() const override {
        return 0.f;
    }

    ScalarFloat max() const override {
        ScalarFloat lambda_peak = dr::clip(b / m_temperature, 
                                            m_wavelength_range.x() * 1e-9f, 
                                            m_wavelength_range.y() * 1e-9f),
                    lambda2_peak = dr::square(lambda_peak),
                    lambda5_peak = dr::square(lambda2_peak) * lambda_peak;

        ScalarFloat P = 1e-9f * c0 / (lambda5_peak *
                    (dr::exp(c1 / (lambda_peak * m_temperature)) - 1.f));

        return P;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "BlackBodySpectrum[" << std::endl
            << "  temperature = " << string::indent(m_temperature) << std::endl
            << "]";
        return oss.str();
    }


    MI_DECLARE_CLASS()
private:
    ScalarFloat m_temperature;
    ScalarFloat m_integral_min;
    ScalarFloat m_integral;
    ScalarVector2f m_wavelength_range;
};

MI_IMPLEMENT_CLASS_VARIANT(BlackBodySpectrum, Texture)
MI_EXPORT_PLUGIN(BlackBodySpectrum, "Black body spectrum")
NAMESPACE_END(mitsuba)
