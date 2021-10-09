#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/distr_2d.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <enoki/tensor.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-envmap:

Environment emitter (:monosp:`envmap`)
--------------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the radiance-valued input image to be loaded; must be in latitude-longitude format.

 * - scale
   - |Float|
   - A scale factor that is applied to the radiance values stored in the input image. (Default: 1.0)

 * - to_world
   - |transform|
   - Specifies an optional emitter-to-world transformation.  (Default: none, i.e. emitter space = world space)

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
`Paul Debevec's <http://gl.ict.usc.edu/Data/HighResProbes>`_ and
`Bernhard Vogl's <http://dativ.at/lightprobes/>`_ websites.

 */

template <typename Float, typename Spectrum>
class EnvironmentMapEmitter final : public Emitter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Emitter, m_flags, m_to_world)
    MTS_IMPORT_TYPES(Scene, Shape, Texture)

    using Warp = Hierarchical2D<Float, 0>;

    EnvironmentMapEmitter(const Properties &props) : Base(props) {
        /* Until `set_scene` is called, we have no information
           about the scene and default to the unit bounding sphere. */
        m_bsphere = ScalarBoundingSphere3f(ScalarPoint3f(0.f), 1.f);

        FileResolver *fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));

        ref<Bitmap> bitmap = new Bitmap(file_path);

        /* Convert to linear RGBA float bitmap, will undergo further
           conversion into coefficients of a spectral upsampling model below */
        bitmap = bitmap->convert(Bitmap::PixelFormat::RGBA, struct_type_v<ScalarFloat>, false);
        m_filename = file_path.filename().string();

        std::unique_ptr<ScalarFloat[]> luminance(new ScalarFloat[bitmap->pixel_count()]);

        ScalarFloat *ptr     = (ScalarFloat *) bitmap->data(),
                    *lum_ptr = (ScalarFloat *) luminance.get();

        for (size_t y = 0; y < bitmap->size().y(); ++y) {
            ScalarFloat sin_theta =
                ek::sin(y / ScalarFloat(bitmap->size().y() - 1) * ek::Pi<ScalarFloat>);

            for (size_t x = 0; x < bitmap->size().x(); ++x) {
                ScalarColor3f rgb = ek::load<ScalarVector3f>(ptr);
                ScalarFloat lum   = mitsuba::luminance(rgb);

                ScalarVector4f coeff;
                if constexpr (is_monochromatic_v<Spectrum>) {
                    coeff = ScalarVector4f(lum, lum, lum, 1.f);
                } else if constexpr (is_rgb_v<Spectrum>) {
                    coeff = ek::concat(rgb, ek::Array<ScalarFloat, 1>(1.f));
                } else {
                    static_assert(is_spectral_v<Spectrum>);
                    /* Evaluate the spectral upsampling model. This requires a
                       reflectance value (colors in [0, 1]) which is accomplished here by
                       scaling. We use a color where the highest component is 50%,
                       which generally yields a fairly smooth spectrum. */
                    ScalarFloat scale = ek::hmax(rgb) * 2.f;
                    ScalarColor3f rgb_norm = rgb / ek::max((ScalarFloat) 1e-8, scale);
                    coeff = ek::concat((ScalarColor3f) srgb_model_fetch(rgb_norm),
                                       ek::Array<ScalarFloat, 1>(scale));
                }

                *lum_ptr++ = lum * sin_theta;
                ek::store(ptr, coeff);
                ptr += 4;
            }
        }

        ScalarVector2u res = bitmap->size();
        size_t shape[3] = { (size_t) res.y(), (size_t) res.x(), 4 };
        m_data = TensorXf(bitmap->data(), 3, shape);

        m_scale = props.float_("scale", 1.f);
        m_warp = Warp(luminance.get(), res);
        m_d65 = Texture::D65(1.f);
        m_flags = EmitterFlags::Infinite | EmitterFlags::SpatiallyVarying;
        ek::set_attr(this, "flags", m_flags);
    }

    void set_scene(const Scene *scene) override {
        if (scene->bbox().valid()) {
            m_bsphere = scene->bbox().bounding_sphere();
            m_bsphere.radius =
                ek::max(math::RayEpsilon<Float>,
                        m_bsphere.radius * (1.f + math::RayEpsilon<Float>) );
        } else {
            m_bsphere.center = 0.f;
            m_bsphere.radius = math::RayEpsilon<Float>;
        }
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        Vector3f v = m_to_world.value().inverse().transform_affine(-si.wi);

        /* Convert to latitude-longitude texture coordinates */
        Point2f uv = Point2f(ek::atan2(v.x(), -v.z()) * ek::InvTwoPi<Float>,
                             ek::safe_acos(v.y()) * ek::InvPi<Float>);
        uv -= ek::floor(uv);

        return depolarizer<Spectrum>(eval_spectrum(uv, si.wavelengths, active));
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &sample2,
                                          const Point2f &sample3,
                                          const Float &/*volume_sample*/,
                                          Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample spatial component
        Point2f offset = warp::square_to_uniform_disk_concentric(sample2);

        // 2. Sample directional component
        auto [uv, pdf] = m_warp.sample(sample3, nullptr, active);
        active &= pdf > 0.f;

        Float theta = uv.y() * ek::Pi<ScalarFloat>;
        Float phi   = uv.x() * ek::TwoPi<ScalarFloat>;

        Vector3f d          = ek::sphdir(theta, phi);
        d                   = Vector3f(d.y(), d.z(), -d.x());
        Float inv_sin_theta = ek::safe_rsqrt(ek::sqr(d.x()) + ek::sqr(d.z()));
        pdf *= inv_sin_theta * ek::InvTwoPi<ScalarFloat> * ek::InvPi<ScalarFloat>;

        Vector3f d_global = m_to_world.value().transform_affine(d);

        // Compute ray origin
        Vector3f perpendicular_offset =
            Frame3f(d).to_world(Vector3f(offset.x(), offset.y(), 0));
        Point3f origin =
            m_bsphere.center + (perpendicular_offset - d_global) * m_bsphere.radius;

        // 3. Sample spectral component (weight accounts for radiance)
        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();
        si.t    = 0.f;
        si.time = time;
        si.p    = origin;
        si.uv   = uv;
        auto [wavelengths, wav_weight] = sample_wavelengths(si, wavelength_sample, active);

        ScalarFloat r2 = ek::sqr(m_bsphere.radius);
        Ray3f ray(origin, d_global, time, wavelengths);
        Spectrum weight = ek::Pi<ScalarFloat> * r2 * wav_weight / pdf;

        return std::make_pair(ray, weight & active);
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample,
                     const Float &/*volume_sample*/, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        auto [uv, pdf] = m_warp.sample(sample, nullptr, active);

        Float theta = uv.y() * ek::Pi<Float>,
              phi = uv.x() * ek::TwoPi<ScalarFloat>;

        Vector3f d = ek::sphdir(theta, phi);
        d = Vector3f(d.y(), d.z(), -d.x());

        // Needed when the reference point is on the sensor, which is not part of the bbox
        Float radius = ek::max(m_bsphere.radius, ek::norm(it.p - m_bsphere.center));
        Float dist = 2.f * radius;

        Float inv_sin_theta =
            ek::safe_rsqrt(ek::max(ek::sqr(d.x()) + ek::sqr(d.z()), ek::sqr(ek::Epsilon<Float>)));

        d = m_to_world.value().transform_affine(d);

        DirectionSample3f ds;
        ds.p       = it.p + d * dist;
        ds.n       = -d;
        ds.uv      = uv;
        ds.time    = it.time;
        ds.pdf     = ek::select(
            pdf > 0.f,
            pdf * inv_sin_theta * (1.f / (2.f * ek::sqr(ek::Pi<ScalarFloat>))),
            0.f
        );
        ds.delta   = false;
        ds.emitter = this;
        ds.d       = d;
        ds.dist    = dist;

        auto weight =
            depolarizer<Spectrum>(eval_spectrum(uv, it.wavelengths, active)) / ds.pdf;
        return { ds, weight & (ds.pdf > 0.f) };
    }

    Float pdf_direction(const Interaction3f & /*it*/, const DirectionSample3f &ds,
                        Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        Vector3f d = m_to_world.value().inverse().transform_affine(ds.d);

        /* Convert to latitude-longitude texture coordinates */
        Point2f uv = Point2f(ek::atan2(d.x(), -d.z()) * ek::InvTwoPi<Float>,
                             ek::safe_acos(d.y()) * ek::InvPi<Float>);
        uv -= ek::floor(uv);

        Float inv_sin_theta =
            ek::safe_rsqrt(ek::max(ek::sqr(d.x()) + ek::sqr(d.z()), ek::sqr(ek::Epsilon<Float>)));
        return m_warp.eval(uv) * inv_sin_theta * (1.f / (2.f * ek::sqr(ek::Pi<Float>)));
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si_, Float sample,
                       Mask active) const override {
        SurfaceInteraction3f si(si_);
        // TODO: consider sampling directly from the envmap spectrum at location.
        auto [wav, weight] = m_d65->sample_spectrum(
            si, math::sample_shifted<Wavelength>(sample), active);
        si.wavelengths = wav;
        // `eval` already accounts for the D65 value
        auto value = eval(si, active);
        weight /= m_d65->eval(si, active);

        return { wav, value * weight };
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float /*time*/, const Point2f & /*sample*/,
                    const Float &/*volume_sample*/,
                    Mask /*active*/) const override {
        if constexpr (ek::is_jit_array_v<Float>) {
            // When vcalls are recorded in symbolic mode, we can't throw an exception,
            // even though this result will be unused.
            return { ek::zero<PositionSample3f>(),
                     ek::full<Float>(ek::NaN<ScalarFloat>) };
        } else {
            NotImplementedError("sample_position");
        }
    }

    ScalarBoundingBox3f bbox() const override {
        /* This emitter does not occupy any particular region
           of space, return an invalid bounding box */
        return ScalarBoundingBox3f();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("scale", m_scale);
        callback->put_parameter("data", m_data);
    }

    void parameters_changed(const std::vector<std::string> &keys = {}) override {
        ScalarVector2u resolution = { m_data.shape()[1], m_data.shape()[0] };
        if (keys.empty() || string::contains(keys, "data")) {
            auto&& data = ek::migrate(m_data.array(), AllocType::Host);

            if constexpr (ek::is_jit_array_v<Float>)
                ek::sync_thread();

            std::unique_ptr<ScalarFloat[]> luminance(
                new ScalarFloat[ek::hprod(resolution)]);

            ScalarFloat *ptr     = (ScalarFloat *) data.data(),
                        *lum_ptr = (ScalarFloat *) luminance.get();

            for (size_t y = 0; y < resolution.y(); ++y) {
                ScalarFloat sin_theta =
                    ek::sin(y / ScalarFloat(resolution.y() - 1) * ek::Pi<ScalarFloat>);

                for (size_t x = 0; x < resolution.x(); ++x) {
                    ScalarVector4f coeff = ek::load_aligned<ScalarVector4f>(ptr);
                    ScalarFloat lum;

                    if constexpr (is_monochromatic_v<Spectrum>) {
                        lum = coeff.x();
                    } else if constexpr (is_rgb_v<Spectrum>) {
                        lum = mitsuba::luminance(ScalarColor3f(ek::head<3>(coeff)));
                    } else {
                        static_assert(is_spectral_v<Spectrum>);
                        lum = srgb_model_mean(ek::head<3>(coeff)) * coeff.w();
                    }

                    *lum_ptr++ = lum * sin_theta;
                    ptr += 4;
                }
            }

            m_warp = Warp(luminance.get(), resolution);
        }
    }

    std::string to_string() const override {
        ScalarVector2u resolution = { m_data.shape()[1], m_data.shape()[0] };
        std::ostringstream oss;
        oss << "EnvironmentMapEmitter[" << std::endl
            << "  filename = \"" << m_filename << "\"," << std::endl
            << "  resolution = \"" << resolution << "\"," << std::endl
            << "  bsphere = " << string::indent(m_bsphere) << std::endl
            << "]";
        return oss.str();
    }

protected:
    UnpolarizedSpectrum eval_spectrum(Point2f uv, const Wavelength &wavelengths, Mask active) const {
        ScalarVector2u resolution = { m_data.shape()[1], m_data.shape()[0] };
        uv *= Vector2f(resolution - 1u);

        Point2u pos = ek::min(Point2u(uv), resolution - 2u);

        Point2f w1 = uv - Point2f(pos),
                w0 = 1.f - w1;

        const uint32_t width = resolution.x();
        UInt32 index = pos.x() + pos.y() * width;

        Vector4f v00 = ek::gather<Vector4f>(m_data.array(), index, active),
                 v10 = ek::gather<Vector4f>(m_data.array(), index + 1, active),
                 v01 = ek::gather<Vector4f>(m_data.array(), index + width, active),
                 v11 = ek::gather<Vector4f>(m_data.array(), index + width + 1, active);

        if constexpr (is_spectral_v<Spectrum>) {
            UnpolarizedSpectrum s00, s10, s01, s11, s0, s1, s;
            Float f0, f1, f;

            s00 = srgb_model_eval<UnpolarizedSpectrum>(ek::head<3>(v00), wavelengths);
            s10 = srgb_model_eval<UnpolarizedSpectrum>(ek::head<3>(v10), wavelengths);
            s01 = srgb_model_eval<UnpolarizedSpectrum>(ek::head<3>(v01), wavelengths);
            s11 = srgb_model_eval<UnpolarizedSpectrum>(ek::head<3>(v11), wavelengths);

            s0  = ek::fmadd(w0.x(), s00, w1.x() * s10);
            s1  = ek::fmadd(w0.x(), s01, w1.x() * s11);
            f0  = ek::fmadd(w0.x(), v00.w(), w1.x() * v10.w());
            f1  = ek::fmadd(w0.x(), v01.w(), w1.x() * v11.w());

            s   = ek::fmadd(w0.y(), s0, w1.y() * s1);
            f   = ek::fmadd(w0.y(), f0, w1.y() * f1);

            /// Evaluate the whitepoint spectrum
            SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();
            si.wavelengths = wavelengths;
            UnpolarizedSpectrum wp = m_d65->eval(si, active);

            return s * wp * f * m_scale;
        } else {
            ENOKI_MARK_USED(wavelengths);
            Vector4f v0 = ek::fmadd(w0.x(), v00, w1.x() * v10),
                     v1 = ek::fmadd(w0.x(), v01, w1.x() * v11),
                     v  = ek::fmadd(w0.y(), v0, w1.y() * v1);

            if constexpr (is_monochromatic_v<Spectrum>)
                return ek::head<1>(v) * m_scale;
            else
                return ek::head<3>(v) * m_scale;
        }
    }

    MTS_DECLARE_CLASS()
protected:
    std::string m_filename;
    ScalarBoundingSphere3f m_bsphere;
    TensorXf m_data;
    Warp m_warp;
    ref<Texture> m_d65;
    ScalarFloat m_scale;
};

MTS_IMPLEMENT_CLASS_VARIANT(EnvironmentMapEmitter, Emitter)
MTS_EXPORT_PLUGIN(EnvironmentMapEmitter, "Environment map emitter")
NAMESPACE_END(mitsuba)
