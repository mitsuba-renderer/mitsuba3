#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-projector:

Projection light source (:monosp:`projector`)
---------------------------------------------

.. pluginparameters::

 * - irradiance
   - |texture|
   - 2D texture specifying irradiance on the emitter's virtual image plane,
     which lies at a distance of :math:`z=1` from the pinhole. Note that this
     does not directly correspond to emitted radiance due to the presence of an
     additional directionally varying scale factor equal to to the inverse
     sensitivity profile (a.k.a. importance) of a perspective camera. This
     ensures that a projection of a constant texture onto a plane is truly
     constant.

 * - scale
   - |Float|
   - A scale factor that is applied to the radiance values stored in the above
     parameter. (Default: 1.0)

 * - to_world
   - |transform|
   - Specifies an optional camera-to-world transformation.
     (Default: none (i.e. camera space = world space))

 * - fov
   - |float|
   - Denotes the camera's field of view in degrees---must be between 0 and 180,
     excluding the extremes. Alternatively, it is also possible to specify a
     field of view using the :monosp:`focal_length` parameter.

 * - focal_length
   - |string|
   - Denotes the camera's focal length specified using *35mm* film
     equivalent units. Alternatively, it is also possible to specify a field of
     view using the :monosp:`fov` parameter. See the main description for further
     details. (Default: :monosp:`50mm`)

 * - fov_axis
   - |string|
   - When the parameter :monosp:`fov` is given (and only then), this parameter further specifies
     the image axis, to which it applies.

     1. :monosp:`x`: :monosp:`fov` maps to the :monosp:`x`-axis in screen space.
     2. :monosp:`y`: :monosp:`fov` maps to the :monosp:`y`-axis in screen space.
     3. :monosp:`diagonal`: :monosp:`fov` maps to the screen diagonal.
     4. :monosp:`smaller`: :monosp:`fov` maps to the smaller dimension
        (e.g. :monosp:`x` when :monosp:`width` < :monosp:`height`)
     5. :monosp:`larger`: :monosp:`fov` maps to the larger dimension
        (e.g. :monosp:`y` when :monosp:`width` < :monosp:`height`)

     The default is :monosp:`x`.


This emitter is the reciprocal counterpart of the perspective camera
implemented by the :ref:`perspective <sensor-perspective>` plugin. It
accepts exactly the same parameters and employs the same pixel-to-direction
mapping. In contrast to the perspective camera, it takes an extra texture
(typically of type :ref:`bitmap <texture-bitmap>`) as input that it then
projects into the scene, with an optional scaling factor.

Pixels are importance sampled according to their density, hence this
operation remains efficient even if only a single pixel is turned on.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/emitter_projector_constant.jpg
   :caption: A projector lights with constant irradiance (no texture specified).
.. subfigure:: ../../resources/data/docs/images/render/emitter_projector_textured.jpg
   :caption: A projector light with a texture specified.
.. subfigend::
   :label: fig-projector-light

*/

MTS_VARIANT class Projector final : public Emitter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Emitter, m_flags, m_world_transform, m_needs_sample_3)
    MTS_IMPORT_TYPES(Texture)

    Projector(const Properties &props) : Base(props) {
        m_intensity = Texture::D65(props.float_("scale", 1));

        m_irradiance = props.texture<Texture>("irradiance");
        ScalarVector2i size = m_irradiance->resolution();
        m_x_fov = parse_fov(props, size.x() / (float) size.y());

        m_camera_to_sample = perspective_projection(size, size, 0, m_x_fov,
                                                    1e-4f, 1e4f);

        m_sample_to_camera = m_camera_to_sample.inverse();

        m_flags = +EmitterFlags::DeltaPosition;
    }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &,
                        Mask) const override {
        return 0.f;
    }

    Spectrum eval(const SurfaceInteraction3f & /*si*/,
                  Mask /*active*/) const override {
        return 0.f;
    }

    /// TODO: Completely untested, need to revist once the particle tracer is merged.
    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f & /* spatial_sample */,
                                          const Point2f & direction_sample,
                                          Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample spectrum
        auto [wavelengths, weight] =
            sample_wavelength<Float, Spectrum>(wavelength_sample);

        // 2. Sample position on film
        auto [uv, pdf] = m_irradiance->sample_position(direction_sample, active);

        // 3. Query irradiance on film
        SurfaceInteraction3f si = zero<SurfaceInteraction3f>();
        si.uv = uv;
        si.wavelengths = wavelengths;
        weight *= m_irradiance->eval(si, active) * m_intensity->eval(si, active);

        // 4. Compute the sample position on the near plane (local camera space).
        Point3f near_p = m_sample_to_camera * Point3f(uv.x(), uv.y(), 0.f);

        // 5. Generate transformed ray
        Transform4f trafo = m_world_transform->eval(time, active);

        Ray3f ray;
        ray.time = time;
        ray.wavelengths = wavelengths;
        ray.o = trafo.translation();
        ray.d = trafo * Vector3f(normalize(near_p));
        ray.update();

        return std::make_pair(
            ray, unpolarized<Spectrum>(weight / pdf) & active);
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f & /*sample*/,
                     Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        // 1. Transform the reference point into the local coordinate system
        Transform4f trafo = m_world_transform->eval(it.time, active);
        Point it_local = trafo.inverse().transform_affine(it.p);

        // 2. Map to UV coordinates
        Point2f uv = head<2>(m_camera_to_sample * it_local);
        active &= all(uv >= 0 && uv <= 1) && it_local.z() > 0;

        // 3. Query texture
        SurfaceInteraction3f it_query = zero<SurfaceInteraction3f>();
        it_query.wavelengths = it.wavelengths;
        it_query.uv = uv;
        UnpolarizedSpectrum spec = m_irradiance->eval(it_query, active);

        // 4. Prepare DirectionSample record for caller (MIS, etc.)
        DirectionSample3f ds;
        ds.p = trafo.translation();
        ds.n = trafo * ScalarVector3f(0, 0, 1);
        ds.uv = uv;
        ds.time = it.time;
        ds.pdf = 1.f;
        ds.delta = true;
        ds.object   = this;

        ds.d = ds.p - it.p;
        Float dist_squared = squared_norm(ds.d);
        ds.dist = sqrt(dist_squared);
        ds.d *= rcp(ds.dist);

        // Scale so that irradiance at z=1 is correct
        spec *= math::Pi<Float> * m_intensity->eval(it_query, active) *
                sqr(rcp(it_local.z())) / -dot(ds.n, ds.d);

        return { ds, unpolarized<Spectrum>(spec & active) };
    }

    ScalarBoundingBox3f bbox() const override {
        /* This emitter does not occupy any particular region
           of space, return an invalid bounding box */
        return ScalarBoundingBox3f();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("irradiance", m_irradiance.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Projector[" << std::endl
            << "  x_fov = " << m_x_fov << "," << std::endl
            << "  irradiance = " << string::indent(m_irradiance) << "," << std::endl
            << "  world_transform = " << string::indent(m_world_transform) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()

protected:
    ref<Texture> m_irradiance;
    ref<Texture> m_intensity;
    ScalarTransform4f m_camera_to_sample;
    ScalarTransform4f m_sample_to_camera;
    ScalarFloat m_x_fov;
};

MTS_IMPLEMENT_CLASS_VARIANT(Projector, Emitter)
MTS_EXPORT_PLUGIN(Projector, "Projection emitter")
NAMESPACE_END(mitsuba)
