#include <mitsuba/core/warp.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sunsky.h>

#include <mutex>
#include <nanothread/nanothread.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class AvgSunskyEmitter final : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, m_flags, m_to_world)
    MI_IMPORT_TYPES(Scene, Shape, Texture)

    using EnvEmitter = Emitter<Float, Spectrum>;
    using FloatStorage = DynamicBuffer<Float>;
    using ScalarFloatStorage = DynamicBuffer<ScalarFloat>;

    AvgSunskyEmitter(const Properties &props) : Base(props) {
        if constexpr (!(is_rgb_v<Spectrum> || is_spectral_v<Spectrum>))
            Throw("Unsupported spectrum type, can only render in Spectral or RGB modes!");

        m_sun_scale = props.get<ScalarFloat>("sun_scale", 1.f);
        if (m_sun_scale < 0.f)
            Log(Error, "Invalid sun scale: %f, must be positive!", m_sun_scale);

        m_sky_scale = props.get<ScalarFloat>("sky_scale", 1.f);
        if (m_sky_scale < 0.f)
            Log(Error, "Invalid sky scale: %f, must be positive!", m_sky_scale);

        ScalarFloat turbidity = props.get<ScalarFloat>("turbidity", 3.f);
        if (turbidity < 1.f || 10.f < turbidity)
            Log(Error, "Turbidity value %f is out of range [1, 10]", turbidity);
        m_turbidity = turbidity;

        m_sun_half_aperture = dr::deg_to_rad(0.5f * props.get<ScalarFloat>("sun_aperture", 0.5358f));
        if (m_sun_half_aperture <= 0.f || 0.5f * dr::Pi<Float> <= m_sun_half_aperture)
            Log(Error, "Invalid sun aperture angle: %f, must be in ]0, 90[ degrees!", dr::rad_to_deg(2.f * m_sun_half_aperture));

        m_albedo = props.get_texture<Texture>("albedo", 0.3f);
        if (m_albedo->is_spatially_varying())
            Log(Error, "Expected a non-spatially varying radiance spectra!");

        m_year = props.get<ScalarUInt32>("year", 2025);
        m_nb_days = (m_year % 4 == 0 && (m_year % 100 != 0 || m_year % 400 == 0)) ? 366u : 365u;
        m_start_time = props.get<ScalarFloat>("start_time", 7.f);
        m_end_time = props.get<ScalarFloat>("end_time", 19.f);
        m_day_resolution = props.get<ScalarUInt32>("day_resolution", 5);

        m_location = LocationRecord<Float>(props);
        dr::make_opaque(m_year, m_start_time, m_end_time, m_day_resolution, m_location);

        // ====================== LOAD DATASETS =====================
        constexpr bool IS_RGB = is_rgb_v<Spectrum>;
        m_sky_params_dataset = sunsky_array_from_file<Float64, Float>(
            path_to_dataset<IS_RGB>(Dataset::SkyParams));
        m_sky_rad_dataset = sunsky_array_from_file<Float64, Float>(
            path_to_dataset<IS_RGB>(Dataset::SkyRadiance));

        m_sun_rad_dataset = sunsky_array_from_file<Float64, Float>(
            path_to_dataset<IS_RGB>(Dataset::SunRadiance));

        // Only used in spectral mode since limb darkening is baked in the RGB dataset
        if constexpr (is_spectral_v<Spectrum>)
            m_sun_ld = sunsky_array_from_file<Float64, Float>(
                path_to_dataset<IS_RGB>(Dataset::SunLimbDarkening));

        m_sun_rad_params = sun_params<SUN_DATASET_SIZE>(m_sun_rad_dataset, m_turbidity);


        // ================== ENVMAP INSTANTIATION ==================
        m_bitmap_resolution = {256, 512};
        m_bitmap = new Bitmap(Bitmap::PixelFormat::RGB, Struct::Type::Float32, m_bitmap_resolution, 3);
        memset(m_bitmap->data(), 0, m_bitmap->buffer_size());

        compute_avg<dr::is_jit_v<Float>>();

        // Permute axis for envmap's convention
        ScalarMatrix4f permute_axis {
            ScalarVector4f{0, 0, -1, 0},
            ScalarVector4f{1, 0,  0, 0},
            ScalarVector4f{0, 1,  0, 0},
            ScalarVector4f{0, 0,  0, 1}
        };
        ScalarTransform4f envmap_transform { permute_axis };

        Properties envmap_props = Properties("envmap");
        envmap_props.set("bitmap", static_cast<ref<Object>>(m_bitmap));
        envmap_props.set("to_world", envmap_transform);
        m_envmap = PluginManager::instance()->create_object<EnvEmitter>(envmap_props);

        m_flags = +EmitterFlags::Infinite | +EmitterFlags::SpatiallyVarying;
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("turbidity", m_turbidity, ParamFlags::NonDifferentiable);
        cb->put("sky_scale", m_sky_scale, ParamFlags::NonDifferentiable);
        cb->put("sun_scale", m_sun_scale, ParamFlags::NonDifferentiable);
        cb->put("albedo",    m_albedo,    ParamFlags::NonDifferentiable);
        cb->put("latitude",  m_location.latitude,  ParamFlags::NonDifferentiable);
        cb->put("longitude", m_location.longitude, ParamFlags::NonDifferentiable);
        cb->put("timezone",  m_location.timezone,  ParamFlags::NonDifferentiable);

        cb->put("to_world", m_to_world, ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &) override {
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

        #define CHANGED(word) string::contains(keys, word)

        //bool changed_atmosphere = CHANGED("albedo") || CHANGED("turbidity");
        //bool changed_time_record = !keys.empty() && m_active_record && (
        //        CHANGED("timezone") || CHANGED("year") ||
        //        CHANGED("day") || CHANGED("month") || CHANGED("hour") ||
        //        CHANGED("minute") || CHANGED("second") || CHANGED("latitude") ||
        //        CHANGED("longitude")
        //    );
        //bool changed_sun_dir = (!m_active_record && CHANGED("sun_direction")) || changed_time_record;


        #undef CHANGED
    }

    MI_INLINE void set_scene(const Scene *scene) override {
        m_envmap->set_scene(scene);
    }

    MI_INLINE Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        return m_envmap->eval(si, active);
    }

    MI_INLINE std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                      const Point2f &sample2,
                                      const Point2f &sample3,
                                      Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);
        return m_envmap->sample_ray(time, wavelength_sample, sample2, sample3, active);
    }

    MI_INLINE std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample,
                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);
        return m_envmap->sample_direction(it, sample, active);
    }

    MI_INLINE Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        return m_envmap->pdf_direction(it, ds, active);
    }

    MI_INLINE Spectrum eval_direction(const Interaction3f &it,
                            const DirectionSample3f &ds,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        return m_envmap->eval_direction(it, ds, active);
    }

    MI_INLINE std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        return m_envmap->sample_wavelengths(si, sample, active);
    }

    MI_INLINE std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f & sample,
                    Mask active) const override {
        return m_envmap->sample_position(time, sample, active);
    }

    /// This emitter does not occupy any particular region of space, return an invalid bounding box
    ScalarBoundingBox3f bbox() const override {
        return ScalarBoundingBox3f();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SunskyEmitter[" << std::endl
            << "  turbidity = " << string::indent(m_turbidity) << std::endl
            << "  sky_scale = " << string::indent(m_sky_scale) << std::endl
            << "  sun_scale = " << string::indent(m_sun_scale) << std::endl
            << "  albedo = " << string::indent(m_albedo) << std::endl
            << "  sun aperture (°) = " << string::indent(dr::rad_to_deg(2 * m_sun_half_aperture))
            << "  location = " << m_location.to_string()
            << "]" << std::endl;
        return oss.str();
    }

    MI_DECLARE_CLASS(AvgSunskyEmitter)

private:


    template<bool isJit>
    void compute_avg() {
        if constexpr (!isJit) {
            Log(Warn, "Using this emitter in scalar mode can cause severe performance issues");
            task_submit_and_wait(nullptr, m_nb_days, compute_avg_day, this);
        } else {

        }
    }

    static void compute_avg_day(uint32_t day, void* payload_) {
        AvgSunskyEmitter* payload = static_cast<AvgSunskyEmitter*>(payload_);
        ScalarUInt32 day_resolution = payload->m_day_resolution;
        ScalarVector2u bitmap_resolution = payload->m_bitmap_resolution;

        ref<Bitmap> result = new Bitmap(Bitmap::PixelFormat::RGB, Struct::Type::Float32, bitmap_resolution, 3);
        memset(result->data(), 0, result->buffer_size());
        ScalarFloat* bitmap_data = static_cast<ScalarFloat*>(result->data());


        ScalarFloat dt = (payload->m_end_time - payload->m_start_time) / static_cast<ScalarFloat>(day_resolution);
        DateTimeRecord<ScalarFloat> time {};
        time.year = payload->m_year;
        time.day = (ScalarInt32) day;

        DynamicBuffer<ScalarFloat> albedo = extract_albedo(payload->m_albedo);


        for (uint32_t i = 0; i < day_resolution; ++i) {
            time.hour = payload->m_start_time + i * dt;

            const auto [sun_elevation, sun_azimuth] = sun_coordinates(time, payload->m_location);
            const ScalarFloat sun_eta = 0.5f * dr::Pi<ScalarFloat> - sun_elevation;
            if (sun_eta < 0.f) continue;
            const ScalarVector3f sun_dir = sph_to_dir(sun_elevation, sun_azimuth);

            ScalarFloatStorage sky_params = sky_radiance_params<SKY_DATASET_SIZE>(payload->m_sky_params_dataset, albedo, payload->m_turbidity, sun_eta);
            ScalarFloatStorage sky_radiance = sky_radiance_params<SKY_DATASET_RAD_SIZE>(payload->m_sky_rad_dataset, albedo, payload->m_turbidity, sun_eta);

            // Iterate over top half of the image
            for (uint32_t pixel_idx = 0; pixel_idx < bitmap_resolution.x() * bitmap_resolution.y() / 2; ++pixel_idx) {
                ScalarFloat pixel_u = static_cast<ScalarFloat>(pixel_idx % bitmap_resolution.x()) / static_cast<ScalarFloat>(bitmap_resolution.x());
                ScalarFloat pixel_v = static_cast<ScalarFloat>(pixel_idx / bitmap_resolution.x()) / static_cast<ScalarFloat>(bitmap_resolution.y());

                ScalarVector3f ray_dir = sph_to_dir(
                    dr::Pi<ScalarFloat> * pixel_v,
                    dr::TwoPi<ScalarFloat> * pixel_u
                );
                ScalarFloat gamma = dr::unit_angle(ray_dir, sun_dir);

                ScalarColor3f res = 0.;
                res += eval_sky<ScalarColor3f>({0, 1, 2}, ray_dir.z(), gamma, sky_params, sky_radiance);
                res += (gamma < payload->m_sun_half_aperture ? 1.f : 0.f) * SPEC_TO_RGB_SUN_CONV *
                    eval_sun<ScalarColor3f>({0, 1, 2}, ray_dir.z(), gamma, payload->m_sun_rad_params, payload->m_sun_half_aperture) * get_area_ratio(payload->m_sun_half_aperture);
                res *= static_cast<ScalarFloat>(MI_CIE_Y_NORMALIZATION) / static_cast<ScalarFloat>(payload->m_nb_days * day_resolution);

                bitmap_data[3 * pixel_idx + 0] += res.r();
                bitmap_data[3 * pixel_idx + 1] += res.g();
                bitmap_data[3 * pixel_idx + 2] += res.b();

            }

        }

        std::lock_guard guard(s_bitmap_mutex);
        payload->m_bitmap->accumulate(result);

    }

    /**
     * \brief Extract the albedo values for the required wavelengths/channels
     *
     * \param albedo_tex
     *      Texture to extract the albedo from
     * \return
     *      The buffer with the extracted albedo values for the needed wavelengths/channels
     */
    static FloatStorage extract_albedo(const ref<Texture>& albedo_tex) {
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


    // ========= Common parameters =========
    ScalarUInt32 m_day_resolution = 100;
    Float m_turbidity = 7.f;
    ScalarFloat m_sky_scale = 1.f;
    ScalarFloat m_sun_scale = 1.f;
    ref<Texture> m_albedo;

    LocationRecord<Float> m_location;
    ScalarUInt32 m_year = 2025;
    ScalarUInt32 m_nb_days = 365;
    ScalarFloat m_start_time = 7.f;
    ScalarFloat m_end_time = 19.f;

    // ========= Sun parameter =========
    // TODO find why macro does not work
    ScalarFloat m_sun_half_aperture = dr::deg_to_rad(0.5358/2.0);

    // ========= Envmap parameters =========
    ref<Bitmap> m_bitmap;
    ref<EnvEmitter> m_envmap;
    ScalarVector2u m_bitmap_resolution;

    static std::mutex s_bitmap_mutex;

    // Permanent datasets loaded from files/memory
    FloatStorage m_sky_rad_dataset;
    FloatStorage m_sky_params_dataset;
    FloatStorage m_sun_ld; // Not initialized in RGB mode
    FloatStorage m_sun_rad_dataset;
    FloatStorage m_sun_rad_params;
};

MI_VARIANT
std::mutex AvgSunskyEmitter<Float, Spectrum>::s_bitmap_mutex{};

MI_EXPORT_PLUGIN(AvgSunskyEmitter)
NAMESPACE_END(mitsuba)
