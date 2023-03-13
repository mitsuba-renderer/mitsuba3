#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/distr_2d.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <drjit/tensor.h>
#include <drjit/texture.h>
#include <drjit/struct.h>
#include <mutex>

NAMESPACE_BEGIN(mitsuba)

#define MI_MIPMAP_LUT_SIZE 128

/// Specifies the desired antialiasing filter
enum MIPFilterType {
    /// No filtering, nearest neighbor lookups
    Nearest = 0,
    /// No filtering, only bilinear interpolation
    Bilinear = 1,
    /// Basic trilinear filtering
    Trilinear = 2,
    /// Elliptically weighted average
    EWA = 3
};


template <typename Float, typename Spectrum>
class MIPMapTexture final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    MIPMapTexture(const Properties &props) : Texture(props) {
        m_transform = props.get<ScalarTransform4f>("to_uv", ScalarTransform4f())
                          .extract();
        if (m_transform != ScalarTransform3f())
            dr::make_opaque(m_transform);

        if (props.has_property("bitmap")) {
            // Creates a Bitmap texture directly from an existing Bitmap object
            if (props.has_property("filename"))
                Throw("Cannot specify both \"bitmap\" and \"filename\".");
            Log(Debug, "Loading bitmap texture from memory...");
            // Note: ref-counted, so we don't have to worry about lifetime
            ref<Object> other = props.object("bitmap");
            Bitmap *b = dynamic_cast<Bitmap *>(other.get());
            if (!b)
                Throw("Property \"bitmap\" must be a Bitmap instance.");
            m_bitmap = b;
        } else {
            // Creates a Bitmap texture by loading an image from the filesystem
            FileResolver* fs = Thread::thread()->file_resolver();
            fs::path file_path = fs->resolve(props.string("filename"));
            m_name = file_path.filename().string();
            Log(Debug, "Loading bitmap texture from \"%s\" ..", m_name);
            m_bitmap = new Bitmap(file_path);
        }

        std::string filter_mode_str = props.string("filter_type", "bilinear");
        if (filter_mode_str == "nearest")
            m_filter_mode = dr::FilterMode::Nearest;
        else if (filter_mode_str == "bilinear")
            m_filter_mode = dr::FilterMode::Linear;
        else
            Throw("Invalid filter type \"%s\", must be one of: \"nearest\", or "
                  "\"bilinear\"!", filter_mode_str);

        std::string wrap_mode_str = props.string("wrap_mode", "repeat");
        if (wrap_mode_str == "repeat"){
            m_wrap_mode = dr::WrapMode::Repeat;
            m_boundary_cond = FilterBoundaryCondition::Repeat;
        }
        else if (wrap_mode_str == "mirror"){
            m_wrap_mode = dr::WrapMode::Mirror;
            m_boundary_cond = FilterBoundaryCondition::Mirror;
        }
        else if (wrap_mode_str == "clamp"){
            m_wrap_mode = dr::WrapMode::Clamp;
            m_boundary_cond = FilterBoundaryCondition::Clamp;
        }
        else
            Throw("Invalid wrap mode \"%s\", must be one of: \"repeat\", "
                  "\"mirror\", or \"clamp\"!", wrap_mode_str);

        /* Convert to linear RGB float bitmap, will be converted
           into spectral profile coefficients below (in place) */
        Bitmap::PixelFormat pixel_format = m_bitmap->pixel_format();
        switch (pixel_format) {
            case Bitmap::PixelFormat::Y:
            case Bitmap::PixelFormat::YA:
                pixel_format = Bitmap::PixelFormat::Y;
                break;

            case Bitmap::PixelFormat::RGB:
            case Bitmap::PixelFormat::RGBA:
            case Bitmap::PixelFormat::XYZ:
            case Bitmap::PixelFormat::XYZA:
                pixel_format = Bitmap::PixelFormat::RGB;
                break;

            default:
                Throw("The texture needs to have a known pixel "
                      "format (Y[A], RGB[A], XYZ[A] are supported).");
        }

        /* Should Mitsuba disable transformations to the stored color data?
           (e.g. sRGB to linear, spectral upsampling, etc.) */
        m_raw = props.get<bool>("raw", false);
        if (m_raw) {
            /* Don't undo gamma correction in the conversion below.
               This is needed, e.g., for normal maps. */
            m_bitmap->set_srgb_gamma(false);
        }

        m_accel = props.get<bool>("accel", true);

        // Convert the image into the working floating point representation
        m_bitmap =
            m_bitmap->convert(pixel_format, struct_type_v<ScalarFloat>, false);

        // Upsampling to 2x2
        if (dr::any(m_bitmap->size() < 2)) {
            Log(Warn,
                "Image must be at least 2x2 pixels in size, up-sampling..");
            using ReconstructionFilter = Bitmap::ReconstructionFilter;
            ref<ReconstructionFilter> rfilter =
                PluginManager::instance()->create_object<ReconstructionFilter>(
                    Properties("tent"));
            m_bitmap =
                m_bitmap->resample(dr::maximum(m_bitmap->size(), 2), rfilter);
        }

        ScalarFloat *ptr = (ScalarFloat *) m_bitmap->data();
        size_t pixel_count = m_bitmap->pixel_count();
        bool exceed_unit_range = false;

        double mean = 0.0;
        if (m_bitmap->channel_count() == 3) {
            if (is_spectral_v<Spectrum> && !m_raw) {
                for (size_t i = 0; i < pixel_count; ++i) {
                    ScalarColor3f value = dr::load<ScalarColor3f>(ptr);
                    if (!all(value >= 0 && value <= 1))
                        exceed_unit_range = true;
                    value = srgb_model_fetch(value);
                    mean += (double) srgb_model_mean(value);
                    dr::store(ptr, value);
                    ptr += 3;
                }
            } else {
                for (size_t i = 0; i < pixel_count; ++i) {
                    ScalarColor3f value = dr::load<ScalarColor3f>(ptr);
                    if (!all(value >= 0 && value <= 1))
                        exceed_unit_range = true;
                    mean += (double) luminance(value);
                    ptr += 3;
                }
            }
        } else if (m_bitmap->channel_count() == 1) {
            for (size_t i = 0; i < pixel_count; ++i) {
                ScalarFloat value = ptr[i];
                if (!(value >= 0 && value <= 1))
                    exceed_unit_range = true;
                mean += (double) value;
            }
        } else {
            Throw("Unsupported channel count: %d (expected 1 or 3)",
                  m_bitmap->channel_count());
        }

        if (exceed_unit_range && !m_raw)
            Log(Warn,
                "MIPMapTexture: texture named \"%s\" contains pixels that "
                "exceed the [0, 1] range!",
                m_name);

        m_mean = Float(mean / pixel_count);

        // Generate MIP map hierarchy; downsample using a 2-lobed Lanczos reconstruction filter
        std::string mip_filter_str = props.string("mipmap_filter_type", "trilinear");
        if (mip_filter_str == "nearest")
            m_mipmap = MIPFilterType::Nearest;
        else if (mip_filter_str == "bilinear")
            m_mipmap = MIPFilterType::Bilinear;
        else if (mip_filter_str == "trilinear"){
            if (filter_mode_str == "nearest"){
                Log(Warn, "Mipmap filter may not be compatible with texture filter");
            }
            m_mipmap = MIPFilterType::Trilinear;
        }
        else if (mip_filter_str == "ewa")
            m_mipmap = MIPFilterType::EWA;
        else
            Throw("Invalid filter type \"%s\", must be one of: \"nearest\", or "
                  "\"bilinear\"! or \"trilinear\", or \"ewa\" ", filter_mode_str);

        // Get Anisotropy of EWA
        m_maxAnisotropy = props.get<ScalarFloat>("maxAnisotropy", 16);
        // if (m_maxAnisotropy < 1){
        //     Log(Warn, "maxAnisotropy clamped to 1");
        //     m_maxAnisotropy = 1;
        // }
        // else if (m_maxAnisotropy > 16){
        //     Log(Warn, "maxAnisotropy clamped to 16");
        //     m_maxAnisotropy = 16;
        // }

        // Get resample filter
        using ReconstructionFilter = Bitmap::ReconstructionFilter;
        Properties rfilterProps("box");
        // rfilterProps.set_int("lobes", 2);
        m_rfilter = static_cast<ReconstructionFilter *> (
            PluginManager::instance()->create_object<ReconstructionFilter>(rfilterProps));

        build_pyramid();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("data",  m_texture.tensor(), +ParamFlags::Differentiable);
        callback->put_parameter("to_uv", m_transform,        +ParamFlags::NonDifferentiable);
    }

    void
    parameters_changed(const std::vector<std::string> &keys = {}) override {
        if (keys.empty() || string::contains(keys, "data")) {
            const size_t channels = m_texture.shape()[2];
            if (channels != 1 && channels != 3)
                Throw("parameters_changed(): The bitmap texture %s was changed "
                      "to have %d channels, only textures with 1 or 3 channels "
                      "are supported!",
                      to_string(), channels);
            else if (m_texture.shape()[0] < 2 || m_texture.shape()[1] < 2)
                Throw("parameters_changed(): The bitmap texture %s was changed,"
                      " it must be at least 2x2 pixels in size!",
                      to_string());

            m_texture.set_tensor(m_texture.tensor());
            rebuild_internals(true, m_distr2d != nullptr);
        }
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.shape()[2];
        if (channels == 3 && is_spectral_v<Spectrum> && m_raw) {
            DRJIT_MARK_USED(si);
            Throw("The bitmap texture %s was queried for a spectrum, but "
                  "texture conversion into spectra was explicitly disabled! "
                  "(raw=true)",
                  to_string());
        } else {
            if (dr::none_or<false>(active))
                return dr::zeros<UnpolarizedSpectrum>();

            if constexpr (is_monochromatic_v<Spectrum>) {
                if (channels == 1)
                    return interpolate_1(si, active);
                else // 3 channels
                    return luminance(interpolate_3(si, active));
            }
            else{
                if (channels == 1)
                    return interpolate_1(si, active);
                else { // 3 channels
                    if constexpr (is_spectral_v<Spectrum>){
                        return interpolate_spectral(si, active);
                    }
                    else{
                        return interpolate_3(si, active);
                    }
                }
            }
        }
    }

    Float eval_1(const SurfaceInteraction3f &si,
                 Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.shape()[2];
        if (channels == 3 && is_spectral_v<Spectrum> && !m_raw) {
            DRJIT_MARK_USED(si);
            Throw("eval_1(): The bitmap texture %s was queried for a "
                  "monochromatic value, but texture conversion to color "
                  "spectra had previously been requested! (raw=false)",
                  to_string());
        } else {
            if (dr::none_or<false>(active))
                return dr::zeros<Float>();

            if (channels == 1)
                return interpolate_1(si, active);
            else // 3 channels
                return luminance(interpolate_3(si, active));
        }
    }

    Vector2f eval_1_grad(const SurfaceInteraction3f &si,
                         Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        std::cout<<"computing grad 1"<<std::endl;

        const size_t channels = m_texture.shape()[2];
        if (channels == 3 && is_spectral_v<Spectrum> && !m_raw) {
            DRJIT_MARK_USED(si);
            Throw(
                "eval_1_grad(): The bitmap texture %s was queried for a "
                "monochromatic gradient value, but texture conversion to color "
                "spectra had previously been requested! (raw=false)",
                to_string());
        } else {
            if (dr::none_or<false>(active))
                return dr::zeros<Vector2f>();

            if (m_texture.filter_mode() == dr::FilterMode::Linear) {
                if constexpr (!dr::is_array_v<Mask>)
                    active = true;

                Point2f uv = m_transform.transform_affine(si.uv);

                Float f00, f10, f01, f11;
                if (channels == 1) {
                    dr::Array<Float *, 4> fetch_values;
                    fetch_values[0] = &f00;
                    fetch_values[1] = &f10;
                    fetch_values[2] = &f01;
                    fetch_values[3] = &f11;

                    if (m_accel)
                        m_texture.eval_fetch(uv, fetch_values, active);
                    else
                        m_texture.eval_fetch_nonaccel(uv, fetch_values, active);
                } else { // 3 channels
                    Color3f v00, v10, v01, v11;
                    dr::Array<Float *, 4> fetch_values;
                    fetch_values[0] = v00.data();
                    fetch_values[1] = v10.data();
                    fetch_values[2] = v01.data();
                    fetch_values[3] = v11.data();

                    if (m_accel)
                        m_texture.eval_fetch(uv, fetch_values, active);
                    else
                        m_texture.eval_fetch_nonaccel(uv, fetch_values, active);

                    f00 = luminance(v00);
                    f10 = luminance(v10);
                    f01 = luminance(v01);
                    f11 = luminance(v11);
                }

                ScalarVector2i res = resolution();
                uv = dr::fmadd(uv, res, -0.5f);
                Vector2i uv_i = dr::floor2int<Vector2i>(uv);
                Point2f w1 = uv - Point2f(uv_i),
                        w0 = 1.f - w1;

                // Partials w.r.t. pixel coordinate x and y
                Vector2f df_xy{
                    dr::fmadd(w0.y(), f10 - f00, w1.y() * (f11 - f01)),
                    dr::fmadd(w0.x(), f01 - f00, w1.x() * (f11 - f10))
                };

                // Partials w.r.t. u and v (include uv transform by transpose
                // multiply)
                Matrix3f uv_tm = m_transform.matrix;
                Vector2f df_uv{ uv_tm.entry(0, 0) * df_xy.x() +
                                    uv_tm.entry(1, 0) * df_xy.y(),
                                uv_tm.entry(0, 1) * df_xy.x() +
                                    uv_tm.entry(1, 1) * df_xy.y() };
                return res * df_uv;
            }
            // else if (m_filter_type == FilterType::Nearest)
            return Vector2f(0.f);
        }
    }

    Color3f eval_3(const SurfaceInteraction3f &si,
                   Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.shape()[2];
        if (channels != 3) {
            DRJIT_MARK_USED(si);
            Throw("eval_3(): The bitmap texture %s was queried for a RGB "
                  "value, but it is monochromatic!",
                  to_string());
        } else if (is_spectral_v<Spectrum> && !m_raw) {
            DRJIT_MARK_USED(si);
            Throw("eval_3(): The bitmap texture %s was queried for a RGB "
                  "value, but texture conversion to color spectra had "
                  "previously been requested! (raw=false)",
                  to_string());
        } else {
            if (dr::none_or<false>(active))
                return dr::zeros<Color3f>();

            return interpolate_3(si, active);
        }
    }

    std::pair<Point2f, Float>
    sample_position(const Point2f &sample, Mask active = true) const override {
        if (dr::none_or<false>(active))
            return { dr::zeros<Point2f>(), dr::zeros<Float>() };

        if (!m_distr2d)
            init_distr();

        auto [pos, pdf, sample2] = m_distr2d->sample(sample, active);

        ScalarVector2i res = resolution();
        ScalarVector2f inv_resolution = dr::rcp(ScalarVector2f(res));

        if (m_texture.filter_mode() == dr::FilterMode::Nearest) {
            sample2 = (Point2f(pos) + sample2) * inv_resolution;
        } else {
            sample2 = (Point2f(pos) + 0.5f + warp::square_to_tent(sample2)) *
                      inv_resolution;

            switch (m_texture.wrap_mode()) {
                case dr::WrapMode::Repeat:
                    sample2[sample2 < 0.f] += 1.f;
                    sample2[sample2 > 1.f] -= 1.f;
                    break;

                /* Texel sampling is restricted to [0, 1] and only interpolation
                   with one row/column of pixels beyond that is considered, so
                   both clamp/mirror effectively use the same strategy. No such
                   distinction is needed for the pdf() method. */
                case dr::WrapMode::Clamp:
                case dr::WrapMode::Mirror:
                    sample2[sample2 < 0.f] = -sample2;
                    sample2[sample2 > 1.f] = 2.f - sample2;
                    break;
            }
        }

        return { sample2, pdf * dr::prod(res) };
    }

    Float pdf_position(const Point2f &pos_, Mask active = true) const override {
        if (dr::none_or<false>(active))
            return dr::zeros<Float>();

        if (!m_distr2d)
            init_distr();

        ScalarVector2i res = resolution();
        if (m_texture.filter_mode() == dr::FilterMode::Linear) {
            // Scale to bitmap resolution and apply shift
            Point2f uv = dr::fmadd(pos_, res, -.5f);

            // Integer pixel positions for bilinear interpolation
            Vector2i uv_i = dr::floor2int<Vector2i>(uv);

            // Interpolation weights
            Point2f w1 = uv - Point2f(uv_i),
                    w0 = 1.f - w1;

            Float v00 = m_distr2d->pdf(m_texture.wrap(uv_i + Point2i(0, 0)),
                                       active),
                  v10 = m_distr2d->pdf(m_texture.wrap(uv_i + Point2i(1, 0)),
                                       active),
                  v01 = m_distr2d->pdf(m_texture.wrap(uv_i + Point2i(0, 1)),
                                       active),
                  v11 = m_distr2d->pdf(m_texture.wrap(uv_i + Point2i(1, 1)),
                                       active);

            Float v0 = dr::fmadd(w0.x(), v00, w1.x() * v10),
                  v1 = dr::fmadd(w0.x(), v01, w1.x() * v11);

            return dr::fmadd(w0.y(), v0, w1.y() * v1) * dr::prod(res);
        } else {
            // Scale to bitmap resolution, no shift
            Point2f uv = pos_ * res;

            // Integer pixel positions for nearest-neighbor interpolation
            Vector2i uv_i = m_texture.wrap(dr::floor2int<Vector2i>(uv));

            return m_distr2d->pdf(uv_i, active) * dr::prod(res);
        }
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f &_si, const Wavelength &sample,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureSample, active);

        if (dr::none_or<false>(active))
            return { dr::zeros<Wavelength>(), dr::zeros<UnpolarizedSpectrum>() };

        if constexpr (is_spectral_v<Spectrum>) {
            SurfaceInteraction3f si(_si);
            si.wavelengths = MI_CIE_MIN + (MI_CIE_MAX - MI_CIE_MIN) * sample;
            return { si.wavelengths,
                     eval(si, active) * (MI_CIE_MAX - MI_CIE_MIN) };
        } else {
            DRJIT_MARK_USED(sample);
            UnpolarizedSpectrum value = eval(_si, active);
            return { dr::empty<Wavelength>(), value };
        }
    }

    ScalarVector2i resolution() const override {
        const size_t *shape = m_texture.shape();
        return { (int) shape[1], (int) shape[0] };
    }

    Float mean() const override { return m_mean; }

    bool is_spatially_varying() const override { return true; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "MIPMapTexture[" << std::endl
            << "  name = \"" << m_name << "\"," << std::endl
            << "  resolution = \"" << resolution() << "\"," << std::endl
            << "  raw = " << (int) m_raw << "," << std::endl
            << "  mean = " << m_mean << "," << std::endl
            << "  transform = " << string::indent(m_transform) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

protected:
    /**
     * \brief Evaluates the texture at the given surface interaction using
     * spectral upsampling
     */
    MI_INLINE UnpolarizedSpectrum
    interpolate_spectral(const SurfaceInteraction3f &si, Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform.transform_affine(si.uv);

        if (m_texture.filter_mode() == dr::FilterMode::Linear) {
            Color3f v00, v10, v01, v11;
            dr::Array<Float *, 4> fetch_values;
            fetch_values[0] = v00.data();
            fetch_values[1] = v10.data();
            fetch_values[2] = v01.data();
            fetch_values[3] = v11.data();

            if (m_accel)
                m_texture.eval_fetch(uv, fetch_values, active);
            else
                m_texture.eval_fetch_nonaccel(uv, fetch_values, active);

            UnpolarizedSpectrum c00, c10, c01, c11, c0, c1;
            c00 = srgb_model_eval<UnpolarizedSpectrum>(v00, si.wavelengths);
            c10 = srgb_model_eval<UnpolarizedSpectrum>(v10, si.wavelengths);
            c01 = srgb_model_eval<UnpolarizedSpectrum>(v01, si.wavelengths);
            c11 = srgb_model_eval<UnpolarizedSpectrum>(v11, si.wavelengths);

            ScalarVector2i res = resolution();
            uv = dr::fmadd(uv, res, -.5f);
            Vector2i uv_i = dr::floor2int<Vector2i>(uv);

            // Interpolation weights
            Point2f w1 = uv - Point2f(uv_i), w0 = 1.f - w1;

            c0 = dr::fmadd(w0.x(), c00, w1.x() * c10);
            c1 = dr::fmadd(w0.x(), c01, w1.x() * c11);

            return dr::fmadd(w0.y(), c0, w1.y() * c1);
        } else {
            Color3f out;
            if (m_accel)
                m_texture.eval(uv, out.data(), active);
            else
                m_texture.eval_nonaccel(uv, out.data(), active);

            return srgb_model_eval<UnpolarizedSpectrum>(out, si.wavelengths);
        }
    }

    /**
     * \brief Evaluates the texture at the given surface interaction
     *
     * Should only be used when the texture has exactly 1 channel.
     */
    MI_INLINE Float interpolate_1(const SurfaceInteraction3f &si,
                                   Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform.transform_affine(si.uv); //{0.00729447091, 0.929102302}; 

        // Get correctly transformed m_rfilter duv/dxy. TODO: Optimization?
        const ScalarMatrix3f uv_tm = m_transform.matrix;

        Vector2f duv_dx = //{0.00571198482, -0.00528087607};
            dr::abs(Vector2f{
            uv_tm.entry(0, 0) * si.duv_dx.x() + uv_tm.entry(0, 1) * si.duv_dx.y(),
            uv_tm.entry(1, 0) * si.duv_dx.x() + uv_tm.entry(1, 1) * si.duv_dx.y() 
        });
        Vector2f duv_dy = //{-0.000342758431, -0.00090997509};
            dr::abs(Vector2f{
            uv_tm.entry(0, 0) * si.duv_dy.x() + uv_tm.entry(0, 1) * si.duv_dy.y(),
            uv_tm.entry(1, 0) * si.duv_dy.x() + uv_tm.entry(1, 1) * si.duv_dy.y() 
        });            

        Float out = 0;

        if (m_mipmap == MIPFilterType::Nearest || m_mipmap == MIPFilterType::Bilinear){
            if (m_accel)
                m_pyramid[0].eval(uv, &out, active);
            else
                m_pyramid[0].eval_nonaccel(uv, &out, active);
            return out;         
        }
        const ScalarVector2u size = m_res[0];

        Vector2f duv0(duv_dx.x() * size.x(), duv_dx.y() * size.y());
        Vector2f duv1(duv_dy.x() * size.x(), duv_dy.y() * size.y());
        Vector2f tmp(duv0);

        dr::masked(duv0, dr::norm(duv0) < dr::norm(duv1)) = duv1;
        dr::masked(duv1, dr::norm(tmp) < dr::norm(duv1)) = tmp;

        Float 
        //       root = dr::hypot(A-C, B),
        //       Aprime = 0.5f * (A + C - root),
        //       Cprime = 0.5f * (A + C + root),
              majorRadius =  dr::norm(duv0), // dr::select(dr::neq(Aprime, 0), dr::sqrt(F / Aprime), 0),
              minorRadius =  dr::norm(duv1);// dr::select(dr::neq(Cprime, 0), dr::sqrt(F / Cprime), 0);

        // If isTri, perform trilinear filter
        Mask isTri = (m_mipmap == Trilinear | !(minorRadius > 0) | !(majorRadius > 0));

        // EWA info
        Mask isSkinny = (minorRadius * m_maxAnisotropy < majorRadius);
        dr::masked(duv1, isSkinny) = duv1 * majorRadius / (minorRadius * m_maxAnisotropy);
        dr::masked(minorRadius, isSkinny) = majorRadius / m_maxAnisotropy;



        // Trilinear level
        Float level = dr::log2(dr::maximum(dr::maximum(dr::maximum(duv0[0], duv1[0]), dr::maximum(duv0[1], duv1[1])), dr::Epsilon<Float>));
        // EWA level select
        dr::masked(level, !isTri) = dr::maximum((Float) 0.0f, dr::log2(minorRadius));
        Int32 lower = dr::floor2int<Int32>(level);
        Float alpha = level - lower;

        // std::cout<<minorRadius/1024<<std::endl;
        
        Mask isBilinear = !isTri; // majorRadius < 1 | 

        // defalt level: 0
        Mask isZero = lower < 0;
        Mask isInf = (lower >= m_levels-1);

        Float c_lower = 0;
        Float c_upper = 0;
        Float c_tmp = 0;

        // For level < 0
        if (m_accel){
            m_pyramid[0].eval(uv, &c_tmp, active & (isTri | isBilinear));
        }
        else{
            m_pyramid[0].eval_nonaccel(uv, &c_tmp, active & (isTri | isBilinear));
        }

        out = c_tmp;

        // For level >= 0
        for(int i = 1; i<m_levels; i++){
            if (m_accel){
                dr::masked(c_lower, dr::eq(i-1, lower) & active & !isZero) = c_tmp;

                m_pyramid[i].eval(uv, &c_tmp, active);
                dr::masked(c_upper, dr::eq(i-1, lower) & active & !isZero) = c_tmp;
            }
            else{
                dr::masked(c_lower, dr::eq(i-1, lower) & active & !isZero) = c_tmp;

                m_pyramid[i].eval_nonaccel(uv, &c_tmp, active);
                dr::masked(c_upper, dr::eq(i-1, lower) & active & !isZero) = c_tmp;
            }
        }

        // Deal with level < 0
        dr::masked(out, active & isTri & !isZero)  = c_upper * alpha + c_lower * (1.f - alpha);

        // This is for EWA with invalid parameters of the ellipse (e.g isBilinear)
        dr::masked(out, active & !isTri & !isZero)  = c_lower;

        // Now c_tmp is evaluated at m_pyramid[m_levels-1]. Deal with level >= m_levels-1
        dr::masked(out, active & isTri & !isZero & isInf) = c_tmp;

        // EWA
        // TODO: Optimize to one call
        dr::masked(out, active & !isTri) = evalEWA(lower+1, uv, duv_dx, duv_dy, !isTri & active) * alpha + evalEWA(lower, uv, duv_dx, duv_dy, active & !isTri) * (1.f - alpha);

        return out;  // level / m_pyramid.size(); // (lower + 1.f)/ m_pyramid.size();   
    }

    /**
     * \brief Evaluates the texture at the given surface interaction
     *
     * Should only be used when the texture has exactly 3 channels.
     */
    MI_INLINE Color3f interpolate_3(const SurfaceInteraction3f &si,
                                     Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform.transform_affine(si.uv);

        // Get correctly transformed duv/dxy. TODO: Optimization?
        const ScalarMatrix3f uv_tm = m_transform.matrix;

        Vector2f duv_dx = dr::abs(Vector2f{
            uv_tm.entry(0, 0) * si.duv_dx.x() + uv_tm.entry(0, 1) * si.duv_dx.y(),
            uv_tm.entry(1, 0) * si.duv_dx.x() + uv_tm.entry(1, 1) * si.duv_dx.y() 
        });
        Vector2f duv_dy = dr::abs(Vector2f{
            uv_tm.entry(0, 0) * si.duv_dy.x() + uv_tm.entry(0, 1) * si.duv_dy.y(),
            uv_tm.entry(1, 0) * si.duv_dy.x() + uv_tm.entry(1, 1) * si.duv_dy.y() 
        });            

        Color3f out;
        return out;
    }

    /**
     * \brief Recompute mean and 2D sampling distribution (if requested) and pyramid
     * following an update
     */
    void rebuild_internals(bool init_mean, bool init_distr, bool init_pyramid = true) {
        auto&& data = dr::migrate(m_texture.value(), AllocType::Host);

        if constexpr (dr::is_jit_v<Float>)
            dr::sync_thread();

        if (m_transform != ScalarTransform3f())
            dr::make_opaque(m_transform);

        const ScalarFloat *ptr = data.data();

        double mean = 0.0;
        size_t pixel_count = (size_t) dr::prod(resolution());
        bool exceed_unit_range = false;

        const size_t channels = m_texture.shape()[2];
        if (channels == 3) {
            std::unique_ptr<ScalarFloat[]> importance_map(
                init_distr ? new ScalarFloat[pixel_count] : nullptr);

            for (size_t i = 0; i < pixel_count; ++i) {
                ScalarColor3f value = dr::load<ScalarColor3f>(ptr);
                ScalarFloat tmp;
                if (is_spectral_v<Spectrum> && !m_raw ) {
                    tmp = srgb_model_mean(value);
                } else {
                    if (!all(value >= 0 && value <= 1))
                        exceed_unit_range = true;
                    tmp = luminance(value);
                }
                if (init_distr)
                    importance_map[i] = tmp;
                mean += (double) tmp;
                ptr += 3;
            }

            if (init_distr)
                m_distr2d = std::make_unique<DiscreteDistribution2D<Float>>(
                    importance_map.get(), resolution());
        } else {
            for (size_t i = 0; i < pixel_count; ++i) {
                ScalarFloat value = ptr[i];
                if (!(value >= 0 && value <= 1))
                    exceed_unit_range = true;
                mean += (double) value;
            }

            if (init_distr)
                m_distr2d = std::make_unique<DiscreteDistribution2D<Float>>(
                    ptr, resolution());
        }

        if (init_mean)
            m_mean = dr::opaque<Float>(ScalarFloat(mean / pixel_count));

        if (init_pyramid){
            build_pyramid();
        }

        if (exceed_unit_range && !m_raw)
            Log(Warn,
                "MIPMapTexture: texture named \"%s\" contains pixels that "
                "exceed the [0, 1] range!",
                m_name);
    }

    /// Construct 2D distribution upon first access, avoid races
    MI_INLINE void init_distr() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_distr2d) {
            auto self = const_cast<MIPMapTexture *>(this);
            self->rebuild_internals(false, true, false);
        }
    }

    void build_pyramid(){
        // Get levels
        m_levels = 1;
        if (m_mipmap != MIPFilterType::Nearest && m_mipmap != MIPFilterType::Bilinear){
            ScalarVector2u size = ScalarVector2u(m_bitmap->size());
            while(size.x() > 1 || size.y() > 1){
                size.x() = dr::maximum(1, (size.x() + 1) / 2);
                size.y() = dr::maximum(1, (size.y() + 1) / 2);
                ++m_levels;
            }
        }

        // Allocate pyramid
        // TODO: will "init_" cause memory leak? Do I need to delete manually?
        // delete[] m_pyramid.data();
        m_pyramid.init_(m_levels);
        m_res.resize(m_levels);
        m_sizeRatio.resize(m_levels);
        size_t channels = m_bitmap->channel_count();

        // Initialize level 0
        m_res[0] = ScalarVector2u(m_bitmap->size());
        m_sizeRatio[0] = ScalarVector2u(1, 1);

        size_t shape[3] = { (size_t) m_res[0].y(), (size_t) m_res[0].x(), channels };
        m_pyramid[0] = Texture2f(TensorXf(m_bitmap->data(), 3, shape), m_accel, m_accel, m_filter_mode, m_wrap_mode);
        m_texture = Texture2f(TensorXf(m_bitmap->data(), 3, shape), m_accel, m_accel, m_filter_mode, m_wrap_mode);


        // Downsample until 1x1
        if (m_mipmap != MIPFilterType::Nearest && m_mipmap != MIPFilterType::Bilinear){
            ScalarVector2u size = m_res[0];
            m_levels = 1;
            while (size.x() > 1 || size.y() > 1) {
                /* Compute the size of the next downsampled layer */
                size.x() = dr::maximum(1, (size.x() + 1) / 2);
                size.y() = dr::maximum(1, (size.y() + 1) / 2);

                // Resample to be new size; set the minimum value to zero
                // TODO: Resample without bitmap
                m_bitmap = m_bitmap->resample(size, 
                    m_rfilter, 
                    {m_boundary_cond, m_boundary_cond}, 
                    {0, dr::Infinity<mitsuba::Bitmap::ScalarFloat>});

                size_t shape[3] = { (size_t) size.y(), (size_t) size.x(), channels };
                m_pyramid[m_levels] = Texture2f(TensorXf(m_bitmap->data(), 3, shape), m_accel, m_accel, m_filter_mode, m_wrap_mode);
                m_sizeRatio[m_levels] = ScalarVector2f(
                    (ScalarFloat) size.x() / (ScalarFloat) m_res[0].x(),
                    (ScalarFloat) size.y() / (ScalarFloat) m_res[0].y()
                );

                m_res[m_levels] = size;

                // Test if the pyramid is built
                // std::string str = std::to_string(m_levels)+".exr";
                // m_bitmap->write(str);

                ++m_levels;
            }            
        }

        m_weightLut.resize(MI_MIPMAP_LUT_SIZE);
        for (int i=0; i<MI_MIPMAP_LUT_SIZE; ++i) {
            ScalarFloat r2 = (ScalarFloat) i / (ScalarFloat) (MI_MIPMAP_LUT_SIZE-1);
            m_weightLut[i] = dr::exp(-2.0f * r2) - dr::exp(-2.0f);
        }

        std::cout<<"MIPMAP BUILT SUCCESS"<<std::endl;
    }

    Float evalEWA(Int32 level, const Point2f &uv, Vector2f duv_dx, Vector2f duv_dy, Mask active = true) const {
        Float f00, f10, f01, f11;
        dr::Array<Float *, 4> fetch_values;
        fetch_values[0] = &f00;
        fetch_values[1] = &f10;
        fetch_values[2] = &f01;
        fetch_values[3] = &f11;
        Float out = 0;
        Float c_tmp = 0;
        Mask isInf = !dr::isfinite(uv.x()+uv.y());
        // TODO: this eval should be box but not bilinear: use eval_fetch, how to handle the 4 value?? 
        // TODO: if accel
        m_pyramid[m_levels-1].eval_fetch(uv, fetch_values, level >= m_levels & active);
        dr::masked(out, level >= m_levels & active & !isInf)  = f00;

        Float denominator = 0.0f;
        Int32 nSamples = 0;

        
        /* Convert to fractional pixel coordinates on the specified level */
        for(int i = 0; i<m_levels; i++){
            const ScalarVector2u &size = m_res[i];
            Float u = uv.x() * size.x() - 0.5f;
            Float v = uv.y() * size.y() - 0.5f;

            Vector2f duv0(duv_dx.x() * size.x(), duv_dx.y() * size.y());
            Vector2f duv1(duv_dy.x() * size.x(), duv_dy.y() * size.y());

            Float A = duv0[1] * duv0[1] + duv1[1] * duv1[1] + 1,
                B = -2.0f * (duv0[0] * duv0[1] + duv1[0] * duv1[1]),
                C = duv0[0] * duv0[0] + duv1[0] * duv1[0] + 1,
                F = A*C - B*B*0.25f;

            // Float theta = 0.5f * dr::atan(B / (A-C));
            // auto [sinTheta, cosTheta] = dr::sincos(theta);
            // Float a2 = majorRadius*majorRadius,
            //         b2 = minorRadius*minorRadius,
            //         sinTheta2 = sinTheta*sinTheta,
            //         cosTheta2 = cosTheta*cosTheta,
            //         sin2Theta = 2*sinTheta*cosTheta;

            // dr::masked(A, isSkinny) = a2*cosTheta2 + b2*sinTheta2;
            // dr::masked(B, isSkinny) = (a2-b2) * sin2Theta;
            // dr::masked(C, isSkinny) = a2*sinTheta2 + b2*cosTheta2;
            // dr::masked(F, isSkinny) = a2*b2;

            /* Switch to normalized coefficients */
            Float scale = 1.0f / F;
            A *= scale; B *= scale; C *= scale;


            /* Do the same to the ellipse coefficients */
            // const ScalarVector2f ratio = m_sizeRatio[i];
            // dr::masked(A, dr::eq(level, i)) = A / (ratio.x() * ratio.x());
            // dr::masked(B, dr::eq(level, i)) = B / (ratio.x() * ratio.y());
            // dr::masked(C, dr::eq(level, i)) = C / (ratio.y() * ratio.y());

            /* Compute the ellipse's bounding box in texture space */
            Float invDet = 1.0f / (-B*B + 4.0f*A*C),
                deltaU = 2.0f * dr::sqrt(C * invDet),
                deltaV = 2.0f * dr::sqrt(A * invDet);

            Int32 u0 = dr::ceil2int<Int32> (u - deltaU), u1 = dr::floor2int<Int32> (u + deltaU);
            Int32 v0 = dr::ceil2int<Int32> (v - deltaV), v1 = dr::floor2int<Int32> (v + deltaV);

            /* Scale the coefficients by the size of the Gaussian lookup table */
            Float As = A * MI_MIPMAP_LUT_SIZE,
                Bs = B * MI_MIPMAP_LUT_SIZE,
                Cs = C * MI_MIPMAP_LUT_SIZE;
            
            // std::cout<<u0<<" "<<u1<<"  "<<v0<<" "<<v1<<std::endl;
            Int32 vt = dr::minimum(v0, (Int32)v);            
            dr::Loop<Mask> loop_v("Loop v", vt, denominator, out);
            while(loop_v(vt <= v1)){
                const Float vv = vt - v;

                Int32 ut = dr::minimum(u0, (Int32)u);
                dr::Loop<Mask> loop_u("Loop u", ut, denominator, out);
                while(loop_u(ut <= u1)){
                    const Float uu = ut - u;

                    Float q  = (As*uu*uu + (Bs*uu + Cs*vv)*vv);
                    // std::cout<<A<<" "<<B<<" "<<C<<std::endl;
                    // std::cout<<q / MI_MIPMAP_LUT_SIZE<<std::endl;

                    UInt32 qi = dr::minimum((UInt32) q, MI_MIPMAP_LUT_SIZE-1);
                    Float weight = dr::gather<Float>(m_weightLut.data(), qi);
                    m_pyramid[i].eval({Float(ut)/size.x(), Float(vt)/size.y()}, &c_tmp, dr::eq(level, i) & active);
                    // TODO: fetch which texel!!
                    dr::masked(out, dr::eq(level, i) & active) += c_tmp * weight;
                    dr::masked(denominator, dr::eq(i, level) & active) += weight;
                    // std::cout<<vt<<" "<<ut<<" "<<weight<<" "<<q<<std::endl;
                    // std::cout<<out<<std::endl;

                    ut++;  
                }
                vt++;
            }
            // std::cin.get();
        }

        Mask isZero = dr::eq(denominator, 0);
        dr::masked(out, !isZero) = out / denominator; 
        dr::masked(out, isInf) = 0;

        return out;
    }
    
protected:
    Texture2f m_texture;
    ScalarTransform3f m_transform;
    bool m_accel;
    bool m_raw;
    Float m_mean;
    ref<Bitmap> m_bitmap;
    std::string m_name;

    // Optional: distribution for importance sampling
    mutable std::mutex m_mutex;
    std::unique_ptr<DiscreteDistribution2D<Float>> m_distr2d;

    // For reconstructing pyramid
    dr::FilterMode m_filter_mode;
    dr::WrapMode m_wrap_mode;
    FilterBoundaryCondition m_boundary_cond;
    ref<Bitmap::ReconstructionFilter> m_rfilter;

    // Mipmap info
    MIPFilterType m_mipmap;
    dr::DynamicArray<Texture2f> m_pyramid;
    std::vector<ScalarVector2u> m_res;
    int m_levels;

    // For anisotropic filter
    ScalarFloat m_maxAnisotropy;
    std::vector<ScalarFloat> m_weightLut;
    std::vector<ScalarVector2f> m_sizeRatio;
};

MI_IMPLEMENT_CLASS_VARIANT(MIPMapTexture, Texture)
MI_EXPORT_PLUGIN(MIPMapTexture, "mipmap texture")

NAMESPACE_END(mitsuba)
