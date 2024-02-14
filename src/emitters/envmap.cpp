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

    /* In RGB variants: 3-channel array for R, G, and B components
       In spectral variants: 4-channel array for polynomial coefficients & scale */
    using PixelData = dr::Array<Float, is_spectral_v<Spectrum> ? 4 : 3>;
    using ScalarPixelData = dr::Array<ScalarFloat, is_spectral_v<Spectrum> ? 4 : 3>;

    EnvironmentMapEmitter(const Properties &props) : Base(props) {
        /* Until `set_scene` is called, we have no information
           about the scene and default to the unit bounding sphere. */
        m_bsphere = BoundingSphere3f(ScalarPoint3f(0.f), 1.f);

        ref<Bitmap> bitmap;

        if (props.has_property("bitmap")) {
            // Creates a Bitmap texture directly from an existing Bitmap object
            if (props.has_property("filename"))
                Throw("Cannot specify both \"bitmap\" and \"filename\".");
            // Note: ref-counted, so we don't have to worry about lifetime
            ref<Object> other = props.object("bitmap");
            Bitmap *b = dynamic_cast<Bitmap *>(other.get());
            if (!b)
                Throw("Property \"bitmap\" must be a Bitmap instance.");
            bitmap = b;
        } else {
            FileResolver *fs = Thread::thread()->file_resolver();
            fs::path file_path = fs->resolve(props.string("filename"));
            m_filename = file_path.filename().string();
            bitmap = new Bitmap(file_path);
        }

        if (bitmap->width() < 2 || bitmap->height() < 3)
            Throw("\"%s\": the environment map resolution must be at least "
                  "2x3 pixels", (m_filename.empty() ? "<Bitmap>" : m_filename));

        /* Convert to linear RGBA float bitmap, will undergo further
           conversion into coefficients of a spectral upsampling model below */
        Bitmap::PixelFormat pixel_format = Bitmap::PixelFormat::RGB;
        if constexpr (is_spectral_v<Spectrum>)
            pixel_format = Bitmap::PixelFormat::RGBA;
        bitmap = bitmap->convert(pixel_format, struct_type_v<Float>, false);

        /* Allocate a larger image including an extra column to
           account for the periodic boundary */
        ScalarVector2u res(bitmap->width() + 1, bitmap->height());
        ref<Bitmap> bitmap_2 = new Bitmap(bitmap->pixel_format(),
                                          bitmap->component_format(), res);

        // Luminance image used for importance sampling
        std::unique_ptr<ScalarFloat[]> luminance(new ScalarFloat[dr::prod(res)]);

        ScalarFloat *in_ptr  = (ScalarFloat *) bitmap->data(),
                    *out_ptr = (ScalarFloat *) bitmap_2->data(),
                    *lum_ptr = (ScalarFloat *) luminance.get();

        ScalarFloat theta_scale = 1.f / (bitmap->size().y() - 1) * dr::Pi<Float>;

        /* "MIS Compensation: Optimizing Sampling Techniques in Multiple
           Importance Sampling" Ondrej Karlik, Martin Sik, Petr Vivoda, Tomas
           Skrivan, and Jaroslav Krivanek. SIGGRAPH Asia 2019 */
        ScalarFloat luminance_offset = 0.f;
        if (props.get<bool>("mis_compensation", false)) {
            ScalarFloat min_lum = 0.f;
            double lum_accum_d = 0.0;

            for (size_t y = 0; y < bitmap->size().y(); ++y) {
                for (size_t x = 0; x < bitmap->size().x(); ++x) {
                    ScalarColor3f rgb = dr::load<ScalarVector3f>(in_ptr);
                    ScalarFloat lum = mitsuba::luminance(rgb);
                    min_lum = dr::minimum(min_lum, lum);
                    lum_accum_d += (double) lum;
                    in_ptr += 4;
                }
            }
            in_ptr = (ScalarFloat *) bitmap->data();

            luminance_offset = ScalarFloat(lum_accum_d / dr::prod(bitmap->size()));

            /* Be wary of constant environment maps: average and minimum
               should be sufficiently different */
            if (luminance_offset - min_lum <= 0.01f * luminance_offset)
                luminance_offset = 0.f; // disable
        }

        size_t pixel_width = is_spectral_v<Spectrum> ? 4 : 3;
        for (size_t y = 0; y < bitmap->size().y(); ++y) {
            ScalarFloat sin_theta = dr::sin(y * theta_scale);

            for (size_t x = 0; x < bitmap->size().x(); ++x) {
                ScalarColor3f rgb = dr::load<ScalarVector3f>(in_ptr);

                ScalarFloat lum = mitsuba::luminance(rgb);

                ScalarPixelData coeff;
                if constexpr (is_monochromatic_v<Spectrum>) {
                    coeff = ScalarPixelData(lum);
                } else if constexpr (is_rgb_v<Spectrum>) {
                    coeff = rgb;
                } else {
                    static_assert(is_spectral_v<Spectrum>);
                    /* Evaluate the spectral upsampling model. This requires a
                       reflectance value (colors in [0, 1]) which is accomplished here by
                       scaling. We use a color where the highest component is 50%,
                       which generally yields a fairly smooth spectrum. */
                    ScalarFloat scale = dr::max(rgb) * 2.f;
                    ScalarColor3f rgb_norm = rgb / dr::maximum(1e-8f, scale);
                    coeff = dr::concat((ScalarColor3f) srgb_model_fetch(rgb_norm),
                                       dr::Array<ScalarFloat, 1>(scale));
                }

                lum = dr::maximum(lum - luminance_offset, 0.f);

                *lum_ptr++ = lum * sin_theta;
                dr::store(out_ptr, coeff);
                in_ptr += pixel_width;
                out_ptr += pixel_width;
            }

            // Last column of pixels mirrors first
            ScalarFloat temp = *(lum_ptr - bitmap->size().x());
            *lum_ptr++ = temp;
            dr::store(out_ptr, dr::load<ScalarPixelData>(
                                   out_ptr - bitmap->size().x() * pixel_width));
            out_ptr += pixel_width;
        }

        size_t shape[3] = { (size_t) res.y(), (size_t) res.x(), pixel_width };
        m_data = TensorXf(bitmap_2->data(), 3, shape);

        m_scale = props.get<ScalarFloat>("scale", 1.f);
        m_warp = Warp(luminance.get(), res);
        m_d65 = Texture::D65(1.f);
        m_flags = EmitterFlags::Infinite | EmitterFlags::SpatiallyVarying;
    }

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
        callback->put_parameter("scale",     m_scale,          +ParamFlags::Differentiable);
        callback->put_parameter("data",      m_data,            ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_parameter("to_world", *m_to_world.ptr(), +ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys = {}) override {
        if (keys.empty() || string::contains(keys, "data")) {

            ScalarVector2u res = { m_data.shape(1), m_data.shape(0) };

            if constexpr (dr::is_jit_v<Float>) {
                // Enforce horizontal continuity
                UInt32 row_index = dr::arange<UInt32>(res.y()) * res.x();
                PixelData v0 = dr::gather<PixelData>(m_data.array(), row_index);
                PixelData v1 = dr::gather<PixelData>(m_data.array(), row_index + (res.x() - 1));
                PixelData v01 = .5f * (v0 + v1);
                dr::scatter(m_data.array(), v01, row_index);
                dr::scatter(m_data.array(), v01, row_index + (res.x() - 1));
            }

            auto&& data = dr::migrate(m_data.array(), AllocType::Host);

            if constexpr (dr::is_jit_v<Float>)
                dr::sync_thread();

            std::unique_ptr<ScalarFloat[]> luminance(
                new ScalarFloat[dr::prod(res)]);

            ScalarFloat *ptr     = (ScalarFloat *) data.data(),
                        *lum_ptr = (ScalarFloat *) luminance.get();

            size_t pixel_width = is_spectral_v<Spectrum> ? 4 : 3;
            constexpr bool is_aligned = ScalarPixelData::Size == 4;

            ScalarFloat theta_scale = 1.f / (res.y() - 1) * dr::Pi<Float>;
            for (size_t y = 0; y < res.y(); ++y) {
                ScalarFloat sin_theta = dr::sin(y * theta_scale);

                if constexpr (!dr::is_jit_v<Float>) {
                    // Enforce horizontal continuity
                    ScalarFloat *ptr2 = ptr + pixel_width * (res.x() - 1);
                    ScalarPixelData v0, v1;
                    if constexpr (is_aligned) {
                        v0  = dr::load_aligned<ScalarPixelData>(ptr);
                        v1  = dr::load_aligned<ScalarPixelData>(ptr2);
                    } else {
                        v0  = dr::load<ScalarPixelData>(ptr);
                        v1  = dr::load<ScalarPixelData>(ptr2);
                    }
                    ScalarPixelData v01 = .5f * (v0 + v1);

                    if constexpr (is_aligned) {
                        dr::store_aligned(ptr, v01),
                        dr::store_aligned(ptr2, v01);
                    } else {
                        dr::store(ptr, v01),
                        dr::store(ptr2, v01);
                    }
                }

                for (size_t x = 0; x < res.x(); ++x) {
                    ScalarPixelData coeff;
                    if constexpr (is_aligned)
                        coeff = dr::load_aligned<ScalarPixelData>(ptr);
                    else
                        coeff = dr::load<ScalarPixelData>(ptr);

                    ScalarFloat lum;
                    if constexpr (is_monochromatic_v<Spectrum>) {
                        lum = coeff.x();
                    } else if constexpr (is_rgb_v<Spectrum>) {
                        lum = mitsuba::luminance(ScalarColor3f(coeff));
                    } else {
                        static_assert(is_spectral_v<Spectrum>);
                        lum = srgb_model_mean(dr::head<3>(coeff)) * coeff.w();
                    }

                    *lum_ptr++ = lum * sin_theta;
                    ptr += pixel_width;
                }
            }

            m_warp = Warp(luminance.get(), res);
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

        Vector3f v = m_to_world.value().inverse().transform_affine(-si.wi);

        // Convert to latitude-longitude texture coordinates
        Point2f uv = Point2f(dr::atan2(v.x(), -v.z()) * dr::InvTwoPi<Float>,
                             dr::safe_acos(v.y()) * dr::InvPi<Float>);

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
        uv.x() += .5f / (m_data.shape(1) - 1);

        active &= pdf > 0.f;

        Float theta = uv.y() * dr::Pi<Float>,
              phi   = uv.x() * dr::TwoPi<Float>;

        Vector3f d = dr::sphdir(theta, phi);
        d = Vector3f(d.y(), d.z(), -d.x());

        Float inv_sin_theta = dr::safe_rsqrt(dr::square(d.x()) + dr::square(d.z()));
        pdf *= inv_sin_theta * dr::InvTwoPi<Float> * dr::InvPi<Float>;

        // Unlike \ref sample_direction, ray goes from the envmap toward the scene
        Vector3f d_global = m_to_world.value().transform_affine(-d);

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
        uv.x() += .5f / (m_data.shape(1) - 1);
        active &= pdf > 0.f;

        Float theta = uv.y() * dr::Pi<Float>,
              phi   = uv.x() * dr::TwoPi<Float>;

        Vector3f d = dr::sphdir(theta, phi);
        d = Vector3f(d.y(), d.z(), -d.x());

        // Needed when the reference point is on the sensor, which is not part of the bbox
        Float radius = dr::maximum(m_bsphere.radius, dr::norm(it.p - m_bsphere.center));
        Float dist = 2.f * radius;

        Float inv_sin_theta = dr::safe_rsqrt(dr::maximum(
            dr::square(d.x()) + dr::square(d.z()), dr::square(dr::Epsilon<Float>)));

        d = m_to_world.value().transform_affine(d);

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

        Vector3f d = m_to_world.value().inverse().transform_affine(ds.d);

        // Convert to latitude-longitude texture coordinates
        Point2f uv = Point2f(dr::atan2(d.x(), -d.z()) * dr::InvTwoPi<Float>,
                             dr::safe_acos(d.y()) * dr::InvPi<Float>);
        uv.x() -= .5f / (m_data.shape(1) - 1u);
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
            /* Do not throw an exception in JIT-compiled variants. This
               function might be invoked by DrJit's virtual function call
               recording mechanism despite not influencing any actual
               calculation. */
            return { dr::zeros<PositionSample3f>(), dr::NaN<Float> };
        } else {
            NotImplementedError("sample_position");
        }
    }

    ScalarBoundingBox3f bbox() const override {
        /* This emitter does not occupy any particular region
           of space, return an invalid bounding box */
        return ScalarBoundingBox3f();
    }

    std::string to_string() const override {
        ScalarVector2u res = { m_data.shape(1), m_data.shape(0) };
        std::ostringstream oss;
        oss << "EnvironmentMapEmitter[" << std::endl;
        if (!m_filename.empty())
            oss << "  filename = \"" << m_filename << "\"," << std::endl;
        oss << "  res = \"" << res << "\"," << std::endl
            << "  bsphere = " << string::indent(m_bsphere) << std::endl
            << "]";
        return oss.str();
    }

protected:
    UnpolarizedSpectrum eval_spectrum(Point2f uv, const Wavelength &wavelengths,
                                      Mask active, bool include_whitepoint = true) const {
        ScalarVector2u res = { m_data.shape(1), m_data.shape(0) };

        uv.x() -= .5f / (res.x() - 1u);
        uv -= dr::floor(uv);
        uv *= Vector2f(res - 1u);

        Point2u pos = dr::minimum(Point2u(uv), res - 2u);

        Point2f w1 = uv - Point2f(pos),
                w0 = 1.f - w1;

        const uint32_t width = res.x();
        UInt32 index = dr::fmadd(pos.y(), width, pos.x());

        PixelData v00 = dr::gather<PixelData>(m_data.array(), index, active),
                  v10 = dr::gather<PixelData>(m_data.array(), index + 1, active),
                  v01 = dr::gather<PixelData>(m_data.array(), index + width, active),
                  v11 = dr::gather<PixelData>(m_data.array(), index + width + 1, active);

        if constexpr (is_spectral_v<Spectrum>) {
            UnpolarizedSpectrum s00, s10, s01, s11, s0, s1, s;
            Float f0, f1, f;

            s00 = srgb_model_eval<UnpolarizedSpectrum>(dr::head<3>(v00), wavelengths);
            s10 = srgb_model_eval<UnpolarizedSpectrum>(dr::head<3>(v10), wavelengths);
            s01 = srgb_model_eval<UnpolarizedSpectrum>(dr::head<3>(v01), wavelengths);
            s11 = srgb_model_eval<UnpolarizedSpectrum>(dr::head<3>(v11), wavelengths);

            s0  = dr::fmadd(w0.x(), s00, w1.x() * s10);
            s1  = dr::fmadd(w0.x(), s01, w1.x() * s11);
            f0  = dr::fmadd(w0.x(), v00.w(), w1.x() * v10.w());
            f1  = dr::fmadd(w0.x(), v01.w(), w1.x() * v11.w());

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
            PixelData v0 = dr::fmadd(w0.x(), v00, w1.x() * v10),
                      v1 = dr::fmadd(w0.x(), v01, w1.x() * v11),
                      v  = dr::fmadd(w0.y(), v0, w1.y() * v1);

            if constexpr (is_monochromatic_v<Spectrum>)
                return dr::head<1>(v) * m_scale;
            else
                return v * m_scale;
        }
    }

    MI_DECLARE_CLASS()
protected:
    std::string m_filename;
    BoundingSphere3f m_bsphere;
    TensorXf m_data;
    Warp m_warp;
    ref<Texture> m_d65;
    Float m_scale;
};

MI_IMPLEMENT_CLASS_VARIANT(EnvironmentMapEmitter, Emitter)
MI_EXPORT_PLUGIN(EnvironmentMapEmitter, "Environment map emitter")
NAMESPACE_END(mitsuba)
