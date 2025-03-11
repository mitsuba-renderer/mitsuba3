#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/quad.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sunsky.h>


NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-sunsky:

Sun and sky emitter (:monosp:`sunsky`)
-------------------------------------------------

.. pluginparameters::

 * - turbidity
   - |float|
   - Atmosphere turbidity, must be within [1, 10] (Default: 3, clear sky in a temperate climate).
     Smaller turbidity values (∼ 1 − 2) produce an arctic-like clear blue sky,
     whereas larger values (∼ 8 − 10) create an atmosphere that is more typical
     of a warm, humid day.
   - |exposed|

 * - albedo
   - |spectrum|
   - Ground albedo, must be within [0, 1] for each wavelength/channel, (Default: 0.3).
     This cannot be spatially varying (e.g. have bitmap as type).
   - |exposed|

 * - latitude
   - |float|
   - Latitude of the location in degrees (Default: 35.689, Tokyo's latitude).
   - |exposed|

 * - longitude
   - |float|
   - Longitude of the location in degrees (Default: 139.6917, Tokyo's longitude).
   - |exposed|

 * - timezone
   - |float|
   - Timezone of the location in hours (Default: 9).
   - |exposed|

 * - year
   - |int|
   - Year (Default: 2010).
   - |exposed|

 * - month
   - |int|
   - Month (Default: 7).
   - |exposed|

 * - day
   - |int|
   - Day (Default: 10).
   - |exposed|

 * - hour
   - |float|
   - Hour (Default: 15).
   - |exposed|

 * - minute
   - |float|
   - Minute (Default: 0).
   - |exposed|

 * - second
   - |float|
   - Second (Default: 0).
   - |exposed|

 * - sun_direction
   - |vector|
   - Direction of the sun in the sky (No defaults),
     cannot be specified along with one of the location/time parameters.
   - |exposed|, |differentiable|

 * - sun_scale
   - |float|
   - Scale factor for the sun radiance (Default: 1).
     Can be used to turn the sun off (by setting it to 0).
   - |exposed|

 * - sky_scale
   - |float|
   - Scale factor for the sky radiance (Default: 1).
     Can be used to turn the sky off (by setting it to 0).
   - |exposed|

 * - sun_aperture
   - |float|
   - Aperture angle of the sun in degrees (Default: 0.5338, normal sun aperture).
   - |exposed|

 * - to_world
   - |transform|
   - Specifies an optional emitter-to-world transformation.  (Default: none, i.e. emitter space = world space)
   - |exposed|

This plugin implements an environment emitter for the sun and sky dome.
It uses the Hosek-Wilkie sun :cite:`HosekSun2013` and sky model
:cite:`HosekSky2012` to generate strong approximations of the sky-dome without
the cost of path tracing the atmosphere.

Internally, this emitter does not compute a bitmap of the sky-dome like an
environment map, but evaluates the irradiance whenever it is needed.
Consequently, sampling is done through a Truncated Gaussian Mixture Model
pre-fitted to the given parameters :cite:`vitsas2021tgmm`.

Users should be aware that given certain parameters, the sun's radiance is
ill-represented by the linear sRGB color space. Whether Mitsuba is rendering in
spectral or RGB mode, if the final output is an sRGB image, it can happen that
it contains negative pixel values or be over-saturated. These results are left
un-clamped to let the user post-process the image to their liking, without
losing information.

Note that attaching a ``sunsky`` emitter to the scene introduces physical units
into the rendering process of Mitsuba 3, which is ordinarily a unitless system.
Specifically, the evaluated irradiance has units of power (:math:`W`) per
unit area (:math:`m^{-2}`) per steradian (:math:`sr^{-1}`) per unit wavelength
(:math:`nm^{-1}`). As a consequence, your scene should be modeled in meters for
this plugin to work properly.

.. tabs::
    .. code-tab:: xml
        :name: sunsky-light

        <emitter type="sunsky">
            <float name="hour" value="20.0"/>
        </emitter>

    .. code-tab:: python

        'type': 'sunsky',
        'hour': 20.0

*/

template <typename Float, typename Spectrum>
class SunskyEmitter final : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, m_flags, m_to_world)
    MI_IMPORT_TYPES(Scene, Shape, Texture)

    using FloatStorage = DynamicBuffer<Float>;
    using Gaussian     = dr::Array<Float, TGMM_GAUSSIAN_PARAMS>;
    using FullSpectrum = std::conditional_t<
        is_spectral_v<Spectrum>,
        unpolarized_spectrum_t<mitsuba::Spectrum<Float, WAVELENGTH_COUNT>>,
        unpolarized_spectrum_t<Spectrum>>;

    SunskyEmitter(const Properties &props) : Base(props) {
        if constexpr (!(is_rgb_v<Spectrum> || is_spectral_v<Spectrum>))
            Throw("Unsupported spectrum type, can only render in Spectral or RGB modes!");

        constexpr bool IS_RGB = is_rgb_v<Spectrum>;

        init_from_props(props);

        // Extract albedo from texture
        FloatStorage albedo = extract_albedo(m_albedo);

        // ================= UPDATE ANGLES =================
        Vector3f local_sun_dir = m_to_world.value().inverse().transform_affine(m_sun_dir);

        m_sun_angles = dir_to_sph(local_sun_dir);
        m_local_sun_frame = Frame3f(local_sun_dir);

        Float sun_eta = 0.5f * dr::Pi<Float> - m_sun_angles.y();

        // ================= GET SKY RADIANCE =================
        m_sky_params_dataset = sunsky_array_from_file<Float64, Float>(
            path_to_dataset<IS_RGB>(Dataset::SkyParams));
        m_sky_rad_dataset = sunsky_array_from_file<Float64, Float>(
            path_to_dataset<IS_RGB>(Dataset::SkyRadiance));

        m_sky_params = sky_radiance_params<SKY_DATASET_SIZE>(
            m_sky_params_dataset, albedo, m_turbidity, sun_eta),
        m_sky_radiance = sky_radiance_params<SKY_DATASET_RAD_SIZE>(
            m_sky_rad_dataset, albedo, m_turbidity, sun_eta);

        // ================= GET SUN RADIANCE =================
        m_sun_rad_dataset = sunsky_array_from_file<Float64, Float>(
            path_to_dataset<IS_RGB>(Dataset::SunRadiance));

        m_sun_radiance = sun_params<SUN_DATASET_SIZE>(
            m_sun_rad_dataset, m_turbidity);

        // Only used in spectral mode since limb darkening is baked in the RGB dataset
        if constexpr (is_spectral_v<Spectrum>)
            m_sun_ld = sunsky_array_from_file<Float64, Float>(
                path_to_dataset<IS_RGB>(Dataset::SunLimbDarkening));

        // ================= GET TGMM TABLES =================
        m_tgmm_tables = sunsky_array_from_file<Float32, Float>(
            path_to_dataset<IS_RGB>(Dataset::TGMMTables));

        FloatStorage distrib_params, mis_weights;
        std::tie(distrib_params, mis_weights) =
            build_tgmm_distribution<TGMM_DATA_SIZE>(m_tgmm_tables, m_turbidity, sun_eta);

        m_gaussians = distrib_params;
        m_gaussian_distr = DiscreteDistribution<Float>(mis_weights);

        // ================= AVERAGE RADIANCE =================
        Float sampling_w;
        ContinuousDistribution<Wavelength> wav_dist;
        std::tie(sampling_w, wav_dist) = estimate_sky_sun_ratio();
        m_sky_sampling_w = sampling_w;
        m_spectral_distr = wav_dist;

        // ================= GENERAL PARAMETERS =================

        /* Until `set_scene` is called, we have no information
        about the scene and default to the unit bounding sphere. */
        m_bsphere = BoundingSphere3f(ScalarPoint3f(0.f), 1.f);

        m_flags = +EmitterFlags::Infinite | +EmitterFlags::SpatiallyVarying;
    }

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
        callback->put_parameter("turbidity", m_turbidity, +ParamFlags::NonDifferentiable);
        callback->put_parameter("sky_scale", m_sky_scale, +ParamFlags::NonDifferentiable);
        callback->put_parameter("sun_scale", m_sun_scale, +ParamFlags::NonDifferentiable);
        callback->put_object("albedo", m_albedo.get(), +ParamFlags::NonDifferentiable);
        if (m_active_record) {
            callback->put_parameter("latitude", m_location.latitude, +ParamFlags::NonDifferentiable);
            callback->put_parameter("longitude", m_location.longitude, +ParamFlags::NonDifferentiable);
            callback->put_parameter("timezone", m_location.timezone, +ParamFlags::NonDifferentiable);
            callback->put_parameter("year", m_time.year, +ParamFlags::NonDifferentiable);
            callback->put_parameter("day", m_time.day, +ParamFlags::NonDifferentiable);
            callback->put_parameter("month", m_time.month, +ParamFlags::NonDifferentiable);
            callback->put_parameter("hour", m_time.hour, +ParamFlags::NonDifferentiable);
            callback->put_parameter("minute", m_time.minute, +ParamFlags::NonDifferentiable);
            callback->put_parameter("second", m_time.second, +ParamFlags::NonDifferentiable);
        } else {
            callback->put_parameter("sun_direction", m_sun_dir, +ParamFlags::Differentiable);
        }
        callback->put_parameter("to_world", *m_to_world.ptr(), +ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (m_sun_scale < 0.f)
            Log(Error, "Invalid sun scale: %f, must be positive!", m_sun_scale);
        if (m_sky_scale < 0.f)
            Log(Error, "Invalid sky scale: %f, must be positive!", m_sky_scale);
        if (dr::any(m_turbidity < 1.f || 10.f < m_turbidity))
            Log(Error, "Turbidity value %f is out of range [1, 10]", m_turbidity);
        if (m_sun_half_aperture <= 0.f || 0.5f * dr::Pi<Float> <= m_sun_half_aperture)
            Log(Error, "Invalid sun aperture angle: %f, must be in ]0, 90[ degrees!",
                dr::rad_to_deg(2 * m_sun_half_aperture));
        if (m_albedo->is_spatially_varying())
            Log(Error, "Expected a non-spatially varying radiance spectra!");

        #define CHANGED(word) string::contains(keys, "albedo")

        // Update sun angles
        Vector3f local_sun_dir;
        if (m_active_record &&
            (keys.empty() || CHANGED("timezone") || CHANGED("year") ||
             CHANGED("day") || CHANGED("month") || CHANGED("hour") ||
             CHANGED("minute") || CHANGED("second") || CHANGED("latitude") ||
             CHANGED("longitude")))
        {
            local_sun_dir = sun_coordinates(m_time, m_location);
            m_sun_dir = m_to_world.value().transform_affine(local_sun_dir);
        } else {
            local_sun_dir = m_to_world.value().inverse().transform_affine(m_sun_dir);
        }
        m_sun_angles = dir_to_sph(local_sun_dir);
        m_local_sun_frame = Frame3f(local_sun_dir);

        Float eta = 0.5f * dr::Pi<Float> - m_sun_angles.y();

        // Update sky
        if (keys.empty() ||
            CHANGED("albedo") || CHANGED("turbidity") ||
            CHANGED("timezone") || CHANGED("year") || CHANGED("day") ||
            CHANGED("month") || CHANGED("hour") || CHANGED("minute") || CHANGED("second") ||
            CHANGED("latitude") || CHANGED("longitude")
        ) {
            FloatStorage albedo = extract_albedo(m_albedo);
            m_sky_params = sky_radiance_params<SKY_DATASET_SIZE>(
                m_sky_params_dataset, albedo, m_turbidity, eta);
            m_sky_radiance = sky_radiance_params<SKY_DATASET_RAD_SIZE>(
                m_sky_rad_dataset, albedo, m_turbidity, eta);
        }

        // Update sun
        if (keys.empty() || CHANGED("albedo") || CHANGED("turbidity")) {
            m_sun_radiance =
                sun_params<SUN_DATASET_SIZE>(m_sun_rad_dataset, m_turbidity);
        }

        // Update TGMM
        if (keys.empty() ||
            CHANGED("albedo") || CHANGED("turbidity") ||
            CHANGED("timezone") || CHANGED("year") || CHANGED("day") ||
            CHANGED("month") || CHANGED("hour") || CHANGED("minute") || CHANGED("second") ||
            CHANGED("latitude") || CHANGED("longitude")
        ) {
            FloatStorage gaussian_params, mis_weights;
            std::tie(gaussian_params, mis_weights) =
                build_tgmm_distribution<TGMM_DATA_SIZE>(m_tgmm_tables, m_turbidity, eta);

            m_gaussians = gaussian_params;
            m_gaussian_distr = DiscreteDistribution<Float>(mis_weights);
        }

        // Update sky-sun ratio and radiance distribution
        Float sampling_w;
        ContinuousDistribution<Wavelength> wav_dist;
        std::tie(sampling_w, wav_dist) = estimate_sky_sun_ratio();

        m_sky_sampling_w = sampling_w;
        m_spectral_distr = wav_dist;

        #undef CHANGED
    }

    void set_scene(const Scene *scene) override {
        if (scene->bbox().valid()) {
            ScalarBoundingSphere3f scene_sphere =
                scene->bbox().bounding_sphere();
            m_bsphere = BoundingSphere3f(scene_sphere.center, scene_sphere.radius);
            m_bsphere.radius =
                dr::maximum(math::RayEpsilon<Float>,
                        m_bsphere.radius * (1.f + math::RayEpsilon<Float>));
        } else {
            m_bsphere.center = 0.f;
            m_bsphere.radius = math::RayEpsilon<Float>;
        }

        dr::make_opaque(m_bsphere.center, m_bsphere.radius);
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        using SpecMask   = dr::mask_t<unpolarized_spectrum_t<Spectrum>>;
        using SpecUInt32 = dr::uint32_array_t<unpolarized_spectrum_t<Spectrum>>;

        Vector3f local_wo = m_to_world.value().inverse().transform_affine(-si.wi);
        Float cos_theta = Frame3f::cos_theta(local_wo),
              gamma = dr::unit_angle(Vector3f(m_local_sun_frame.n), local_wo);

        active &= cos_theta >= 0;
        Mask hit_sun = active &
            (dr::dot(m_local_sun_frame.n, local_wo) >= dr::cos(m_sun_half_aperture));

        Spectrum res = 0.f;
        if constexpr (!is_spectral_v<Spectrum>) {
            SpecUInt32 idx = dr::zeros<SpecUInt32>();
            if constexpr (is_rgb_v<Spectrum>)
                idx = {0, 1, 2};
            else
                idx = SpecUInt32(0);

            res = m_sky_scale *
                  eval_sky<Spectrum>(idx, cos_theta, gamma, active);
            res += m_sun_scale *
                   eval_sun<Spectrum>(idx, cos_theta, gamma, hit_sun) *
                   get_area_ratio(m_sun_half_aperture) * SPEC_TO_RGB_SUN_CONV;

            res *= MI_CIE_Y_NORMALIZATION;
        } else {
            Wavelength normalized_wavelengths =
                (si.wavelengths - WAVELENGTHS<ScalarFloat>[0]) / WAVELENGTH_STEP;
            SpecMask valid_idx = (0.f <= normalized_wavelengths) &
                                 (normalized_wavelengths <= CHANNEL_COUNT - 1);

            SpecUInt32 query_idx_low = dr::floor2int<SpecUInt32>(normalized_wavelengths);
            SpecUInt32 query_idx_high = query_idx_low + 1;

            Wavelength lerp_factor = normalized_wavelengths - query_idx_low;

            // Linearly interpolate the sky's irradiance across the spectrum
            res = m_sky_scale * dr::lerp(
                eval_sky<Spectrum>(query_idx_low, cos_theta, gamma, active & valid_idx),
                eval_sky<Spectrum>(query_idx_high, cos_theta, gamma, active & valid_idx),
                lerp_factor);

            // Linearly interpolate the sun's irradiance across the spectrum
            Spectrum sun_rad_low = eval_sun<Spectrum>(
                         query_idx_low, cos_theta, gamma, hit_sun & valid_idx);
            Spectrum sun_rad_high = eval_sun<Spectrum>(
                         query_idx_high, cos_theta, gamma, hit_sun & valid_idx);
            Spectrum sun_rad = dr::lerp(sun_rad_low, sun_rad_high, lerp_factor);

            Spectrum sun_ld = compute_sun_ld<Spectrum>(
                query_idx_low, query_idx_high, lerp_factor, gamma,
                hit_sun & valid_idx
            );

            res += m_sun_scale * sun_rad * sun_ld * get_area_ratio(m_sun_half_aperture);
        }

        return depolarizer<Spectrum>(res) & active;
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                      const Point2f &sample2,
                                      const Point2f &sample3,
                                      Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample spatial component
        Point2f offset = warp::square_to_uniform_disk_concentric(sample2);

        // 2. Sample directional component
        Mask pick_sky = sample3.x() < m_sky_sampling_w;
        Vector3f d = dr::select(
                pick_sky,
                sample_sky({sample3.x() / m_sky_sampling_w, sample3.y()}, active),
                sample_sun({(sample3.x() - m_sky_sampling_w) / (1 - m_sky_sampling_w), sample3.y()})
        );
        active &= Frame3f::cos_theta(d) >= 0.f;
        // Unlike \ref sample_direction, ray goes from the envmap toward the scene
        Vector3f d_world = m_to_world.value().transform_affine(-d);

        // 3. PDF
        Float sky_pdf, sun_pdf;
        std::tie(sky_pdf, sun_pdf) = compute_pdfs(d, pick_sky, active);
        Float pdf = dr::lerp(sun_pdf, sky_pdf, m_sky_sampling_w);
        // Apply pdf of the origin offset
        pdf *= dr::InvPi<Float> * dr::rcp(dr::square(m_bsphere.radius));
        active &= pdf > 0.f;

        // 4. Sample spectrum
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.wi = d_world;
        Wavelength wavelengths;
        Spectrum weight;
        std::tie(wavelengths, weight) =
            sample_wavelengths(si, wavelength_sample, active);

        // 5. Compute ray origin
        Vector3f perpendicular_offset =
            Frame3f(d_world).to_world(Vector3f(offset.x(), offset.y(), 0));
        Point3f origin = m_bsphere.center +
                         (perpendicular_offset - d_world) * m_bsphere.radius;

        weight /= pdf;
        weight &= dr::isfinite(weight);
        return { Ray3f(origin, d_world, time, wavelengths), weight };
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample,
                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        Mask pick_sky = sample.x() < m_sky_sampling_w;

        // Sample the sun or the sky
        Vector3f sample_dir = dr::select(
                pick_sky,
                sample_sky({sample.x() / m_sky_sampling_w, sample.y()}, active),
                sample_sun({(sample.x() - m_sky_sampling_w) / (1 - m_sky_sampling_w), sample.y()})
        );
        active &= Frame3f::cos_theta(sample_dir) >= 0.f;

        // Automatically enlarge the bounding sphere when it does not contain the reference point
        Float radius = dr::maximum(m_bsphere.radius, dr::norm(it.p - m_bsphere.center)),
              dist   = 2.f * radius;

        Vector3f d = m_to_world.value().transform_affine(sample_dir);
        DirectionSample3f ds = dr::zeros<DirectionSample3f>();
        ds.p = dr::fmadd(d, dist, it.p);
        ds.n = -d;
        ds.uv = sample;
        ds.time = it.time;
        ds.delta = false;
        ds.emitter = this;
        ds.d = d;
        ds.dist = dist;

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.wi = -d;
        si.wavelengths = it.wavelengths;

        Float sky_pdf, sun_pdf;
        std::tie(sky_pdf, sun_pdf) = compute_pdfs(sample_dir, pick_sky, active);
        ds.pdf = dr::lerp(sun_pdf, sky_pdf, m_sky_sampling_w);

        Spectrum res = eval(si, active) / ds.pdf;
                 res &= dr::isfinite(res);
        return { ds, res };
    }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &ds,
                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        Vector3f local_dir = m_to_world.value().inverse().transform_affine(ds.d);
        Float sky_pdf, sun_pdf;
        std::tie(sky_pdf, sun_pdf) = compute_pdfs(local_dir, true, active);

        Float combined_pdf = dr::lerp(sun_pdf, sky_pdf, m_sky_sampling_w);
        return dr::select(active, combined_pdf, 0.f);
    }

    Spectrum eval_direction(const Interaction3f &it,
                            const DirectionSample3f &ds,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.wavelengths = it.wavelengths;
        si.wi = -ds.d;

        return eval(si, active);
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        if constexpr (is_spectral_v<Spectrum>) {
            Wavelength w_sample = math::sample_shifted<Wavelength>(sample);

            Wavelength wavelengths;
            Spectrum pdf;
            std::tie(wavelengths, pdf) =
                m_spectral_distr.sample_pdf(w_sample, active);

            SurfaceInteraction3f si_query = si;
            si_query.wavelengths = wavelengths;

            return { si_query.wavelengths, eval(si_query, active) / pdf };
        } else {
            DRJIT_MARK_USED(sample);

            return { Wavelength(0.f), eval(si, active) };
        }
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float /*time*/, const Point2f & /*sample*/,
                    Mask /*active*/) const override {
        if constexpr (dr::is_jit_v<Float>) {
            /* Do not throw an exception in JIT-compiled variants. This
               function might be invoked by DrJit's virtual function call
               recording mechanism despite not influencing any actual
               calculation. */
            return { dr::zeros<PositionSample3f>(), dr::NaN<Float> };
        } else {
            NotImplementedError("sample_position");
        }
    }

    /// This emitter does not occupy any particular region of space, return an invalid bounding box
    ScalarBoundingBox3f bbox() const override {
        return ScalarBoundingBox3f();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SunskyEmitter[" << std::endl
            << "  bsphere = " << string::indent(m_bsphere) << std::endl
            << "  turbidity = " << string::indent(m_turbidity) << std::endl
            << "  sky_scale = " << string::indent(m_sky_scale) << std::endl
            << "  sun_scale = " << string::indent(m_sun_scale) << std::endl
            << "  albedo = " << string::indent(m_albedo) << std::endl
            << "  sun aperture (°) = " << string::indent(dr::rad_to_deg(2 * m_sun_half_aperture)) << std::endl;
        if (m_active_record) {
            oss << "  location = " << m_location.to_string() << std::endl
                << "  date_time = " << m_time.to_string() << std::endl;
        } else {
            oss << "  sun_dir = " << string::indent(m_sun_dir) << std::endl;
        }
        oss << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

private:
    /**
     * \brief Evaluate the sky model for the given channel indices and angles
     *
     * Based on the Hosek-Wilkie skylight model
     * https://cgg.mff.cuni.cz/projects/SkylightModelling/HosekWilkie_SkylightModel_SIGGRAPH2012_Preprint_lowres.pdf
     * \tparam Spec
     *      Spectral type to render (adapts the number of channels)
     * \param channel_idx
     *      Indices of the channels to render
     * \param cos_theta
     *      Cosine of the angle between the z-axis (up) and the viewing direction
     * \param gamma
     *      Angle between the sun and the viewing direction
     * \param active
     *      Mask for the active lanes and channel indices
     * \return
     *      Sky radiance
     */
    template <typename Spec_, typename Spec = unpolarized_spectrum_t<Spec_>>
    Spec_ eval_sky(const dr::uint32_array_t<Spec> &channel_idx,
                   const Float &cos_theta, const Float &gamma,
                   const dr::mask_t<Spec> &active) const {

        // Gather coefficients for the skylight equation
        using SpecSkyParams = dr::Array<Spec, SKY_PARAMS>;
        SpecSkyParams coefs = dr::gather<SpecSkyParams>(m_sky_params, channel_idx, active);

        Float cos_gamma = dr::cos(gamma),
              cos_gamma_sqr = dr::square(cos_gamma);

        Spec c1 = 1 + coefs[0] * dr::exp(coefs[1] / (cos_theta + 0.01f));
        Spec chi = (1 + cos_gamma_sqr) /
                   dr::pow(1 + dr::square(coefs[8]) - 2 * coefs[8] * cos_gamma, 1.5);
        Spec c2 = coefs[2] + coefs[3] * dr::exp(coefs[4] * gamma) +
                  coefs[5] * cos_gamma_sqr + coefs[6] * chi +
                  coefs[7] * dr::safe_sqrt(cos_theta);

        return c1 * c2 * dr::gather<Spec>(m_sky_radiance, channel_idx, active);
    }

   /**
    * \brief Evaluates the sun model for the given channel indices and angles
    *
    * The template parameter is used to render the full 11 wavelengths at once
    * in pre-computations
    *
    * Based on the Hosek-Wilkie sun model
    * https://cgg.mff.cuni.cz/publications/adding-a-solar-radiance-function-to-the-hosek-wilkie-skylight-model/
    *
    * \tparam Spec
    *       Spectral type to render (adapts the number of channels)
    * \param channel_idx
    *       Indices of the channels to render
    * \param cos_theta
    *       Cosine of the angle between the z-axis (up) and the viewing direction
    * \param gamma
    *       Angle between the sun and the viewing direction
    * \param active
    *       Mask for the active lanes and channel indices
    * \return
    *       Sun radiance
    */
    template <typename Spec_, typename Spec = unpolarized_spectrum_t<Spec_>>
    Spec_ eval_sun(const dr::uint32_array_t<Spec> &channel_idx,
                   const Float &cos_theta, const Float &gamma,
                   const dr::mask_t<Spec> &active) const {
        using SpecUInt32 = dr::uint32_array_t<Spec>;

        // Angles computation
        Float elevation = 0.5f * dr::Pi<Float> - dr::acos(cos_theta);

        // Find the segment of the piecewise function we are in
        UInt32 pos = dr::floor2int<UInt32>(
            dr::cbrt(2 * elevation * dr::InvPi<Float>) * SUN_SEGMENTS);
        pos = dr::minimum(pos, SUN_SEGMENTS - 1);

        Float break_x =
            0.5f * dr::Pi<Float> * dr::pow((Float) pos / SUN_SEGMENTS, 3.f);
        Float x = elevation - break_x;

        Spec solar_radiance = 0.f;
        if constexpr (is_spectral_v<Spec>) {
            DRJIT_MARK_USED(gamma);
            // Compute sun radiance
            SpecUInt32 global_idx = pos * WAVELENGTH_COUNT * SUN_CTRL_PTS +
                                    channel_idx * SUN_CTRL_PTS;
            for (uint8_t k = 0; k < SUN_CTRL_PTS; ++k)
                solar_radiance +=
                    dr::pow(x, k) *
                    dr::gather<Spec>(m_sun_radiance, global_idx + k, active);
        } else {
            // Reproduces the spectral computation for RGB, however, in this case,
            // limb darkening is baked into the dataset, hence the two for-loops
            Float cos_psi = sun_cos_psi<Float>(gamma, m_sun_half_aperture);
            SpecUInt32 global_idx = pos * (3 * SUN_CTRL_PTS * SUN_LD_PARAMS) +
                                    channel_idx * (SUN_CTRL_PTS * SUN_LD_PARAMS);

            for (uint8_t k = 0; k < SUN_CTRL_PTS; ++k) {
                for (uint8_t j = 0; j < SUN_LD_PARAMS; ++j) {
                    SpecUInt32 idx = global_idx + k * SUN_LD_PARAMS + j;
                    solar_radiance +=
                        dr::pow(x, k) *
                        dr::pow(cos_psi, j) *
                        dr::gather<Spec>(m_sun_radiance, idx, active);
                }
            }
        }

        return solar_radiance & active;
    }

    /**
     * \brief Computes the limb darkening of the sun for a given gamma.
     *
     * Only works for spectral mode since limb darkening is baked into the RGB
     * model
     *
     * \tparam Spec
     *      Spectral type to render (adapts the number of channels)
     * \param channel_idx_low
     *      Indices of the lower wavelengths
     * \param channel_idx_high
     *      Indices of the upper wavelengths
     * \param lerp_f
     *      Linear interpolation factor for wavelength
     * \param gamma
     *      Angle between the sun's center and the viewing ray
     * \param active
     *      Indicates if the channel indices are valid and that the sun was hit
     * \return
     *      The spectral values of limb darkening to apply to the sun's
     *      radiance by multiplication
     */
    template <typename Spec_, typename Spec = unpolarized_spectrum_t<Spec_>>
    Spec_ compute_sun_ld(const dr::uint32_array_t<Spec> &channel_idx_low,
                         const dr::uint32_array_t<Spec> &channel_idx_high,
                         const wavelength_t<Spec> &lerp_f, const Float &gamma,
                         const dr::mask_t<Spec> &active) const {
        using SpecLdArray = dr::Array<Spec, SUN_LD_PARAMS>;

        SpecLdArray sun_ld_low  = dr::gather<SpecLdArray>(m_sun_ld, channel_idx_low, active),
                    sun_ld_high = dr::gather<SpecLdArray>(m_sun_ld, channel_idx_high, active),
                    sun_ld_coefs = dr::lerp(sun_ld_low, sun_ld_high, lerp_f);

        Float cos_psi = sun_cos_psi<Float>(gamma, m_sun_half_aperture);

        Spec sun_ld = 0.f;
        for (uint8_t j = 0; j < SUN_LD_PARAMS; ++j)
            sun_ld += dr::pow(cos_psi, j) * sun_ld_coefs[j];

        return sun_ld & active;
    }

    /**
     * \brief Samples the sky from the truncated gaussian mixture with the given sample.
     *
     * Based on the Truncated Gaussian Mixture Model (TGMM) for sky dome by N. Vitsas and K. Vardis
     * https://diglib.eg.org/items/b3f1efca-1d13-44d0-ad60-741c4abe3d21
     *
     * \param sample
     *      Sample uniformly distributed in [0, 1]^2
     * \param active
     *      Mask for the active lanes
     * \return
     *      The sampled direction in the sky and its PDF
     */
    Vector3f sample_sky(Point2f sample, const Mask& active) const {
        // Sample a gaussian from the mixture
        UInt32 idx;
        Float temp_sample;
        std::tie(idx, temp_sample) =
            m_gaussian_distr.sample_reuse(sample.x(), active);

        // {mu_phi, mu_theta, sigma_phi, sigma_theta, weight}
        Gaussian gaussian = dr::gather<Gaussian>(m_gaussians, idx, active);

        Point2f a = { 0.0 },
                b = { dr::TwoPi<Float>, 0.5f * dr::Pi<Float> };

        Point2f mu    = { gaussian[0], gaussian[1] },
                sigma = { gaussian[2], gaussian[3] };

        Point2f cdf_a = gaussian_cdf(mu, sigma, a),
                cdf_b = gaussian_cdf(mu, sigma, b);

        sample = dr::lerp(cdf_a, cdf_b, Point2f{temp_sample, sample.y()});
        // Clamp to erfinv's domain of definition
        sample = dr::clip(sample, dr::Epsilon<Float>, dr::OneMinusEpsilon<Float>);

        Point2f angles = dr::SqrtTwo<Float> * dr::erfinv(2 * sample - 1) * sigma + mu;

        // From fixed reference frame where sun_phi = pi/2 to local frame
        angles.x() += m_sun_angles.x() - 0.5f * dr::Pi<Float>;
        // Clamp theta to avoid negative z-axis values (FP errors)
        angles.y() = dr::minimum(angles.y(), 0.5f * dr::Pi<Float> - dr::Epsilon<Float>);

        return dr::sphdir(angles.y(), angles.x());
    }

    /**
     * \brief Samples the sun from a uniform cone with the given sample
     *
     * \param sample
     *      Sample to generate the sun ray
     * \return
     *      The sampled sun direction
     */
    Vector3f sample_sun(const Point2f& sample) const {
        return m_local_sun_frame.to_world(
            warp::square_to_uniform_cone<Float>(sample, dr::cos(m_sun_half_aperture))
        );
    }

    /**
     * Computes the PDFs of the sky and sun for the given local direction
     *
     * \param local_dir
     *      Local direction in the sky
     * \param check_sun
     *      Indicates if the sun's intersection should be tested
     * \param active
     *      Mask for the active lanes
     * \return
     *      The sky and sun PDFs
     */
    std::pair<Float, Float> compute_pdfs(const Vector3f &local_dir,
                                         const Mask &check_sun,
                                         Mask active) const {
        // Check for bounds on PDF
        Float sin_theta = Frame3f::sin_theta(local_dir);
        active &= (Frame3f::cos_theta(local_dir) >= 0.f) && (sin_theta != 0.f);
        sin_theta = dr::maximum(sin_theta, dr::Epsilon<Float>);
        Float sky_pdf = tgmm_pdf(dir_to_sph(local_dir), active) / sin_theta;

        Float cosine_cutoff = dr::cos(m_sun_half_aperture);
        Float sun_pdf = warp::square_to_uniform_cone_pdf(
            m_local_sun_frame.to_local(local_dir), cosine_cutoff);
        Mask valid = (dr::dot(m_local_sun_frame.n, local_dir) >= cosine_cutoff);
        sun_pdf = dr::select(!check_sun || valid, sun_pdf, 0.f);

        return {sky_pdf, sun_pdf};
    }

    /**
     * \brief Computes the PDF from the TGMM for the given angles
     *
     * \param angles
     *      Angles of the vector in local coordinates (phi, theta)
     * \param active
     *      Mask for the active lanes and valid rays
     * \return
     *      The PDF of the sky for the given angles with no sin(theta) factor
     */
    Float tgmm_pdf(Point2f angles, Mask active) const {
        // From local frame to reference frame where sun_phi = pi/2 and phi is in [0, 2pi]
        angles.x() -= m_sun_angles.x() - 0.5f * dr::Pi<Float>;
        angles.x() = dr::select(angles.x() < 0, angles.x() + dr::TwoPi<Float>, angles.x());
        angles.x() = dr::select(angles.x() > dr::TwoPi<Float>, angles.x() - dr::TwoPi<Float>, angles.x());

        // Bounds check for theta
        active &= (angles.y() >= 0.f) && (angles.y() <= 0.5f * dr::Pi<Float>);

        // Bounding points of the truncated gaussian mixture
        Point2f a = { 0.0 },
                b = { dr::TwoPi<Float>, 0.5f * dr::Pi<Float> };

        Float pdf = 0.0;
        for (size_t i = 0; i < 4 * TGMM_COMPONENTS; ++i) {
            size_t base_idx = i * TGMM_GAUSSIAN_PARAMS;
            // {mu_phi, mu_theta}, {sigma_phi, sigma_theta}
            Point2f mu    = { m_gaussians[base_idx + 0], m_gaussians[base_idx + 1] },
                    sigma = { m_gaussians[base_idx + 2], m_gaussians[base_idx + 3] };

            Point2f cdf_a = gaussian_cdf(mu, sigma, a),
                    cdf_b = gaussian_cdf(mu, sigma, b);

            Float volume = (cdf_b.x() - cdf_a.x()) *
                           (cdf_b.y() - cdf_a.y()) *
                           (sigma.x() * sigma.y());

            Point2f sample = (angles - mu) / sigma;
            Float gaussian_pdf = warp::square_to_std_normal_pdf(sample);

            pdf += m_gaussians[base_idx + 4] * gaussian_pdf / volume;
        }
        return dr::select(active, pdf, 0.f);
    }

    /**
     * \brief Estimates the ratio of sky to sun luminance over the hemisphere,
     * can be used to estimate the sampling weight of the sun and sky.
     *
     * \return The sky's ratio of luminance, in [0, 1] <br>
     *         The continuous distribution of the sky-dome's integrated radiance
     */
    std::pair<Float, ContinuousDistribution<Wavelength>> estimate_sky_sun_ratio() const {
        ScalarVector2f range = {
            WAVELENGTHS<ScalarFloat>[1],
            WAVELENGTHS<ScalarFloat>[WAVELENGTH_COUNT - 1]
        };

        if constexpr (!dr::is_array_v<Float>) {
            // Mean ratio over the range of parameters (turbidity, sun angle)
            // And uniform spectral sampling
            ScalarFloat distribution[2] = {1.f, 1.f};
            return { 0.5f, ContinuousDistribution<Wavelength>(range, distribution, 2) };
        } else {
            FullSpectrum sky_radiance = dr::zeros<FullSpectrum>(),
                         sun_radiance = dr::zeros<FullSpectrum>();

            dr::uint32_array_t<FullSpectrum> channel_idx;
            if constexpr (is_rgb_v<Spectrum>)
                channel_idx = {0, 1, 2};
            else if constexpr (is_monochromatic_v<Spectrum>)
                channel_idx = {0};
            else
                channel_idx = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

            // Quadrature points and weights
            const auto [x, w_x] = quad::gauss_legendre<Float>(200);

            // Compute sky radiance over hemisphere
            {
                // Mapping for [-1, 1] x [-1, 1] -> [0, 2pi] x [0, 1]
                Float J = 0.5f * dr::Pi<Float>;
                Float phi = dr::Pi<Float> * (x + 1),
                      cos_theta = 0.5f * (x + 1);

                std::tie(phi, cos_theta) = dr::meshgrid(phi, cos_theta);
                const auto [w_phi, w_cos_theta] = dr::meshgrid(w_x, w_x);

                const auto [sin_phi, cos_phi] = dr::sincos(phi);
                const auto sin_theta = dr::safe_sqrt(1 - dr::square(cos_theta));

                Vector3f sky_wo {sin_theta * cos_phi, sin_theta * sin_phi, cos_theta};
                Float gamma = dr::unit_angle(Vector3f(m_local_sun_frame.n), sky_wo);

                FullSpectrum ray_radiance =
                    eval_sky<FullSpectrum>(channel_idx, cos_theta, gamma, true) * w_phi * w_cos_theta;
                sky_radiance = dr::sum_inner(ray_radiance) * J;
            }

            // Compute sun radiance over hemisphere
            {
                ScalarFloat cosine_cutoff = dr::cos(m_sun_half_aperture);

                // Mapping for [-1, 1] x [-1, 1] -> [0, 2pi] x [cos(alpha/2), 1]
                Float J = 0.5f * dr::Pi<ScalarFloat> * (1 - cosine_cutoff);
                Float phi = dr::Pi<Float> * (x + 1),
                      cos_gamma = 0.5f * ((1 - cosine_cutoff) * x + (1 + cosine_cutoff));

                std::tie(phi, cos_gamma) = dr::meshgrid(phi, cos_gamma);
                const auto [w_phi, w_cos_theta] = dr::meshgrid(w_x, w_x);

                const auto sin_gamma = dr::safe_sqrt(1 - dr::square(cos_gamma));
                const auto [sin_phi, cos_phi] = dr::sincos(phi);

                // View ray in local sun coordinates
                Vector3f sun_wo {sin_gamma * cos_phi, sin_gamma * sin_phi, cos_gamma};
                Float gamma = dr::unit_angle_z(sun_wo);

                // View ray in local emitter coordinates
                sun_wo = m_local_sun_frame.to_world(sun_wo);

                Float cos_theta = Frame3f::cos_theta(sun_wo);

                Mask active = cos_theta >= 0;
                FullSpectrum ray_radiance =
                    eval_sun<FullSpectrum>(channel_idx, cos_theta, gamma, active) *
                    w_phi * w_cos_theta;

                // Apply sun limb darkening if not already
                if constexpr (is_spectral_v<Spectrum>)
                    ray_radiance *= compute_sun_ld<FullSpectrum>(
                        channel_idx, channel_idx, 0.f, gamma, active);

                sun_radiance = dr::sum_inner(ray_radiance) * J;
            }

            // Extract luminance
            Float sky_lum = m_sky_scale, sun_lum = m_sun_scale;
            if constexpr (is_rgb_v<Spectrum>) {
                sky_lum *= luminance(sky_radiance);
                sun_lum *= luminance(sun_radiance) * get_area_ratio(m_sun_half_aperture) * SPEC_TO_RGB_SUN_CONV;
            } else if constexpr (is_monochromatic_v<Spectrum>) {
                // Use dummy wavelength
                sky_lum *= luminance(sky_radiance, {0});
                sun_lum *= luminance(sun_radiance, {0}) * get_area_ratio(m_sun_half_aperture) * SPEC_TO_RGB_SUN_CONV;
            } else  {
                FullSpectrum wavelengths = {320, 360, 400, 440, 480, 520, 560, 600, 640, 680, 720};

                sky_lum *= luminance(sky_radiance, wavelengths);
                sun_lum *= luminance(sun_radiance, wavelengths) * get_area_ratio(m_sun_half_aperture);
            }

            // Normalize quantities for valid distribution
            Float res = sky_lum / (sky_lum + sun_lum);
                  res = dr::select(dr::isnan(res), 0.f, res);

            if constexpr (is_spectral_v<Spectrum>) {
                // Compute spectrum distribution
                FullSpectrum avg_spec = sun_radiance + sky_radiance;
                FloatStorage spectrum = dr::zeros<FloatStorage>(CHANNEL_COUNT - 1);

                // Skip first spectral value since 320nm is not
                // currently supported in Mitsuba
                for (ScalarUInt32 i = 0; i < CHANNEL_COUNT - 1; ++i)
                    dr::scatter(spectrum, avg_spec[i + 1], (UInt32) i);

                if (dr::all(spectrum == 0.f))
                    spectrum += 1.f; // Prevent error in the distribution

                return { res, ContinuousDistribution<Wavelength>(range, spectrum) };
            } else {
                return { res, ContinuousDistribution<Wavelength>() };
            }
        }
    }

    void init_from_props(const Properties& props) {
        m_sun_scale = props.get<ScalarFloat>("sun_scale", 1.f);
        if (m_sun_scale < 0.f)
            Log(Error, "Invalid sun scale: %f, must be positive!", m_sun_scale);

        m_sky_scale = props.get<ScalarFloat>("sky_scale", 1.f);
        if (m_sky_scale < 0.f)
            Log(Error, "Invalid sky scale: %f, must be positive!", m_sky_scale);

        ScalarFloat turb = props.get<ScalarFloat>("turbidity", 3.f);
        if (turb < 1.f || 10.f < turb)
            Log(Error, "Turbidity value %f is out of range [1, 10]", turb);
        m_turbidity = turb;
        dr::make_opaque(m_turbidity);

        m_sun_half_aperture = dr::deg_to_rad(0.5f * props.get<ScalarFloat>("sun_aperture", 0.5358));
        if (m_sun_half_aperture <= 0.f || 0.5f * dr::Pi<Float> <= m_sun_half_aperture)
            Log(Error, "Invalid sun aperture angle: %f, must be in ]0, 90[ degrees!", dr::rad_to_deg(2 * m_sun_half_aperture));

        m_albedo = props.texture<Texture>("albedo", 0.3f);
        if (m_albedo->is_spatially_varying())
            Log(Error, "Expected a non-spatially varying radiance spectra!");

        if (props.has_property("sun_direction")) {
            if (props.has_property("latitude") || props.has_property("longitude")
                || props.has_property("timezone") || props.has_property("year")
                || props.has_property("month") || props.has_property("day")
                || props.has_property("hour") || props.has_property("minute")
                || props.has_property("second")) {
                Log(Error, "Both the 'sun_direction' and parameters for time/location "
                           "were provided, both information cannot be given at the same time!");
            }

            m_active_record = false;
            m_sun_dir = dr::normalize(props.get<ScalarVector3f>("sun_direction"));
            dr::make_opaque(m_sun_dir);
        } else {
            m_location.latitude  = props.get<ScalarFloat>("latitude", 35.6894f);
            m_location.longitude = props.get<ScalarFloat>("longitude", 139.6917f);
            m_location.timezone  = props.get<ScalarFloat>("timezone", 9);
            m_time.year          = props.get<ScalarInt32>("year", 2010);
            m_time.month         = props.get<ScalarUInt32>("month", 7);
            m_time.day           = props.get<ScalarUInt32>("day", 10);
            m_time.hour          = props.get<ScalarFloat>("hour", 15.0f);
            m_time.minute        = props.get<ScalarFloat>("minute", 0.0f);
            m_time.second        = props.get<ScalarFloat>("second", 0.0f);

            m_active_record = true;
            dr::make_opaque(m_location.latitude, m_location.longitude, m_location.timezone,
                            m_time.year, m_time.day, m_time.month, m_time.hour, m_time.minute, m_time.second);

            m_sun_dir = sun_coordinates(m_time, m_location);
            if (dr::all(m_sun_dir.z() < 0))
                Log(Warn, "The sun is below the horizon at the specified time and location!");

            m_sun_dir = m_to_world.value().transform_affine(m_sun_dir);
        }
    }

    /**
     * \brief Extract the albedo values for the required wavelengths/channels
     *
     * \param albedo_tex
     *      Texture to extract the albedo from
     * \return
     *      The buffer with the extracted albedo values for the needed wavelengths/channels
     */
    FloatStorage extract_albedo(const ref<Texture>& albedo_tex) const {
        FloatStorage albedo = dr::zeros<FloatStorage>(CHANNEL_COUNT);
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

        if constexpr (is_rgb_v<Spectrum>) {
            albedo = dr::ravel(albedo_tex->eval(si));

        } else if constexpr (dr::is_array_v<Float> && is_spectral_v<Spectrum>) {
            si.wavelengths = dr::load<FloatStorage>(WAVELENGTHS<ScalarFloat>, CHANNEL_COUNT);
            albedo = albedo_tex->eval(si)[0];
        } else if (!dr::is_array_v<Float> && is_spectral_v<Spectrum>) {
            for (ScalarUInt32 i = 0; i < CHANNEL_COUNT; ++i) {
                if constexpr (!is_monochromatic_v<Spectrum>)
                    si.wavelengths = WAVELENGTHS<ScalarFloat>[i];
                dr::scatter(albedo, albedo_tex->eval(si)[0], (UInt32) i);
            }
        }

        if (dr::any(albedo < 0.f || albedo > 1.f))
            Log(Error, "Albedo values must be in [0, 1], got: %f", albedo);

        return albedo;
    }

    // ================================================================================================
    // ========================================= ATTRIBUTES ===========================================
    // ================================================================================================

    /// Number of channels used in the skylight model
    static constexpr uint32_t CHANNEL_COUNT =
        is_spectral_v<Spectrum> ?
        WAVELENGTH_COUNT :
        (is_monochromatic_v<Spectrum> ? 1 : 3);

    // Dataset sizes
    static constexpr uint32_t SKY_DATASET_SIZE =
        TURBITDITY_LVLS * ALBEDO_LVLS * SKY_CTRL_PTS * CHANNEL_COUNT *
        SKY_PARAMS;
    static constexpr uint32_t SKY_DATASET_RAD_SIZE =
        TURBITDITY_LVLS * ALBEDO_LVLS * SKY_CTRL_PTS * CHANNEL_COUNT;
    static constexpr uint32_t SUN_DATASET_SIZE =
        TURBITDITY_LVLS * CHANNEL_COUNT * SUN_SEGMENTS * SUN_CTRL_PTS *
        (is_spectral_v<Spectrum> ? 1 : SUN_LD_PARAMS);
    static constexpr uint32_t TGMM_DATA_SIZE =
        (TURBITDITY_LVLS - 1) * ELEVATION_CTRL_PTS * TGMM_COMPONENTS *
        TGMM_GAUSSIAN_PARAMS;

    BoundingSphere3f m_bsphere;

    // ========= Common parameters =========
    Float m_turbidity;
    Float m_sky_sampling_w;
    ScalarFloat m_sky_scale;
    ScalarFloat m_sun_scale;
    ref<Texture> m_albedo;

    // ========= Sun parameters =========

    /// Sun direction in world coordinates
    Vector3f m_sun_dir;
    /// Sun angles in local coordinates, (phi, theta)
    Point2f m_sun_angles;
    Frame3f m_local_sun_frame;
    ScalarFloat m_sun_half_aperture;
    /// Indicates if the plugin was initialized with a location/time record
    bool m_active_record;
    LocationRecord<Float> m_location;
    DateTimeRecord<Float> m_time;

    // ========= Radiance parameters =========
    FloatStorage m_sky_params;
    FloatStorage m_sky_radiance;
    FloatStorage m_sun_radiance;

    // ========= Sampling parameters =========
    FloatStorage m_gaussians;
    DiscreteDistribution<Float> m_gaussian_distr;
    ContinuousDistribution<Wavelength> m_spectral_distr;

    // Permanent datasets loaded from files/memory
    FloatStorage m_sky_rad_dataset;
    FloatStorage m_sky_params_dataset;
    FloatStorage m_sun_ld; // Not initialized in RGB mode
    FloatStorage m_sun_rad_dataset;
    FloatStorage m_tgmm_tables;
};

MI_IMPLEMENT_CLASS_VARIANT(SunskyEmitter, Emitter)
MI_EXPORT_PLUGIN(SunskyEmitter, "Sun and Sky dome background emitter")
NAMESPACE_END(mitsuba)
