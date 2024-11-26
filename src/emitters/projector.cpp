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
   - |exposed|, |differentiable|

 * - scale
   - |Float|
   - A scale factor that is applied to the radiance values stored in the above
     parameter. (Default: 1.0)
   - |exposed|, |differentiable|

 * - to_world
   - |transform|
   - Specifies an optional camera-to-world transformation.
     (Default: none (i.e. camera space = world space))
   - |exposed|

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

.. tabs::
    .. code-tab:: xml
        :name: projector-light

        <emitter type="projector">
            <rgb name="irradiance" value="1.0"/>
            <float name="fov" value="45"/>
            <transform name="to_world">
                <lookat origin="1, 1, 1"
                        target="1, 2, 1"
                        up="0, 0, 1"/>
            </transform>
        </emitter>

    .. code-tab:: python

        'type': 'projector',
        'irradiance': {
            'type': 'rgb',
            'value': 1.0,
        },
        'fov': 45,
        'to_world': mi.ScalarTransform4f().look_at(
            origin=[1, 1, 1],
            target=[1, 2, 1],
            up=[0, 0, 1]
        )

*/

MI_VARIANT class Projector final : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, m_flags, m_to_world, m_needs_sample_3)
    MI_IMPORT_TYPES(Texture)

    Projector(const Properties &props) : Base(props) {
        m_intensity_scale = dr::opaque<Float>(props.get<ScalarFloat>("scale", 1.f));

        m_irradiance = props.texture_d65<Texture>("irradiance", 1.f);

        ScalarVector2i size = m_irradiance->resolution();
        m_x_fov = ScalarFloat(parse_fov(props, size.x() / (double) size.y()));

        parameters_changed();

        m_flags = +EmitterFlags::DeltaPosition;
    }

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
        callback->put_parameter("scale",     m_intensity_scale,  +ParamFlags::Differentiable);
        callback->put_object("irradiance",   m_irradiance.get(), +ParamFlags::Differentiable);
        callback->put_parameter("to_world", *m_to_world.ptr(),   +ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys = {}) override {
        if (keys.empty() || string::contains(keys, "irradiance")) {
            ScalarVector2i size = m_irradiance->resolution();

            m_camera_to_sample = perspective_projection<Float>(size, size, 0, m_x_fov,
                                                        ScalarFloat(1e-4f),
                                                        ScalarFloat(1e4f));
            m_sample_to_camera = m_camera_to_sample.inverse();

            // Compute
            Point3f pmin(m_sample_to_camera * Point3f(0.f, 0.f, 0.f)),
                        pmax(m_sample_to_camera * Point3f(1.f, 1.f, 0.f));
            BoundingBox2f image_rect(Point2f(pmin.x(), pmin.y()) / pmin.z());
            image_rect.expand(Point2f(pmax.x(), pmax.y()) / pmax.z());
            m_sensor_area = image_rect.volume();

            dr::make_opaque(m_camera_to_sample, m_sample_to_camera,
                            m_intensity_scale, m_sensor_area);
        }
        dr::make_opaque(m_intensity_scale);

        Base::parameters_changed(keys);
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f & /*spatial_sample*/,
                                          const Point2f & direction_sample,
                                          Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);


        // 1. Sample position on film
        auto [uv, pdf] = m_irradiance->sample_position(direction_sample, active);

        // 2. Sample spectrum (weight includes irradiance eval)
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.t                    = 0.f;
        si.time                 = time;
        si.p                    = m_to_world.value().translation();
        si.uv                   = uv;
        auto [wavelengths, weight] =
            sample_wavelengths(si, wavelength_sample, active);

        // 4. Compute the sample position on the near plane (local camera space).
        Point3f near_p = m_sample_to_camera * Point3f(uv.x(), uv.y(), 0.f);
        Vector3f near_dir = dr::normalize(near_p);

        // 5. Generate transformed ray
        Ray3f ray;
        ray.time = time;
        ray.wavelengths = wavelengths;
        ray.o = si.p;
        ray.d = m_to_world.value() * near_dir;

        // Scaling factor to match \ref sample_direction.
        weight *= dr::Pi<ScalarFloat> * m_sensor_area;

        return { ray, depolarizer<Spectrum>(weight / pdf) & active };
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f & /*sample*/,
                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        // 1. Transform the reference point into the local coordinate system
        Point3f it_local = m_to_world.value().inverse().transform_affine(it.p);

        // 2. Map to UV coordinates
        Point2f uv = dr::head<2>(m_camera_to_sample * it_local);
        active &= dr::all(uv >= 0 && uv <= 1) && it_local.z() > 0;

        // 3. Query texture
        SurfaceInteraction3f it_query = dr::zeros<SurfaceInteraction3f>();
        it_query.wavelengths = it.wavelengths;
        it_query.uv = uv;
        UnpolarizedSpectrum spec = m_irradiance->eval(it_query, active);

        // 4. Prepare DirectionSample record for caller (MIS, etc.)
        DirectionSample3f ds;
        ds.p       = m_to_world.value().translation();
        ds.n       = m_to_world.value() * ScalarVector3f(0, 0, 1);
        ds.uv      = uv;
        ds.time    = it.time;
        ds.pdf     = 1.f;
        ds.delta   = true;
        ds.emitter = this;
        ds.d       = ds.p - it.p;
        Float dist_squared = dr::squared_norm(ds.d);
        ds.dist = dr::sqrt(dist_squared);
        ds.d *= dr::rcp(ds.dist);

        /* Scale so that irradiance at z=1 is correct.
         * See the weight returned by \ref PerspectiveCamera::sample_direction
         * and the comments in \ref PerspectiveCamera::importance.
         * Note that:
         *    dist^2 * cos_theta^3 == it_local.z^2 * cos_theta
         */
        spec *= dr::Pi<Float> * m_intensity_scale /
                (dr::square(it_local.z()) * -dr::dot(ds.n, ds.d));

        return { ds, depolarizer<Spectrum>(spec & active) };
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f & /*sample*/,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSamplePosition, active);

        Vector3f center_dir = m_to_world.value() * ScalarVector3f(0.f, 0.f, 1.f);
        PositionSample3f ps(
            /* position */ m_to_world.value().translation(), center_dir,
            /*uv*/ Point2f(0.5f), time, /*pdf*/ 1.f, /*delta*/ true
        );
        return { ps, Float(1.f) };
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        auto [wav, weight] = m_irradiance->sample_spectrum(
            si, math::sample_shifted<Wavelength>(sample), active);
        return { wav, weight * m_intensity_scale };
    }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &,
                        Mask) const override {
        return 0.f;
    }

    Spectrum eval_direction(const Interaction3f &it,
                            const DirectionSample3f &ds,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        Point3f it_local = m_to_world.value().inverse().transform_affine(it.p);

        SurfaceInteraction3f it_query = dr::zeros<SurfaceInteraction3f>();
        it_query.wavelengths = it.wavelengths;
        it_query.uv = ds.uv;

        UnpolarizedSpectrum spec = m_irradiance->eval(it_query, active);
        spec *= dr::Pi<Float> * m_intensity_scale /
                (dr::square(it_local.z()) * -dr::dot(ds.n, ds.d));

        return depolarizer<Spectrum>(spec) & active;
    }

    Spectrum eval(const SurfaceInteraction3f & /*si*/,
                  Mask /*active*/) const override {
        return 0.f;
    }

    ScalarBoundingBox3f bbox() const override {
        /* This emitter does not occupy any particular region
           of space, return an invalid bounding box */
        return ScalarBoundingBox3f();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Projector[" << std::endl
            << "  x_fov = " << m_x_fov << "," << std::endl
            << "  irradiance = " << string::indent(m_irradiance) << "," << std::endl
            << "  intensity_scale = " << string::indent(m_intensity_scale) << "," << std::endl
            << "  to_world = " << string::indent(m_to_world) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

protected:
    ref<Texture> m_irradiance;
    Float m_intensity_scale;
    Transform4f m_camera_to_sample;
    Transform4f m_sample_to_camera;
    ScalarFloat m_x_fov;
    Float m_sensor_area;
};

MI_IMPLEMENT_CLASS_VARIANT(Projector, Emitter)
MI_EXPORT_PLUGIN(Projector, "Projection emitter")
NAMESPACE_END(mitsuba)
