#include <mitsuba/core/distr_1d.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/random.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/eradiate/oceanprops.h>
#include <drjit/dynamic.h>
#include <drjit/texture.h>

// Transmittance angular resolution
// i.e. texture resolution
#define MI_OCEAN_TRANSMITTANCE_RES 64

NAMESPACE_BEGIN(mitsuba)

/**!

.. _plugin-bsdf-ocean_legacy:

(Legacy 6S) Oceanic reflection model (:monosp:`ocean_legacy`)
-------------------------------------------------------------

.. pluginparameters::
 :extra-rows: 7

 * - wavelength
   - |float|
   - Specifies the wavelength at which to evaluate the oceanic reflectance.
     Range: [200, 4000] nm
   - |exposed|

 * - wind_speed
   - |spectrum| or |texture|
   - Specifies the wind speed at which to evaluate the oceanic
     reflectance. Range: [0, 37.54] m/s. Default: 0.1 m/s
   - |exposed| |differentiable|

 * - wind_direction
   - |float|
   - Specifies the wind direction at which to evaluate the oceanic reflectance
     in North Left convention. Range: [0, 360]. Default: 0.0°
   - |exposed| |differentiable|

 * - chlorinity
   - |float|
   - Specifies the chlorinity of the water at which to evaluate the oceanic
     reflectance. Default: 19.0 g/kg
   - |exposed| |differentiable|

 * - pigmentation
   - |float|
   - Specifies the pigmentation of the water at which to evaluate the oceanic
     reflectance. Range: [0.3, ∞[. Default: 0.3 mg/m^3
   - |exposed| |differentiable|

 * - shadowing
   - |bool|
   - Indicates whether evaluation accounts for the shadowing-masking term.
     Disabling it mirrors 6SV.
     (Default: :monosp:`true`).
   - |exposed|

 * - component
   - |int|
   - **Debug:** specifies which component of the oceanic reflection model to
     evaluate. Default: 0.

     **Component 0:** Used to evaluate the total oceanic reflectance.

     **Component 1:** Evaluates the whitecap reflectance.

     **Component 2:** Evaluates the sun glint reflectance.

     **Component 3:** Evaluates the underlight reflectance.

     **Component 4:** Evaluates the whitecap and underlight reflectance together.

   - |exposed|

 * - coverage
   - |float|
   - Fraction of the surface occupied by whitecaps. Modifying this parameter has
     no effect: it is automatically computed from the wind speed.
   - |exposed|

This plugin implements the oceanic reflection model originally
implemented in the 6S radiative transfer model. Note that this model
is monochromatic.

For the fundamental formulae defining the oceanic reflectance model, please
refer to the Eradiate Scientific Handbook.

Note that this material is one-sided---that is, observed from the
back side, it will be completely black. If this is undesirable,
consider using the ``twosided`` BSDF adapter plugin.
The following snippet describes an oceanic surface material with monochromatic
parameters:

.. warning::
    The wind direction is given in degrees and follows the North Left convention
    as in 6SV.

.. tab-set-code::

    .. code-block:: python

        "type": "ocean_legacy",
        "wavelength": 550,
        "wind_speed": 10,
        "wind_direction": 0,
        "chlorinity": 19,
        "pigmentation": 0.3,
        "shadowing": True,
        "component": 0,

    .. code-block:: xml

        <bsdf type="ocean_legacy">
            <float name="wavelength" value="550"/>
            <float name="wind_speed" value="10"/>
            <float name="wind_direction" value="0"/>
            <float name="chlorinity" value="19"/>
            <float name="pigmentation" value="0.3"/>
            <float name="shadowing" value="True"/>
            <int name="component" value="0"/>
        </bsdf>
*/

/**
 * @brief Evaluate the transmittance of the radiance over all
 * provided angles.
 *
 * For each (theta, phi) entry, estimates the directional-hemispherical
 * reflectance of the interface via importance sampling (mirroring
 * eval_reflectance() in microfacet.h): a Beckmann distribution matched to
 * the Cox-Munk mean-square slopes is used as the sampling proposal,
 * reweighted per sample by the Gram-Charlier coefficient. Depending on
 * ``shadowing`` equals true or false, sample and evaluate using the VNDF or NDF
 * respectively. The latter is closer to the base implementation in 6S but has
 * values that blow up at grazing incidence.
 * The transmittance is then 1 - reflectance.
 *
 * @param theta Incident zenith angle.
 * @param phi Incident azimuth angle, relative to the wind direction.
 * @param n_real Real part of the index of refraction.
 * @param n_imag Imaginary part of the index of refraction.
 * @param wind_speed Speed of wind at mast height [m/s]
 * @param upwelling Flag for computing upwelling transmittance.
 * Will compute theta according to snells law and invert the
 * index of refraction.
 * @param shadowing Whether to include shadowing-masking (Smith separable
 * G1(wo,m)) in the reflectance estimate.
 */
template <typename Float, typename Spectrum>
Float eval_ocean_transmittance(Float theta, Float phi,
                               dr::scalar_t<Float> n_real,
                               dr::scalar_t<Float> n_imag,
                               dr::scalar_t<Float> wind_speed, bool upwelling,
                               bool shadowing) {
    MI_IMPORT_CORE_TYPES()

    using FloatP      = dr::Packet<dr::scalar_t<Float>>;
    using Vector3fP   = Vector<FloatP, 3>;
    using Normal3fP   = Normal<FloatP, 3>;
    using MicrofacetDistributionP = MicrofacetDistribution<FloatP, Spectrum>;

    // number of importance samples (low-discrepancy grid over the [0,1]^2
    // VNDF sample space)
    int res = 16;

    if (upwelling){
        theta = dr::asin(dr::sin(theta) / n_real);
        n_real = 1.f / n_real;
        n_imag = 0.f;
    }

    using FloatX          = dr::DynamicArray<dr::scalar_t<Float>>;
    auto [nodes, weights] = quad::gauss_legendre<FloatX>(res);
    Float result          = dr::zeros<Float>(dr::width(theta));

    auto [nodes_x, nodes_y]     = dr::meshgrid(nodes, nodes);
    auto [weights_x, weights_y] = dr::meshgrid(weights, weights);
    // Map nodes from [-1,1] to the [0,1]^2 sample space,
    // weights rescaled to the 0.25 Jacobian factor.
    nodes_x = dr::fmadd(nodes_x, 0.5f, 0.5f);
    nodes_y = dr::fmadd(nodes_y, 0.5f, 0.5f);
    weights_x *= 0.25f;

    size_t packet_count = dr::width(theta) / FloatP::Size;
    Assert(dr::width(theta) % FloatP::Size == 0);

    // Prepare cox munk distribution
    auto [sigma_c, sigma_u] = cox_munk_crosswind_upwind(wind_speed);
    sigma_c                 = dr::sqrt(sigma_c);
    sigma_u                 = dr::sqrt(sigma_u);

    MicrofacetDistributionP distr(MicrofacetType::Beckmann,
                                  dr::SqrtTwo<ScalarFloat> * sigma_u,
                                  dr::SqrtTwo<ScalarFloat> * sigma_c, shadowing);

    for (size_t i = 0; i < packet_count; ++i) {
        FloatP theta_i = dr::load<FloatP>(theta.data() + i * FloatP::Size),
               phi_i   = dr::load<FloatP>(phi.data() + i * FloatP::Size);

        // wi expressed directly in the wind-aligned frame: phi_i is, by
        // the azimuth relative to the wind direction
        Vector3fP wi = dr::sphdir(theta_i, phi_i);

        // no-shadowing branch: floored to avoid dividing by ~0 at grazing incidence.
        FloatP cos_theta_i_safe = dr::maximum(wi.z(), 1e-6f);

        FloatP result_p = 0.f;

        // compute the importance-sampled reflectance estimate for each packet.
        for (size_t j = 0; j < dr::width(nodes_x); ++j) {
            ScalarVector2f node = { nodes_x[j], nodes_y[j] };
            ScalarFloat weight  = weights_x[j] * weights_y[j];

            Normal3fP m  = std::get<0>(distr.sample(wi, node));
            Vector3fP wo = reflect(wi, m);

            FloatP cos_chi =
                       dr::clip(dr::dot(wo, m), -0.999999999f, 0.999999999f),
                   sin_chi = dr::clip(dr::sqrt(1.f - cos_chi * cos_chi),
                                      -0.999999999f, 0.999999999f);
            FloatP F = fresnel_sunglint_legacy<FloatP>(n_real, n_imag, cos_chi,
                                                       sin_chi);

            // Reweight the (isotropic-core) VNDF proposal to the true
            // anisotropic Cox-Munk/Gram-Charlier distribution.
            FloatP cm_coef = cox_munk_gram_charlier_coef<FloatP>(wind_speed, sigma_u,
                                                            sigma_c, m);

            FloatP smith;
            if (shadowing) {
                smith = distr.smith_g1(wo, m) * F * cm_coef;
            } else {
                smith = F * cm_coef * cos_chi / (cos_theta_i_safe * m.z());
            }
            dr::masked(smith, cos_chi <= 0.f || wo.z() <= 0.f) = 0.f;

            result_p += smith * weight;
        }

        // Guard against importance-sampling noise pushing the reflectance
        // estimate above 1.
        result_p = 1.f - dr::minimum(result_p, 1.f);

        dr::store(result.data() + i * FloatP::Size, result_p);
    }

    return result;
}

template <typename Float, typename Spectrum>
class OceanBSDF final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture, MicrofacetDistribution)

    using Complex2u = dr::Complex<UnpolarizedSpectrum>;

    /**
     * @brief Construct a new OceanBSDF object.
     *
     * @param props A set of properties to initialize the oceanic BSDF.
     */
    OceanBSDF(const Properties &props) : Base(props) {
        // Retrieve parameters
        m_wavelength     = props.get<ScalarFloat>("wavelength");
        m_wind_speed     = props.get<ScalarFloat>("wind_speed", 0.1f);
        m_wind_direction = props.get<ScalarFloat>("wind_direction", 0.f);
        m_chlorinity     = props.get<ScalarFloat>("chlorinity", 19.f);
        m_pigmentation   = props.get<ScalarFloat>("pigmentation", 0.3f);
        m_component      = props.get<ScalarInt32>("component", 0);
        m_shadowing      = props.get<bool>("shadowing", true);
        m_accel          = props.get<bool>("accel", true);

        // convert from North Left to East Right.
        m_wind_direction = -m_wind_direction + 90.f;
        // m_wind_direction % 360.
        m_wind_direction = m_wind_direction - (360.f * dr::floor(m_wind_direction / 360.f));
        // Degree to radians.
        m_wind_direction = dr::deg_to_rad(m_wind_direction);

        update();

        // Set the BSDF flags
        // => Whitecap and underlight reflectance is "diffuse"
        m_components.push_back(BSDFFlags::DiffuseReflection |
                               BSDFFlags::FrontSide);

        // => Sun glint reflectance at the water surface is "specular"
        m_components.push_back(BSDFFlags::GlossyReflection |
                               BSDFFlags::FrontSide);

        // Set all the flags
        for (auto c : m_components)
            m_flags |= c;
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("wavelength", m_wavelength, ParamFlags::NonDifferentiable);
        cb->put("wind_speed", m_wind_speed, ParamFlags::Differentiable);
        cb->put("wind_direction", m_wind_direction, ParamFlags::Differentiable);
        cb->put("chlorinity", m_chlorinity, ParamFlags::Differentiable);
        cb->put("pigmentation", m_pigmentation, ParamFlags::Differentiable);
        cb->put("shadowing", m_shadowing, ParamFlags::NonDifferentiable);
        cb->put("coverage", m_coverage, ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/) override {
        update();
    }


    /**
     * @brief Update the variables that can be preprocessed.
     * This includes the complex index of refraction, upwelling
     * and downwelling transmittance textures and the ocean utils
     * variables (cox-munk, underlight).
     */
    void update() {

        // compute the index of refraction
        std::tie(m_n_real, m_n_imag) = water_ior<Float, Spectrum, ScalarFloat>(
            m_ocean_props, m_wavelength, m_chlorinity);

        // Update Cox-Munk variables
        std::tie(m_sigma_c, m_sigma_u) =
            cox_munk_crosswind_upwind(m_wind_speed);
        m_sigma_c = dr::sqrt(m_sigma_c);
        m_sigma_u = dr::sqrt(m_sigma_u);

        m_r_omega = r_omega<Float, Spectrum, ScalarFloat>(
            m_ocean_props, m_wavelength, m_pigmentation);

        std::tie(m_s_wind, m_c_wind) = dr::sincos(m_wind_direction);

        {
            // Pre-compute textures for the upwelling and downwelling
            // transmittances of radiance in the water body.
            using FloatX = DynamicBuffer<ScalarFloat>;

            FloatX zeniths = dr::maximum(
                0.f, dr::linspace<FloatX>(0.f, 0.5f * dr::Pi<ScalarFloat>,
                                          MI_OCEAN_TRANSMITTANCE_RES));
            FloatX azimuths = dr::maximum(
                0.f, dr::linspace<FloatX>(0.f, 2.f * dr::Pi<ScalarFloat>,
                                         MI_OCEAN_TRANSMITTANCE_RES));
            auto [zeniths_x, azimuths_y] = dr::meshgrid(zeniths, azimuths);

            FloatX downwelling = eval_ocean_transmittance<FloatX, Spectrum>(
                zeniths_x, azimuths_y, m_n_real, m_n_imag, m_wind_speed, false,
                m_shadowing);
            FloatX upwelling = eval_ocean_transmittance<FloatX, Spectrum>(
                zeniths_x, azimuths_y, m_n_real, m_n_imag, m_wind_speed, true,
                m_shadowing);

            size_t shape[3] = { MI_OCEAN_TRANSMITTANCE_RES,
                                MI_OCEAN_TRANSMITTANCE_RES, 1 };

            m_downwelling_transmittance =
                Texture2f(TensorXf(downwelling.data(), 3, shape), m_accel,
                        m_accel, dr::FilterMode::Linear, dr::WrapMode::Clamp);
            m_upwelling_transmittance =
                Texture2f(TensorXf(upwelling.data(), 3, shape), m_accel,
                        m_accel, dr::FilterMode::Linear, dr::WrapMode::Clamp);

            dr::eval(m_downwelling_transmittance);
            dr::eval(m_upwelling_transmittance);
        }

        // Pre-compute whitecap coverage
        m_coverage = eval_whitecap_coverage();
    }

    /**
     * @brief Evaluate the whitecap coverage.
     *
     * Evaluates the whitecap coverage at the provided wavelength and wind
     * speed.
     *
     * @return Float The whitecap coverage.
     */
    ScalarFloat eval_whitecap_coverage() const {
        return whitecap_coverage_monahan(m_wind_speed);
    }

    /**
     * @brief Evaluate the whitecap reflectance.
     *
     * Evaluates the whitecap reflectance at the provided wavelength and wind
     * speed.
     *
     * @return Float The whitecap reflectance.
     */
    Float eval_whitecaps() const {
        // Proper interpolation of the effective reflectance
        ScalarFloat eff_reflectance =
            m_ocean_props.effective_reflectance(m_wavelength);

        // Compute the whitecap reflectance
        ScalarFloat whitecap_reflectance = m_coverage * eff_reflectance;

        return whitecap_reflectance;
    }

    /**
     * @brief Evaluate the sun glint reflectance.
     *
     * Evaluates the sun glint reflectance at the provided wavelength, incident
     * and outgoing directions, wind direction, wind speed, and chlorinity.
     *
     * @param wi The incident direction of the light in graphics convention.
     * @param wo The outgoing direction of the light in graphics convention.
     * @return Float The sun glint reflectance.
     */
    Spectrum eval_glint(const Vector3f &wi, const Vector3f &wo) const {

        Float cos_theta_i = Frame3f::cos_theta(wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        MicrofacetDistribution distr(MicrofacetType::Beckmann,
                                     dr::SqrtTwo<Float> * Float(m_sigma_u),
                                     dr::SqrtTwo<Float> * Float(m_sigma_c),
                                     true);

        Vector3f m = dr::normalize(wi + wo);
        // rotate to the axis aligned frame
        Vector3f m_rot  = rotate_wind_direction<false>(m);

        Float D = distr.eval(m_rot);
        D *= cox_munk_gram_charlier_coef<Float>(m_wind_speed,
                                          m_sigma_u, m_sigma_c, m_rot);
        Float result = D / (4.f * cos_theta_i * cos_theta_o);

        if (m_shadowing) {
            Vector3f wi_rot = rotate_wind_direction<false>(wi);
            Vector3f wo_rot  = rotate_wind_direction<false>(wo);
            Float G = distr.G_height_correlated(wi_rot, wo_rot, m_rot);
            result *= G;
        }

        Spectrum F;
        if constexpr (is_polarized_v<Spectrum>) {
            Complex2u n_ext(1.f, 0.f);
            Complex2u n_water(m_n_real, m_n_imag);
            F = fresnel_sunglint_polarized(n_ext, n_water, -wi, wo);
        } else {
            Float cos_chi =
                      dr::clip(dr::dot(wo, m), -0.999999999f, 0.999999999f),
                  sin_chi = dr::clip(dr::sqrt(1.f - cos_chi * cos_chi),
                                      -0.999999999f, 0.999999999f);

            F = fresnel_sunglint_legacy<Float>(m_n_real, m_n_imag, cos_chi,
                                               sin_chi);
        }

        return result * F * dr::Pi<Float>;
    }

    Float eval_transmittance(const Texture2f &data, const Float &cos_theta,
                             const Vector3f &v) const {
        Point2f uv;

        uv.x() = (dr::acos(dr::clip(cos_theta, -0.999999999f, 0.999999999f)) * (dr::InvPi<Float> * 2.f));
        uv.y() =
            (dr::atan2(v.y(), v.x()) - m_wind_direction) * dr::InvTwoPi<Float>;
        uv.y() = uv.y() - (1.f * dr::floor(uv.y())); // equivalent to uv.y % 1.f

        using Data1 = dr::Array<Float, 1>;
        return (m_accel ? data.template eval<Data1>(uv)
                        : data.template eval_nonaccel<Data1>(uv)).x();
    }

    /**
     * @brief Evaluate the underwater light reflectance.
     *
     * Evaluates the underwater light reflectance at the provided wavelength,
     * incident and outgoing directions, wind direction, wind speed, chlorinity,
     * and pigmentation.
     *
     * @param wi The incident direction of the light.
     * @param wo The outgoing direction of the light.
     * @return Float The underwater light reflectance.
     */
    Float eval_underlight(const Vector3f &wi, const Vector3f &wo) const {

        // Analogue to 6SV, we return 0.0 if the wavelength is outside the range
        // of [0.4, 0.7]
        auto outside_range = Mask(m_wavelength < 400.f || m_wavelength > 700.f);
        if (dr::any_or<false>(outside_range))
            return 0.f;

        Float t_d = eval_transmittance(m_downwelling_transmittance, wi.z(), wi);
        Float t_u = eval_transmittance(m_upwelling_transmittance, wo.z(), wi);

        // Compute the underlight term
        Float underlight = (1.f / (dr::square(m_n_real) + dr::square(m_n_imag))) *
                           (m_r_omega * t_u * t_d) /
                           (1.f - m_underlight_alpha * m_r_omega);

        return dr::select(outside_range, 0.f, underlight);
    }

    std::pair<BSDFSample3f, Spectrum>
    sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
           Float sample1, const Point2f &sample2, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        bool has_diffuse  = ctx.is_enabled(BSDFFlags::DiffuseReflection, 0),
             has_specular = ctx.is_enabled(BSDFFlags::GlossyReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        active &= cos_theta_i > 0.f;

        BSDFSample3f bs   = dr::zeros<BSDFSample3f>();
        Spectrum result(0.f);

        if (unlikely(dr::none_or<false>(active)) || (!has_diffuse && !has_specular))
            return { bs, 0.f };

        // Determine which component should be sampled
        Float t_i =
            eval_transmittance(m_downwelling_transmittance, cos_theta_i, si.wi);
        Float whitecap      = eval_whitecaps();
        Float prob_diffuse  = whitecap + t_i * (1 - whitecap),
              prob_specular = (1.f - m_coverage);

        // Cater for case where only one lobe is activated
        if (unlikely(has_specular != has_diffuse))
            prob_specular = has_specular ? 1.f : 0.f;
        else
            prob_specular = prob_specular / (prob_specular + prob_diffuse);
        prob_diffuse = 1.f - prob_specular;

        // sample diffuse or specular lobe.
        Mask sample_diffuse  = active && (sample1 < prob_diffuse),
             sample_specular = active && !sample_diffuse;

        if (dr::any_or<true>(sample_diffuse)) {
            // In the case of sampling the diffuse component, the outgoing
            // direction is sampled from a cosine-weighted hemisphere.
            dr::masked(bs.wo, sample_diffuse) =
                warp::square_to_cosine_hemisphere(sample2);
            dr::masked(bs.sampled_component, sample_diffuse) = 0;
            dr::masked(bs.sampled_type, sample_diffuse) =
                +BSDFFlags::DiffuseReflection;
        }

        if (dr::any_or<true>(sample_specular)) {
            MicrofacetDistribution distr(
                MicrofacetType::Beckmann, dr::SqrtTwo<Float> * m_sigma_u,
                dr::SqrtTwo<Float> * m_sigma_c, true);

            Vector3f wi_rot = rotate_wind_direction<false>(si.wi);
            auto [H, weight] = distr.sample(wi_rot, sample2);
            H = rotate_wind_direction<true>(H);

            Vector3f wo      = reflect(si.wi, H);

            dr::masked(bs.wo, sample_specular) = wo;
            dr::masked(bs.sampled_component, sample_specular) = 1;
            dr::masked(bs.sampled_type, sample_specular) =
                +BSDFFlags::GlossyReflection;
        }

        bs.pdf = pdf(ctx, si, bs.wo, active);
        bs.eta = 1.f;

        active &= bs.pdf > 0.f;
        result = eval(ctx, si, bs.wo, active);

        return { bs, result/bs.pdf & (active && bs.pdf > 0.f) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        bool has_diffuse = ctx.is_enabled(BSDFFlags::DiffuseReflection, 0),
             has_glint   = ctx.is_enabled(BSDFFlags::GlossyReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        // Ensure incoming and outgoing directions are in the upper hemisphere
        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely(dr::none_or<false>(active) || (!has_glint && !has_diffuse)))
            return 0.f;


        // Compute the whitecap reflectance
        Spectrum result(0.f);

        // 6SV considers the incident direction to come from the light source
        //     - `Radiance`, trace from the sensor to the light sources
        //     - `Importance`, trace from the light sources to the sensor
        Vector3f wo_hat = ctx.mode == TransportMode::Radiance ? wo : si.wi,
                 wi_hat = ctx.mode == TransportMode::Radiance ? si.wi : wo;

        // Combine the results

        // Make reflectances available for debug purposes
        UnpolarizedSpectrum whitecap_reflectance(0.f);
        UnpolarizedSpectrum underlight_reflectance(0.f);
        Spectrum glint_reflectance(0.f);

        if (has_diffuse) {
            whitecap_reflectance   = eval_whitecaps();
            underlight_reflectance = eval_underlight(wo_hat, wi_hat);
            // Diffuse scattering implies no polarization.
            result += depolarizer<Spectrum>(
                whitecap_reflectance + (1.f - whitecap_reflectance) * underlight_reflectance
            );
        }

        if (has_glint) {

            if constexpr( is_polarized_v<Spectrum> ) {
                // If sun glint is enabled, compute the glint reflectance
                glint_reflectance = eval_glint(wo_hat, wi_hat);

                /* The Stokes reference frame vector of this matrix lies in the meridian plane
                spanned by wi and n. */
                Vector3f n(0.f, 0.f, 1.f);
                Vector3f p_axis_in  =
                    dr::normalize(dr::cross(dr::normalize(dr::cross(n,-wo_hat)),-wo_hat));
                Vector3f p_axis_out =
                    dr::normalize(dr::cross(dr::normalize(dr::cross(n,wi_hat)),wi_hat));

                dr::masked( p_axis_in, dr::any(dr::isnan(p_axis_in)) ) =
                    Vector3f(0.f, 1.f, 0.f);
                dr::masked( p_axis_out, dr::any(dr::isnan(p_axis_out)) ) =
                    Vector3f(0.f, 1.f, 0.f);

                // Rotate in/out reference vector of `value` s.t. it aligns with the implicit
                // Stokes bases of -wo_hat & wi_hat. */
                glint_reflectance = mueller::rotate_mueller_basis(glint_reflectance,
                                        -wo_hat, p_axis_in, mueller::stokes_basis(-wo_hat),
                                        wi_hat, p_axis_out, mueller::stokes_basis(wi_hat));

            } else {
                // If sun glint is enabled, compute the glint reflectance
                glint_reflectance = eval_glint(wo_hat, wi_hat);
            }

            result += (1.f - m_coverage) * glint_reflectance;
        }

        dr::masked(result, active) *= cos_theta_o * dr::InvPi<Float>;

        // For debugging purposes, channel indicates which BRDF term to evaluate
        switch (m_component) {
            case 1:
                result[active] = depolarizer<Spectrum>(whitecap_reflectance);
                break;
            case 2:
                result[active] =
                    (1.f - m_coverage) * glint_reflectance;
                break;
            case 3:
                result[active] = depolarizer<Spectrum>(
                    (1.f - whitecap_reflectance) * underlight_reflectance);
                break;
            case 4:
                result[active] = depolarizer<Spectrum>(
                    whitecap_reflectance +
                    (1.f - whitecap_reflectance) * underlight_reflectance);
                break;
            default:
                break;
        }

        return result & active;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        bool has_diffuse  = ctx.is_enabled(BSDFFlags::DiffuseReflection, 0),
             has_specular = ctx.is_enabled(BSDFFlags::GlossyReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely((!has_diffuse && !has_specular) ||
                     dr::none_or<false>(active)))
            return 0.f;

        Float prob_whitecap = eval_whitecaps();
        Float t_i =
            eval_transmittance(m_downwelling_transmittance, cos_theta_i, si.wi);

        Float prob_diffuse  = t_i * (1.f - prob_whitecap) + prob_whitecap,
              prob_specular = (1.f - m_coverage);

        if (unlikely(has_specular != has_diffuse))
            prob_specular = has_specular ? 1.f : 0.f;
        else
            prob_specular = prob_specular / (prob_specular + prob_diffuse);
        prob_diffuse = 1.f - prob_specular;

        // diffuse probability
        Float prob_cosine = warp::square_to_cosine_hemisphere_pdf(wo);
        prob_whitecap *= prob_cosine;
        prob_diffuse *= prob_cosine;

        // Weight the specular component using the visible beckmann pdf
        Vector3f H = dr::normalize(wo + si.wi);

        MicrofacetDistribution distr(
            MicrofacetType::Beckmann, dr::SqrtTwo<Float> * m_sigma_u,
            dr::SqrtTwo<Float> * m_sigma_c, true);

        Vector3f H_rot = rotate_wind_direction<false>(H);
        Vector3f wi_rot = rotate_wind_direction<false>(si.wi);

        prob_specular *=
            distr.eval(H_rot) * distr.smith_g1(wi_rot, H_rot) / (4.f * cos_theta_i);

        Float result = prob_diffuse + prob_specular;

        return dr::select(active, result, 0.f);
    }

    template<bool ccw>
    Vector3f rotate_wind_direction(Vector3f w) const {
        constexpr ScalarFloat sign = ccw ? 1 : -1;

        Float c_a = m_c_wind;
        Float s_a = sign * m_s_wind;

        return Vector3f(
            c_a*w.x() - s_a*w.y(),
            s_a*w.x() + c_a*w.y(),
            w.z());
    }


    std::string to_string() const override {
        std::ostringstream oss;
        oss << "OceanLegacy[" << std::endl
            << "  component = " << string::indent(m_component) << "," << std::endl
            << "  wavelength = " << string::indent(m_wavelength) << "," << std::endl
            << "  wind_speed = " << string::indent(m_wind_speed) << "," << std::endl
            << "  wind_direction = " << string::indent(m_wind_direction) << "," << std::endl
            << "  chlorinity = " << string::indent(m_chlorinity) << "," << std::endl
            << "  pigmentation = " << string::indent(m_pigmentation) << "," << std::endl
            << "  shadowing = " << string::indent(m_shadowing) << "," << std::endl
            << "  coverage = " << string::indent(m_coverage) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(OceanBSDF)
private:
    //  User-provided fields
    ScalarInt32 m_component;
    ScalarFloat m_wavelength;
    ScalarFloat m_wind_speed;
    ScalarFloat m_wind_direction;
    ScalarFloat m_chlorinity;
    ScalarFloat m_pigmentation;
    ScalarFloat m_coverage;
    bool m_shadowing;
    bool m_accel;

    // On update fields
    ScalarFloat m_c_wind;
    ScalarFloat m_s_wind;
    ScalarFloat m_n_real;
    ScalarFloat m_n_imag;
    ScalarFloat m_sigma_c = 1.f;
    ScalarFloat m_sigma_u = 1.f;
    ScalarFloat m_r_omega = 0.f;

    Texture2f m_downwelling_transmittance;
    Texture2f m_upwelling_transmittance;

    OceanProperties<Float, Spectrum> m_ocean_props;

    // Whitecap constants
    static constexpr ScalarFloat m_f_eff_base = 0.4f;
    // Underlight constants
    static constexpr ScalarFloat m_underlight_alpha = 0.485f;

    MI_TRAVERSE_CB(Base, m_wind_speed, m_wind_direction, m_chlorinity, m_pigmentation)
};

MI_EXPORT_PLUGIN(OceanBSDF)
NAMESPACE_END(mitsuba)
