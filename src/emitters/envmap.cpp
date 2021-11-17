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
#include <mitsuba/render/fwd.h>
#include <mitsuba/core/fstream.h>
#include <enoki/struct.h>
#include <mitsuba/core/traits.h>
#include <mitsuba/core/math.h>
#include <enoki/matrix.h>
#include <enoki/dynamic.h>

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

        ref<Bitmap> bitmap;
        // Enabled internal readin of bitmaps
        if(props.has_property("bitmap")) {
            bitmap = (Bitmap*) props.pointer("bitmap");
            m_filename = "internal";
        } 
        else {
            FileResolver *fs = Thread::thread()->file_resolver();
            fs::path file_path = fs->resolve(props.string("filename"));

            bitmap = new Bitmap(file_path);

            m_filename = file_path.filename().string();
        }

        if (bitmap->width() < 2 || bitmap->height() < 3)
            Throw("\"%s\": the environment map resolution must be at "
                  "least 2x3 pixels", m_filename);

        
        std::vector<std::string> channel_names;
        for (size_t i = 0; i < bitmap->channel_count(); i++)
            channel_names.push_back(bitmap->struct_()->operator[](i).name);
        
        /* Allocate a larger image including an extra column to
           account for the periodic boundary */
        ScalarVector2u res(bitmap->width() + 1, bitmap->height());
        ref<Bitmap> bitmap_2;

        if (bitmap->pixel_format() == Bitmap::PixelFormat::MultiChannel){
            m_is_spectral = true;
    
            bitmap_2 = new Bitmap(bitmap->pixel_format(),
                                            bitmap->component_format(), res,
                                            bitmap->channel_count(),
                                            channel_names);
        }else{
            /* Convert to linear RGBA float bitmap, will undergo further
            conversion into coefficients of a spectral upsampling model below */
            bitmap = bitmap->convert(Bitmap::PixelFormat::RGBA, struct_type_v<Float>, false);
            m_is_spectral = false;

            bitmap_2 = new Bitmap(bitmap->pixel_format(),
                                            bitmap->component_format(), res);
        }
        m_channel_count = bitmap->channel_count();
            
        // Luminance image used for importance sampling
        std::unique_ptr<ScalarFloat[]> luminance(new ScalarFloat[ek::hprod(res)]);

        ScalarFloat *in_ptr  = (ScalarFloat *) bitmap->data(),
                    *out_ptr = (ScalarFloat *) bitmap_2->data(),
                    *lum_ptr = (ScalarFloat *) luminance.get();

        ScalarFloat theta_scale = 1.f / (bitmap->size().y() - 1) * ek::Pi<Float>;

        // If bitmap contains non-spectral data
        if (bitmap->pixel_format() != Bitmap::PixelFormat::MultiChannel){
            for (size_t y = 0; y < bitmap->size().y(); ++y) {
                ScalarFloat sin_theta = ek::sin(y * theta_scale);

                for (size_t x = 0; x < bitmap->size().x(); ++x) {
                    ScalarColor3f rgb = ek::load<ScalarVector3f>(in_ptr);
                    ScalarFloat lum = mitsuba::luminance(rgb);

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
                        ScalarColor3f rgb_norm = rgb / ek::max(1e-8f, scale);
                        coeff = ek::concat((ScalarColor3f) srgb_model_fetch(rgb_norm),
                                        ek::Array<ScalarFloat, 1>(scale));
                    }

        // If bitmap contains non-spectral data
        if (bitmap->pixel_format() != Bitmap::PixelFormat::MultiChannel){
            for (size_t y = 0; y < bitmap->size().y(); ++y) {
                ScalarFloat sin_theta = ek::sin(y * theta_scale);

                for (size_t x = 0; x < bitmap->size().x(); ++x) {
                    ScalarColor3f rgb = ek::load<ScalarVector3f>(in_ptr);
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
        if (keys.empty() || string::contains(keys, "data")) {
            ScalarVector2u res = { m_data.shape(1), m_data.shape(0) };
            auto&& data = ek::migrate(m_data.array(), AllocType::Host);

            if constexpr (ek::is_jit_array_v<Float>)
                ek::sync_thread();

            std::unique_ptr<ScalarFloat[]> luminance(
                new ScalarFloat[ek::hprod(res)]);

            ScalarFloat *ptr     = (ScalarFloat *) data.data(),
                        *lum_ptr = (ScalarFloat *) luminance.get();

            ScalarFloat theta_scale = 1.f / (res.y() - 1) * ek::Pi<Float>;
            for (size_t y = 0; y < res.y(); ++y) {
                ScalarFloat sin_theta = ek::sin(y * theta_scale);

                // Enforce horizontal continuity
                ScalarFloat *ptr2 = ptr + 4 * (res.x() - 1);
                ScalarVector4f v0  = ek::load_aligned<ScalarVector4f>(ptr),
                               v1  = ek::load_aligned<ScalarVector4f>(ptr2),
                               v01 = .5f * (v0 + v1);
                ek::store_aligned(ptr, v01),
                ek::store_aligned(ptr2, v01);

                for (size_t x = 0; x < res.x(); ++x) {
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

            m_warp = Warp(luminance.get(), res);

            if constexpr (ek::is_jit_array_v<Float>)
                m_data.array() = ek::migrate(data, ek::is_cuda_array_v<Float>
                                                       ? AllocType::Device
                                                       : AllocType::HostAsync);
        }
    }

    std::string to_string() const override {
        ScalarVector2u res = { m_data.shape(1), m_data.shape(0) };
        std::ostringstream oss;
        oss << "EnvironmentMapEmitter[" << std::endl
            << "  filename = \"" << m_filename << "\"," << std::endl
            << "  res = \"" << res << "\"," << std::endl
            << "  bsphere = " << string::indent(m_bsphere) << std::endl
            << "]";
        return oss.str();
    }

protected:

    UnpolarizedSpectrum eval_spectrum(Point2f uv, const Wavelength &wavelengths,
                                      Mask active, bool include_whitepoint = true) const {
        ScalarVector2u res = { m_data.shape(1), m_data.shape(0) };

        uv.x() -= .5f / (res.x() - 1u);
        uv -= ek::floor(uv);
        uv *= Vector2f(res - 1u);

        Point2u pos = ek::min(Point2u(uv), res - 2u);

        Point2f w1 = uv - Point2f(pos),
                w0 = 1.f - w1;

        const uint32_t width = res.x();
        UInt32 index = pos.x() + pos.y() * width;

        Vector4f v00 = ek::gather<Vector4f>(m_data.array(), index, active),
                 v10 = ek::gather<Vector4f>(m_data.array(), index + 1, active),
                 v01 = ek::gather<Vector4f>(m_data.array(), index + width, active),
                 v11 = ek::gather<Vector4f>(m_data.array(), index + width + 1, active);

        if constexpr (!is_spectral_v<Spectrum>) {
            ENOKI_MARK_USED(wavelengths);
            Vector4f v0 = ek::fmadd(w0.x(), v00, w1.x() * v10),
                     v1 = ek::fmadd(w0.x(), v01, w1.x() * v11),
                     v  = ek::fmadd(w0.y(), v0, w1.y() * v1);

            if constexpr (is_monochromatic_v<Spectrum>)
                return ek::head<1>(v) * m_scale;
            else
                return ek::head<3>(v) * m_scale;

        } else {
            
            UnpolarizedSpectrum s00, s10, s01, s11, s0, s1, s;
            UnpolarizedSpectrum result;
            //upsamping needed for non-spectral data
            if (m_channel <= 4){
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

                result = s * f * m_scale;
            } else{ //spectral sky data (11-channel)
                using DiscreteSpectrum = mitsuba::Spectrum<ScalarFloat, 11>;
                using WavelengthIndex = enoki::Array<uint32_t, Spectrum::Size>; 

                DiscreteSpectrum v00 = ek::gather<DiscreteSpectrum>(m_data.array(), index, active),
                                 v10 = ek::gather<DiscreteSpectrum>(m_data.array(), index + 1, active),
                                 v01 = ek::gather<DiscreteSpectrum>(m_data.array(), index + width, active),
                                 v11 = ek::gather<DiscreteSpectrum>(m_data.array(), index + width + 1, active);

                auto interpolate_spec = [](DiscreteSpectrum v, const Wavelength &wavelengths) {
                    float band_wavelength[11] = {320.f, 360.f, 400.f, 440.f, 
                    480.f, 520.f, 560.f, 600.f, 640.f, 680.f, 720.f};

                    WavelengthIndex idx = WavelengthIndex((wavelengths - 320.f) / 40.f);

                    UnpolarizedSpectrum s = 0.f;

                    for (size_t i = 0; i < 4; i++){
                        if ((idx[i] >= 10) || (idx[i] <= 0) ) {
                            int index = ek::max(0, ek::min(idx[i], 10));
                            s[i] = v[index];
                        } else {
                            s[i] = v[idx[i]] + (v[idx[i] + 1] - v[idx[i]]) * (wavelengths[i] - band_wavelength[idx[i]]) / 40.f;
                        }
                    }
                    return s;
                };
                s00 = interpolate_spec(v00, wavelengths);
                s01 = interpolate_spec(v01, wavelengths);
                s10 = interpolate_spec(v10, wavelengths);
                s11 = interpolate_spec(v11, wavelengths);

                s0  = ek::fmadd(w0.x(), s00, w1.x() * s10);
                s1  = ek::fmadd(w0.x(), s01, w1.x() * s11);

                s   = ek::fmadd(w0.y(), s0, w1.y() * s1);
                result = s *  m_scale;
            }

            Vector4f v00 = ek::gather<Vector4f>(m_data.array(), index, active),
                     v10 = ek::gather<Vector4f>(m_data.array(), index + 1, active),
                     v01 = ek::gather<Vector4f>(m_data.array(), index + width, active),
                     v11 = ek::gather<Vector4f>(m_data.array(), index + width + 1, active);

            return result;
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
    size_t m_channel_count;
    // true if incoming bitmap is MultiChannel;
    bool m_is_spectral;
    std::vector<float> m_channels;
};

MTS_IMPLEMENT_CLASS_VARIANT(EnvironmentMapEmitter, Emitter)
MTS_EXPORT_PLUGIN(EnvironmentMapEmitter, "Environment map emitter")
NAMESPACE_END(mitsuba)