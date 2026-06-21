#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/distr_2d.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <drjit/tensor.h>
#include <drjit/texture.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/core/fstream.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-envmap:

Environment emitter (:monosp:`envmap`)
--------------------------------------

.. pluginparameters::
 :extra-rows: 4

 * - filename
   - |string|
   - Filename of the radiance-valued input image to be loaded; must be in latitude-longitude format.

 * - bitmap
   - :monosp:`Bitmap object`
   - When creating a Environment emitter at runtime, e.g. from Python or C++,
     an existing Bitmap image instance can be passed directly rather than
     loading it from the filesystem with :paramtype:`filename`.

 * - scale
   - |Float|
   - A scale factor that is applied to the radiance values stored in the input image. (Default: 1.0)
   - |exposed|, |differentiable|

 * - to_world
   - |transform|
   - Specifies an optional emitter-to-world transformation.  (Default: none, i.e. emitter space = world space)
   - |exposed|

 * - mis_compensation
   - |bool|
   - Compensate sampling for the presence of other Monte Carlo techniques that
     will be combined using multiple importance sampling (MIS)? This is
     extremely cheap to do and can slightly reduce variance. (Default: false)

 * - data
   - |tensor|
   - Tensor array containing the radiance-valued data.
   - |exposed|, |differentiable|, |discontinuous|

This plugin provides a HDRI (high dynamic range imaging) environment map,
which is a type of light source that is well-suited for representing "natural"
illumination.

The implementation loads a captured illumination environment from a image in
latitude-longitude format and turns it into an infinitely distant emitter. The
conventions of this mapping are shown in this image:

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/emitter/emitter_envmap_example.jpg
   :caption: The museum environment map by Bernhard Vogl that is used in
             many example renderings in this documentation.
.. subfigure:: ../../resources/data/docs/images/emitter/emitter_envmap_axes.jpg
   :caption: Coordinate conventions used when mapping the input image onto the sphere.
.. subfigend::
   :label: fig-envmap-mapping

The plugin can work with all types of images that are natively supported by Mitsuba
(i.e. JPEG, PNG, OpenEXR, RGBE, TGA, and BMP). In practice, a good environment
map will contain high-dynamic range data that can only be represented using the
OpenEXR or RGBE file formats.
High quality free light probes are available on
`Bernhard Vogl's <http://dativ.at/lightprobes/>`_ website or
`Polyhaven <https://polyhaven.com/hdris>`_.

.. tabs::
    .. code-tab:: xml
        :name: envmap-light

        <emitter type="envmap">
            <string name="filename" value="textures/museum.exr"/>
        </emitter>

    .. code-tab:: python

        'type': 'envmap',
        'filename': 'textures/museum.exr'

 */

template <typename Float, typename Spectrum>
class EnvironmentMapEmitter final : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, m_flags, m_to_world)
    MI_IMPORT_TYPES(Scene, Shape, Texture)

    using Warp = Hierarchical2D<Float, 0>;
    using Tex  = dr::Texture<Float, 2>;

    /* In RGB variants: 3-channel array for R, G, and B components
       In spectral variants: 4-channel array for polynomial coefficients & scale */
    static constexpr uint32_t PixelWidth = is_spectral_v<Spectrum> ? 4 : 3;
    using PixelData = dr::Array<Float, PixelWidth>;
    using ScalarPixelData = dr::Array<ScalarFloat, PixelWidth>;

    EnvironmentMapEmitter(const Properties &props) : Base(props) {
        // Until `set_scene` is called, we have no information about the
        // scene and default to the unit bounding sphere.
        m_bsphere = BoundingSphere3f(ScalarPoint3f(0.f), 1.f);

        ref<Bitmap> bitmap;

        if (props.has_property("bitmap")) {
            // Creates a Bitmap texture directly from an existing Bitmap object
            if (props.has_property("filename"))
                Throw("Cannot specify both \"bitmap\" and \"filename\".");
            // Note: ref-counted, so we don't have to worry about lifetime
            ref<Object> other = props.get<ref<Object>>("bitmap");
            Bitmap *b = dynamic_cast<Bitmap *>(other.get());
            if (!b)
                Throw("Property \"bitmap\" must be a Bitmap instance.");
            bitmap = b;
        } else {
            FileResolver *fs = file_resolver();
            fs::path file_path = fs->resolve(props.get<std::string_view>("filename"));
            m_filename = file_path.filename().string();
            bitmap = new Bitmap(file_path);
        }

        if (bitmap->width() < 2 || bitmap->height() < 3)
            Throw("\"%s\": the environment map resolution must be at least "
                  "2x3 pixels", (m_filename.empty() ? "<Bitmap>" : m_filename));

        // Convert to linear RGBA float bitmap, will undergo further
        // conversion into coefficients of a spectral upsampling model below
        Bitmap::PixelFormat pixel_format = Bitmap::PixelFormat::RGB;
        if constexpr (is_spectral_v<Spectrum>)
            pixel_format = Bitmap::PixelFormat::RGBA;
        bitmap = bitmap->convert(pixel_format, struct_type_v<Float>, false);

        // Allocate a larger image with a one-column halo on each side carrying
        // a copy of the opposite edge so that ``WrapMode::Clamp`` reproduces
        // the periodic boundary exactly.
        m_res = ScalarVector2u((uint32_t) bitmap->width(),
                               (uint32_t) bitmap->height());
        const uint32_t sw = m_res.x() + 2;
        ref<Bitmap> bitmap_2 = new Bitmap(bitmap->pixel_format(),
                                          bitmap->component_format(),
                                          ScalarVector2u(sw, m_res.y()));

        ScalarFloat *in_ptr  = (ScalarFloat *) bitmap->data(),
                    *out_ptr = (ScalarFloat *) bitmap_2->data();

        for (size_t y = 0; y < m_res.y(); ++y) {
            ScalarFloat *row = out_ptr + y * sw * PixelWidth;
            for (size_t x = 0; x < m_res.x(); ++x) {
                ScalarColor3f rgb = dr::load<ScalarVector3f>(in_ptr);

                ScalarPixelData coeff;
                if constexpr (is_monochromatic_v<Spectrum>) {
                    coeff = ScalarPixelData(luminance(rgb));
                } else if constexpr (is_rgb_v<Spectrum>) {
                    coeff = rgb;
                } else {
                    static_assert(is_spectral_v<Spectrum>);
                    // Evaluate the spectral upsampling model. This requires a
                    // reflectance value (colors in [0, 1]) which is accomplished
                    // here by scaling. We use a color where the highest component
                    // is 50%, which generally yields a fairly smooth spectrum.
                    ScalarFloat scale = dr::max(rgb) * 2.f;
                    ScalarColor3f rgb_norm = rgb / dr::maximum(1e-8f, scale);
                    coeff = dr::concat((ScalarColor3f) srgb_model_fetch(rgb_norm),
                                       dr::Array<ScalarFloat, 1>(scale));
                }

                // Real column x is stored at column x+1
                dr::store(row + (x + 1) * PixelWidth, coeff);
                in_ptr += PixelWidth;
            }
        }

        refresh_halo((ScalarFloat *) bitmap_2->data(), m_res);

        size_t shape[3] = { (size_t) m_res.y(), (size_t) sw, (size_t) PixelWidth };
        TensorXf tensor(bitmap_2->data(), 3, shape);
        m_texture = Tex(tensor, /* use_accel = */ true,
                        /* migrate = */ dr::is_jit_v<Float>,
                        dr::FilterMode::Linear, dr::WrapMode::Clamp);

        m_scale = props.get<ScalarFloat>("scale", 1.f);
        m_mis_compensation = props.get<bool>("mis_compensation", false);
        m_d65 = Texture::D65(1.f);
        m_flags = EmitterFlags::Infinite | EmitterFlags::SpatiallyVarying;

        rebuild_distribution((ScalarFloat *) bitmap_2->data());
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("scale",     m_scale,               ParamFlags::Differentiable);
        cb->put("data",      m_texture.tensor(),    ParamFlags::Differentiable | ParamFlags::Discontinuous);
        cb->put("to_world",  m_to_world,            ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys = {}) override {
        if (keys.empty() || string::contains(keys, "data")) {
            TensorXf &tensor = m_texture.tensor();

            if (tensor.ndim() != 3)
                Throw("Environment map data has dimension %lu, expected 3", tensor.ndim());

            if constexpr (is_spectral_v<Spectrum>) {
                if (tensor.shape(2) != 4)
                    Throw("Environment map data has %lu channels, expected 4", tensor.shape(2));
            } else {
                if (tensor.shape(2) != 3)
                    Throw("Environment map data has %lu channels, expected 3", tensor.shape(2));
            }

            m_res = ScalarVector2u((uint32_t) tensor.shape(1) - 2,
                                   (uint32_t) tensor.shape(0));
            const uint32_t sw = m_res.x() + 2;

            // The user may have edited the real columns, so refresh the halo
            // that makes ``WrapMode::Clamp`` periodic in phi (col 0 <- last real
            // col, col W+1 <- first). Scatter into a copy so the exposed leaf
            // stays clean; the AD-tracked scatter then routes each halo column's
            // gradient onto its source real column.
            if constexpr (dr::is_jit_v<Float>) {
                auto &array = tensor.array();
                Float corrected = array; // copy-on-write; the scatters fork it

                UInt32 row = dr::arange<UInt32>(m_res.y()) * sw;
                dr::scatter(corrected, dr::gather<PixelData>(array, row + m_res.x()), row);
                dr::scatter(corrected, dr::gather<PixelData>(array, row + 1u),
                            row + (m_res.x() + 1u));

                size_t shape[3] = { (size_t) m_res.y(), (size_t) sw, (size_t) PixelWidth };
                m_texture.set_tensor(TensorXf(corrected, 3, shape), /* migrate */ true);
            } else {
                refresh_halo((ScalarFloat *) tensor.array().data(), m_res);
                m_texture.update_inplace();
            }

            auto&& data = dr::migrate(m_texture.tensor().array(), JitBackend::None);
            if constexpr (dr::is_jit_v<Float>)
                dr::sync_thread();

            rebuild_distribution((ScalarFloat *) data.data());
        }
        Base::parameters_changed(keys);
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

        Vector3f v = m_to_world.value().inverse() * (-si.wi);

        Point2f uv = direction_to_uv(v);

        return depolarizer<Spectrum>(eval_spectrum(uv, si.wavelengths, active));
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &sample2,
                                          const Point2f &sample3,
                                          Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample spatial component
        Point2f offset = warp::square_to_uniform_disk_concentric(sample2);

        // 2. Sample directional component
        auto [uv, pdf] = m_warp.sample(sample3, nullptr, active);
        uv.x() += half_texel();

        active &= pdf > 0.f;

        Float inv_sin_theta;
        Vector3f d = uv_to_direction(uv, inv_sin_theta);
        pdf *= inv_sin_theta * dr::InvTwoPi<Float> * dr::InvPi<Float>;

        // Unlike \ref sample_direction, ray goes from the envmap toward the scene
        Vector3f d_global = m_to_world.value() * -d;

        // Compute ray origin
        Vector3f perpendicular_offset =
            Frame3f(d).to_world(Vector3f(offset.x(), offset.y(), 0));
        Point3f origin =
            m_bsphere.center + (perpendicular_offset - d_global) * m_bsphere.radius;

        // 3. Sample spectral component (weight accounts for radiance)
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.t    = 0.f;
        si.time = time;
        si.p    = origin;
        si.uv   = uv;
        auto [wavelengths, weight] =
            sample_wavelengths(si, wavelength_sample, active);

        Float r2 = dr::square(m_bsphere.radius);
        Ray3f ray(origin, d_global, time, wavelengths);
        weight *= dr::Pi<Float> * r2 / pdf;

        return std::make_pair(ray, weight & active);
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample,
                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        auto [uv, pdf] = m_warp.sample(sample, nullptr, active);
        uv.x() += half_texel();
        active &= pdf > 0.f;

        Float inv_sin_theta;
        Vector3f d = uv_to_direction(uv, inv_sin_theta);

        // Needed when the reference point is on the sensor, which is not part of the bbox
        Float radius = dr::maximum(m_bsphere.radius, dr::norm(it.p - m_bsphere.center));
        Float dist = 2.f * radius;

        d = m_to_world.value() * d;

        DirectionSample3f ds;
        ds.p       = it.p + d * dist;
        ds.n       = -d;
        ds.uv      = uv;
        ds.time    = it.time;
        ds.pdf     = dr::select(
            active,
            pdf * inv_sin_theta * (1.f / (2.f * dr::square(dr::Pi<Float>))),
            0.f
        );
        ds.delta   = false;
        ds.emitter = this;
        ds.d       = d;
        ds.dist    = dist;

        auto weight =
            depolarizer<Spectrum>(eval_spectrum(uv, it.wavelengths, active)) /
            ds.pdf;

        return { ds, weight & active };
    }

    Float pdf_direction(const Interaction3f & /*it*/, const DirectionSample3f &ds,
                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        Vector3f d = m_to_world.value().inverse() * ds.d;

        Point2f uv = direction_to_uv(d);
        uv.x() -= half_texel();
        uv -= dr::floor(uv);

        Float inv_sin_theta = dr::safe_rsqrt(dr::maximum(
            dr::square(d.x()) + dr::square(d.z()), dr::square(dr::Epsilon<Float>)));

        return m_warp.eval(uv) * inv_sin_theta * (1.f / (2.f * dr::square(dr::Pi<Float>)));
    }

    Spectrum eval_direction(const Interaction3f &it,
                            const DirectionSample3f &ds,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        return eval_spectrum(ds.uv, it.wavelengths, active);
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        auto [wavelengths, weight] = m_d65->sample_spectrum(
            si, math::sample_shifted<Wavelength>(sample), active);

        return { wavelengths,
                 weight * eval_spectrum(si.uv, wavelengths, active, false) };
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float /*time*/, const Point2f & /*sample*/,
                    Mask /*active*/) const override {
        if constexpr (dr::is_jit_v<Float>) {
            // Do not throw an exception in JIT-compiled variants. This function
            // might be invoked by DrJit's virtual function call recording
            // mechanism despite not influencing any actual calculation.
            return { dr::zeros<PositionSample3f>(), dr::NaN<Float> };
        } else {
            NotImplementedError("sample_position");
        }
    }

    ScalarBoundingBox3f bbox() const override {
        // This emitter does not occupy any particular region of space, return
        // an invalid bounding box
        return ScalarBoundingBox3f();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "EnvironmentMapEmitter[" << std::endl;
        if (!m_filename.empty())
            oss << "  filename = \"" << m_filename << "\"," << std::endl;
        oss << "  res = \"" << m_res << "\"," << std::endl
            << "  bsphere = " << string::indent(m_bsphere) << std::endl
            << "]";
        return oss.str();
    }

protected:
    /// Half-texel shift aligning the sample warp grid with the texture.
    /// The warp grid has ``W + 1`` columns, so this is ``0.5 / W``.
    ScalarFloat half_texel() const { return .5f / m_res.x(); }

    /// Convert a latitude-longitude UV coordinate into a local-frame direction,
    /// also returning the spherical-coordinate Jacobian term 1 / sin(theta).
    Vector3f uv_to_direction(const Point2f &uv, Float &inv_sin_theta) const {
        Float theta = uv.y() * dr::Pi<Float>,
              phi   = uv.x() * dr::TwoPi<Float>;
        auto [sin_theta, cos_theta] = dr::sincos(theta);
        auto [sin_phi,   cos_phi]   = dr::sincos(phi);
        inv_sin_theta = dr::rcp(dr::maximum(sin_theta, dr::Epsilon<Float>));
        return Vector3f(sin_phi * sin_theta, cos_theta, -cos_phi * sin_theta);
    }

    /// Inverse of \ref uv_to_direction (latitude-longitude texture coordinates)
    Point2f direction_to_uv(const Vector3f &d) const {
        return Point2f(dr::atan2(d.x(), -d.z()) * dr::InvTwoPi<Float>,
                       dr::safe_acos(d.y()) * dr::InvPi<Float>);
    }

    /// Copy the real edge columns into the halo columns of a host-resident
    /// (H, W+2, N) buffer: col 0 <- last real col, col W+1 <- first real col.
    static void refresh_halo(ScalarFloat *ptr, const ScalarVector2u &res) {
        const uint32_t sw = res.x() + 2;
        for (size_t y = 0; y < res.y(); ++y) {
            ScalarFloat *r = ptr + (size_t) y * sw * PixelWidth;
            dr::store(r, dr::load<ScalarPixelData>(r + res.x() * PixelWidth));
            dr::store(r + (res.x() + 1) * PixelWidth,
                      dr::load<ScalarPixelData>(r + PixelWidth));
        }
    }

    /// Rebuild a width W+1 luminance warp from the width W+2 host texture
    /// storage applying optional MIS compensation and sin(theta) weighting.
    void rebuild_distribution(const ScalarFloat *data) {
        const uint32_t sw = m_res.x() + 2;                  // texture storage stride
        const ScalarVector2u res(m_res.x() + 1, m_res.y()); // warp grid (W+1, H)
        size_t n = (size_t) res.x() * res.y();

        std::unique_ptr<ScalarFloat[]> luminance_data(new ScalarFloat[n]);
        ScalarFloat *lum_ptr = luminance_data.get();

        for (size_t y = 0; y < res.y(); ++y) {
            for (size_t x = 0; x < res.x(); ++x) {
                // Warp column x corresponds to storage column x+1
                ScalarPixelData coeff = dr::load<ScalarPixelData>(
                    data + ((size_t) y * sw + (x + 1)) * PixelWidth);
                ScalarFloat lum;
                if constexpr (is_monochromatic_v<Spectrum>)
                    lum = coeff.x();
                else if constexpr (is_rgb_v<Spectrum>)
                    lum = luminance(ScalarColor3f(coeff));
                else
                    lum = srgb_model_mean(dr::head<3>(coeff)) * coeff.w();
                lum_ptr[y * res.x() + x] = lum;
            }
        }

        // "MIS Compensation: Optimizing Sampling Techniques in Multiple
        // Importance Sampling" Ondrej Karlik, Martin Sik, Petr Vivoda, Tomas
        // Skrivan, and Jaroslav Krivanek. SIGGRAPH Asia 2019
        ScalarFloat offset = 0.f;
        if (m_mis_compensation) {
            ScalarFloat min_lum = dr::Infinity<ScalarFloat>;
            double lum_accum_d = 0.0;

            // Skip the padded periodic column (last column mirrors the first)
            for (size_t y = 0; y < res.y(); ++y) {
                for (size_t x = 0; x < res.x() - 1u; ++x) {
                    ScalarFloat lum = lum_ptr[y * res.x() + x];
                    min_lum = dr::minimum(min_lum, lum);
                    lum_accum_d += (double) lum;
                }
            }

            offset = ScalarFloat(lum_accum_d / ((res.x() - 1u) * (size_t) res.y()));

            // Be wary of constant environment maps: average and minimum should
            // be sufficiently different
            if (offset - min_lum <= 0.01f * offset)
                offset = 0.f; // disable
        }

        ScalarFloat theta_scale = 1.f / (res.y() - 1) * dr::Pi<Float>;
        for (size_t y = 0; y < res.y(); ++y) {
            ScalarFloat sin_theta = dr::sin(y * theta_scale);
            for (size_t x = 0; x < res.x(); ++x) {
                ScalarFloat &lum = lum_ptr[y * res.x() + x];
                lum = dr::maximum(lum - offset, 0.f) * sin_theta;
            }
        }

        m_warp = Warp(luminance_data.get(), res);
    }

    UnpolarizedSpectrum eval_spectrum(Point2f uv, const Wavelength &wavelengths,
                                      Mask active, bool include_whitepoint = true) const {
        Vector2f res(m_res); // real (W, H) as floats

        // Texel-centered phi over the W real columns (offset by one to skip the
        // leading halo column), align-corners theta over the H rows. The
        // texture's ``pos = uv * res - 0.5`` absorbs the half-texel offset, and
        // ``WrapMode::Clamp`` plus the halo give periodic phi / clamped theta.
        Float u = uv.x() - dr::floor(uv.x()),       // wrap phi into [0, 1)
              v = dr::clip(uv.y(), 0.f, 1.f);
        Point2f pos(dr::fmadd(u, res.x(), 1.f) / (res.x() + 2.f),
                    dr::fmadd(v, res.y() - 1.f, 0.5f) / res.y());

        if constexpr (is_spectral_v<Spectrum>) {
            // The spectral upsampling model is nonlinear. Use ``eval_fetch`` to
            // get the coefficients at the four corners so that we can evaluate
            // the model and interpolate ourselves.
            dr::Array<PixelData, 4> corners =
                m_texture.template eval_fetch<PixelData>(pos, active);
            const PixelData &c00 = corners[0], &c10 = corners[1],
                            &c01 = corners[2], &c11 = corners[3];

            // Weights from ``eval_fetch``'s own ``pos_f = uv * res - 0.5``
            Point2f pos_f = dr::fmadd(pos, Point2f(res.x() + 2.f, res.y()), -.5f);
            Point2f w1 = pos_f - dr::floor(pos_f),
                    w0 = 1.f - w1;

            UnpolarizedSpectrum s00, s10, s01, s11, s0, s1, s;
            Float f0, f1, f;

            s00 = srgb_model_eval<UnpolarizedSpectrum>(dr::head<3>(c00), wavelengths);
            s10 = srgb_model_eval<UnpolarizedSpectrum>(dr::head<3>(c10), wavelengths);
            s01 = srgb_model_eval<UnpolarizedSpectrum>(dr::head<3>(c01), wavelengths);
            s11 = srgb_model_eval<UnpolarizedSpectrum>(dr::head<3>(c11), wavelengths);

            s0  = dr::fmadd(w0.x(), s00, w1.x() * s10);
            s1  = dr::fmadd(w0.x(), s01, w1.x() * s11);
            f0  = dr::fmadd(w0.x(), c00.w(), w1.x() * c10.w());
            f1  = dr::fmadd(w0.x(), c01.w(), w1.x() * c11.w());

            s   = dr::fmadd(w0.y(), s0, w1.y() * s1);
            f   = dr::fmadd(w0.y(), f0, w1.y() * f1);

            UnpolarizedSpectrum result = s * f * m_scale;

            if (include_whitepoint) {
                SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
                si.wavelengths = wavelengths;
                result *= m_d65->eval(si, active);
            }

            return result;
        } else {
            DRJIT_MARK_USED(wavelengths);
            // RGB / mono store linearly-interpolatable values, so the hardware
            // bilinear lookup matches the original math exactly.
            Color3f v_rgb = m_texture.template eval<Color3f>(pos, active);

            if constexpr (is_monochromatic_v<Spectrum>)
                return dr::head<1>(v_rgb) * m_scale;
            else
                return v_rgb * m_scale;
        }
    }

    MI_DECLARE_CLASS(EnvironmentMapEmitter)
protected:
    std::string m_filename;
    BoundingSphere3f m_bsphere;
    Tex m_texture;
    ScalarVector2u m_res;
    Warp m_warp;
    ref<Texture> m_d65;
    Float m_scale;
    bool m_mis_compensation;

    MI_TRAVERSE_CB(Base, m_bsphere, m_texture, m_warp, m_d65, m_scale)
};

MI_EXPORT_PLUGIN(EnvironmentMapEmitter)
NAMESPACE_END(mitsuba)
