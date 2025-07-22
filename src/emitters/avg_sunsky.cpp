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

    using SkyRadDataset    = std::conditional_t<dr::is_jit_v<Float>, Color3f, FloatStorage>;
    using SkyParamsDataset = std::conditional_t<dr::is_jit_v<Float>, dr::Array<Color3f, SKY_PARAMS>, FloatStorage>;

    AvgSunskyEmitter(const Properties &props) : Base(props) {
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

        m_albedo_tex = props.get_texture<Texture>("albedo", 0.3f);
        if (m_albedo_tex->is_spatially_varying())
            Log(Error, "Expected a non-spatially varying radiance spectra!");

        LocationRecord<ScalarFloat> location {props};
        DateTimeRecord<ScalarFloat> start_day, end_day;
        start_day.year = props.get<ScalarInt32>("start_year", 2025);
        start_day.month = props.get<ScalarInt32>("start_month", 1);
        start_day.day = props.get<ScalarInt32>("start_day", 1);

        end_day.year = props.get<ScalarInt32>("end_year", 2025);
        end_day.month = props.get<ScalarInt32>("end_month", 12);
        end_day.day = props.get<ScalarInt32>("end_day", 31);

        m_nb_days = DateTimeRecord<ScalarFloat>::get_days_between(start_day, end_day, location);

        m_location = location;
        m_start_date = start_day;
        m_end_date = end_day;

        m_uniform_resolution = props.get<bool>("uniform_resolution", true);
        m_window_start_time = props.get<ScalarFloat>("window_start_time", 7.f);
        m_window_end_time = props.get<ScalarFloat>("window_end_time", 19.f);
        m_time_resolution = props.get<ScalarUInt32>("time_resolution", 5);

        dr::make_opaque(m_start_date, m_end_date, m_location);

        // ====================== LOAD DATASETS =====================
        // Force RGB datasets to load since there are no spectral envmaps
        constexpr bool IS_RGB = true;
        m_sky_params_dataset = sunsky_array_from_file<Float64, Float>(
            path_to_dataset<IS_RGB>(Dataset::SkyParams));
        m_sky_rad_dataset = sunsky_array_from_file<Float64, Float>(
            path_to_dataset<IS_RGB>(Dataset::SkyRadiance));

        m_sun_rad_dataset = sunsky_array_from_file<Float64, Float>(
            path_to_dataset<IS_RGB>(Dataset::SunRadiance));

        m_sun_rad_params = sun_params<SUN_DATASET_SIZE>(m_sun_rad_dataset, m_turbidity);


        // ================== ENVMAP INSTANTIATION ==================
        m_bitmap_resolution = {512, 256};
        m_bitmap = compute_avg_bitmap();

        // Permute axis for envmap's convention
        ScalarMatrix4f permute_axis {
            ScalarVector4f{0, 0, -1, 0},
            ScalarVector4f{1, 0,  0, 0},
            ScalarVector4f{0, 1,  0, 0},
            ScalarVector4f{0, 0,  0, 1}
        };
        ScalarAffineTransform4f envmap_transform { permute_axis };

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
        cb->put("albedo",    m_albedo_tex,    ParamFlags::NonDifferentiable);
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
        if (m_albedo_tex->is_spatially_varying())
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
            << "  albedo = " << string::indent(m_albedo_tex) << std::endl
            << "  sun aperture (°) = " << string::indent(dr::rad_to_deg(2.f * m_sun_half_aperture))
            << "  location = " << m_location.to_string()
            << "]" << std::endl;
        return oss.str();
    }

    MI_DECLARE_CLASS(AvgSunskyEmitter)

private:

    struct Datasets {
        Vector3f sun_dir;
        SkyRadDataset sky_rad;
        SkyParamsDataset sky_params;

        DRJIT_STRUCT(Datasets, sun_dir, sky_rad, sky_params)
    };


    Datasets compute_dataset(const UInt32& time_idx, const FloatStorage& albedo) const {
        DateTimeRecord<Float> time = dr::zeros<DateTimeRecord<Float>>();
        time.year = m_start_date.year;

        if (!m_uniform_resolution) {
            Float fractional_day = m_nb_days * (time_idx / Float(m_time_resolution - 1));

            time.day = dr::floor2int<Int32>(fractional_day);
            time.hour = m_window_start_time + (m_window_end_time - m_window_start_time) * dr::fmod(fractional_day, 1.f);
        } else {
            const auto [time_idx_div, time_idx_mod] = dr::idivmod(time_idx, m_time_resolution);

            time.day = m_start_date.day + time_idx_div;
            time.hour = m_window_start_time + (m_window_end_time - m_window_start_time) * time_idx_mod / (m_time_resolution - 1);
        }


        const auto [sun_elevation, sun_azimuth] = sun_coordinates(time, m_location);
        const Float sun_eta = 0.5f * dr::Pi<Float> - sun_elevation;

        Datasets dataset = {
            sph_to_dir(sun_elevation, sun_azimuth),
            sky_radiance_params<SKY_DATASET_RAD_SIZE, SkyRadDataset>(m_sky_rad_dataset, albedo, m_turbidity, sun_eta),
            sky_radiance_params<SKY_DATASET_SIZE, SkyParamsDataset>(m_sky_params_dataset,  albedo, m_turbidity, sun_eta)
        };

        return dataset;
    }

    Vector3f compute_ray_dir(const UInt32& pixel_idx, const ScalarPoint2u& resolution) const {
        const auto [pixel_u_idx, pixel_v_idx] = dr::idivmod(pixel_idx, resolution.x());

        Point2f coord = Point2f(pixel_v_idx, pixel_u_idx) + 0.5f;
                coord /= resolution - 1.f;
                coord *= Point2f(dr::TwoPi<Float>, dr::Pi<Float>);

        return sph_to_dir(coord.y(), coord.x());
    }

    struct ThreadPayload {
        const ScalarUInt32 nb_threads;
        const AvgSunskyEmitter *emitter;
        const FloatStorage albedo;
        ref<Bitmap> bitmap;
    };

    ref<Bitmap> compute_avg_bitmap() const {
        ref<Bitmap> res_bitmap = new Bitmap(Bitmap::PixelFormat::RGB, Struct::Type::Float32, m_bitmap_resolution, 3);
        memset(res_bitmap->data(), 0, res_bitmap->buffer_size());

        FloatStorage albedo = extract_albedo(m_albedo_tex);

        if constexpr (!dr::is_jit_v<Float>) {

            ThreadPayload payload = {
                core_count(), this, albedo, res_bitmap
            };

            task_submit_and_wait(nullptr, payload.nb_threads, compute_avg_bitmap_thread, &payload);

        } else {

            // ================== COMPUTE DATASETS =====================
            size_t nb_time_samples = m_time_resolution * (m_uniform_resolution ? m_nb_days : 1);
            Datasets datasets = compute_dataset(dr::arange<UInt32>(nb_time_samples), albedo);

            // ==================== COMPUTE RAYS ======================
            // Only the top half of the image is used
            size_t nb_rays = dr::prod(m_bitmap_resolution) / 2;

            UInt32 pixel_idx = dr::arange<UInt32>(nb_rays);
            Vector3f ray_dir = compute_ray_dir(pixel_idx, m_bitmap_resolution);

            // =============== BLEND TWO DIMENSIONS ===========
            // This part computes a window on the 2D grid of (rays, time)
            // It slides along the time dimension such that the window has
            // a maximum "area" of 2^32 in order to not have a wavefront size to large.

            size_t time_width = UINT32_MAX / nb_rays;

            // Prevent perfect square edge case
            if (time_width * nb_rays >= UINT32_MAX) {
                if (time_width == 1)
                    Throw("Image resolution is too high, cannot compute average sunsky!");
                time_width -= 1;
            }

            Log(Info, "Using %zu time samples per wavefront and running %u iterations",
                time_width, dr::ceil2int<ScalarUInt32>(nb_time_samples * ((float)nb_rays / UINT32_MAX))
            );

            Color3f result = dr::zeros<Color3f>(nb_rays);
            const UInt32 frame_time_idx = dr::arange<UInt32>(time_width);
            auto [pixel_idx_wav, time_idx] = dr::meshgrid(pixel_idx, frame_time_idx);

            // Slide the window along the time axis
            for (size_t frame_start = 0; frame_start < nb_time_samples; frame_start += time_width) {
                UInt32 time_idx_wav = time_idx + frame_start;
                Mask active = time_idx_wav < nb_time_samples;

                Vector3f ray_dir_wav = dr::gather<Vector3f>(ray_dir, pixel_idx_wav, active);
                Datasets datasets_wav = dr::gather<Datasets>(datasets, time_idx_wav, active);

                active &= (ray_dir_wav.z() >= 0.f) & (datasets_wav.sun_dir.z() >= 0.f);

                Float gamma = dr::unit_angle(ray_dir_wav, datasets_wav.sun_dir);
                const Float& cos_theta = ray_dir_wav.z();

                // Compute sky appearance over the hemisphere
                Color3f rays = m_sky_scale * eval_sky<Color3f, Float, SkyParamsDataset, SkyRadDataset, UInt32>(
                                                        nb_rays * time_idx_wav, cos_theta, gamma,
                                                        datasets_wav.sky_params, datasets_wav.sky_rad, active);

                rays += m_sun_scale * SPEC_TO_RGB_SUN_CONV * get_area_ratio(m_sun_half_aperture) *
                        eval_sun<Color3f>({0, 1, 2}, cos_theta, gamma, m_sun_rad_params, m_sun_half_aperture, active & (gamma < m_sun_half_aperture));

                rays *= MI_CIE_Y_NORMALIZATION / nb_time_samples;

                // Accumulate results
                dr::scatter_add(result.r(), rays.r(), pixel_idx_wav, active);
                dr::scatter_add(result.g(), rays.g(), pixel_idx_wav, active);
                dr::scatter_add(result.b(), rays.b(), pixel_idx_wav, active);

                // TODO is it needed?
                // Launch wavefront
                // dr::eval(result);
            }

            Float res = dr::migrate(dr::ravel(result), AllocType::Host);

            dr::sync_thread();

            memcpy(res_bitmap->data(), res.data(), 3 * sizeof(ScalarFloat) * nb_rays);

        }

        return res_bitmap;
    }

    static void compute_avg_bitmap_thread(uint32_t thread_id, void* payload_) {
        ThreadPayload* payload = static_cast<ThreadPayload*>(payload_);
        const AvgSunskyEmitter* emitter = static_cast<const AvgSunskyEmitter*>(payload->emitter);

        ScalarVector2u bitmap_resolution = emitter->m_bitmap_resolution;
        ref<Bitmap> result = new Bitmap(Bitmap::PixelFormat::RGB, Struct::Type::Float32, bitmap_resolution, 3);
        memset(result->data(), 0, result->buffer_size());

        ScalarFloat* bitmap_data = static_cast<ScalarFloat*>(result->data());

        const uint32_t nb_time_samples = emitter->m_time_resolution * (emitter->m_uniform_resolution ? emitter->m_nb_days : 1);
        uint32_t times_per_thread = nb_time_samples / payload->nb_threads + 1;

        if (thread_id * times_per_thread >= nb_time_samples) return;

        // Adjust for last thread edge case
        const uint32_t this_times_per_thread =
                    (thread_id + 1) * times_per_thread > nb_time_samples ?
                        nb_time_samples - thread_id * times_per_thread :
                        times_per_thread;

        for (uint32_t i = 0; i < this_times_per_thread; ++i) {

            uint32_t time_idx = times_per_thread * thread_id + i;
            Datasets dataset = emitter->compute_dataset(time_idx, payload->albedo);
            if (dataset.sun_dir.z() < 0.f) continue;

            // Iterate over top half of the image
            for (uint32_t pixel_idx = 0; pixel_idx < bitmap_resolution.x() * bitmap_resolution.y() / 2; ++pixel_idx) {
                ScalarVector3f ray_dir = emitter->compute_ray_dir(pixel_idx, bitmap_resolution);
                if (ray_dir.z() < 0.f) continue;

                ScalarFloat gamma = dr::unit_angle(ray_dir, dataset.sun_dir);

                ScalarColor3f res = 0.;
                res += eval_sky<ScalarColor3f>({0, 1, 2}, ray_dir.z(), gamma, dataset.sky_params, dataset.sky_rad);
                res += SPEC_TO_RGB_SUN_CONV * get_area_ratio(emitter->m_sun_half_aperture) *
                        eval_sun<ScalarColor3f>({0, 1, 2}, ray_dir.z(), gamma, emitter->m_sun_rad_params, emitter->m_sun_half_aperture, gamma < emitter->m_sun_half_aperture);
                res *= MI_CIE_Y_NORMALIZATION / nb_time_samples;

                bitmap_data[3 * pixel_idx + 0] += res.r();
                bitmap_data[3 * pixel_idx + 1] += res.g();
                bitmap_data[3 * pixel_idx + 2] += res.b();

            }

        }

        std::lock_guard guard(s_bitmap_mutex);
        payload->bitmap->accumulate(result);

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


    // ========= Common parameters =========
    bool m_uniform_resolution = false;
    ScalarUInt32 m_time_resolution = 300;
    Float m_turbidity = 7.f;
    ScalarFloat m_sky_scale = 1.f;
    ScalarFloat m_sun_scale = 1.f;
    ref<Texture> m_albedo_tex;

    LocationRecord<Float> m_location;
    DateTimeRecord<Float> m_start_date, m_end_date;
    ScalarUInt32 m_nb_days;
    ScalarFloat m_window_start_time = 7.f;
    ScalarFloat m_window_end_time = 19.f;

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
    FloatStorage m_sun_rad_dataset;
    FloatStorage m_sun_rad_params;
};

MI_VARIANT
std::mutex AvgSunskyEmitter<Float, Spectrum>::s_bitmap_mutex{};

MI_EXPORT_PLUGIN(AvgSunskyEmitter)
NAMESPACE_END(mitsuba)
