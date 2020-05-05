#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-disk:

Disk (:monosp:`disk`)
-------------------------------------------------

.. pluginparameters::

 * - flip_normals
   - |bool|
   - Is the disk inverted, i.e. should the normal vectors be flipped? (Default: |false|)
 * - to_world
   - |transform|
   - Specifies a linear object-to-world transformation. Note that non-uniform scales are not
     permitted! (Default: none, i.e. object space = world space)

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/shape_disk.jpg
   :caption: Basic example
.. subfigure:: ../../resources/data/docs/images/render/shape_disk_parameterization.jpg
   :caption: A textured disk with the default parameterization
.. subfigend::
   :label: fig-disk

This shape plugin describes a simple disk intersection primitive. It is
usually preferable over discrete approximations made from triangles.

By default, the disk has unit radius and is located at the origin. Its
surface normal points into the positive Z-direction.
To change the disk scale, rotation, or translation, use the
:monosp:`to_world` parameter.

The following XML snippet instantiates an example of a textured disk shape:

.. code-block:: xml

    <shape type="disk">
        <bsdf type="diffuse">
            <texture name="reflectance" type="checkerboard">
                <transform name="to_uv">
                    <scale x="2" y="10" />
                </transform>
            </texture>
        </bsdf>
    </shape>

.. warning:: This plugin is currently not supported by the Embree and OptiX raytracing backend.

 */

template <typename Float, typename Spectrum>
class Disk final : public Shape<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Shape, m_to_world, m_to_object, bsdf, emitter, is_emitter, sensor, is_sensor, set_children, get_children_string)
    MTS_IMPORT_TYPES()

    using typename Base::ScalarSize;

    Disk(const Properties &props) : Base(props) {
        if (props.bool_("flip_normals", false))
            m_to_world =
                m_to_world * ScalarTransform4f::scale(ScalarVector3f(1.f, 1.f, -1.f));

        m_to_object = m_to_world.inverse();

        ScalarVector3f dp_du = m_to_world * ScalarVector3f(1.f, 0.f, 0.f);
        ScalarVector3f dp_dv = m_to_world * ScalarVector3f(0.f, 1.f, 0.f);

        m_du = norm(dp_du);
        m_dv = norm(dp_dv);

        ScalarNormal3f normal = normalize(m_to_world * ScalarNormal3f(0.f, 0.f, 1.f));
        m_frame = ScalarFrame3f(dp_du / m_du, dp_dv / m_dv, normal);

        m_inv_surface_area = 1.f / surface_area();
        if (abs_dot(m_frame.s, m_frame.t) > math::RayEpsilon<ScalarFloat> ||
            abs_dot(m_frame.s, m_frame.n) > math::RayEpsilon<ScalarFloat>)
            Throw("The `to_world` transformation contains shear, which is not"
                  " supported by the Disk shape.");

        set_children();
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox;
        bbox.expand(m_to_world.transform_affine(ScalarPoint3f( 1.f,  0.f, 0.f)));
        bbox.expand(m_to_world.transform_affine(ScalarPoint3f(-1.f,  0.f, 0.f)));
        bbox.expand(m_to_world.transform_affine(ScalarPoint3f( 0.f,  1.f, 0.f)));
        bbox.expand(m_to_world.transform_affine(ScalarPoint3f( 0.f, -1.f, 0.f)));
        return bbox;
    }

    ScalarFloat surface_area() const override {
        return math::Pi<ScalarFloat> * m_du * m_dv;
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        Point2f p = warp::square_to_uniform_disk_concentric(sample);

        PositionSample3f ps;
        ps.p    = m_to_world.transform_affine(Point3f(p.x(), p.y(), 0.f));
        ps.n    = m_frame.n;
        ps.pdf  = m_inv_surface_area;
        ps.time = time;
        ps.delta = false;

        return ps;
    }

    Float pdf_position(const PositionSample3f & /*ps*/, Mask active) const override {
        MTS_MASK_ARGUMENT(active);
        return m_inv_surface_area;
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    std::pair<Mask, Float> ray_intersect(const Ray3f &ray_, Float *cache,
                                         Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        Ray3f ray     = m_to_object.transform_affine(ray_);
        Float t       = -ray.o.z() / ray.d.z();
        Point3f local = ray(t);

        // Is intersection within ray segment and disk?
        active = active && t >= ray.mint
                        && t <= ray.maxt
                        && local.x()*local.x() + local.y()*local.y() <= 1;

        if (cache) {
            masked(cache[0], active) = local.x();
            masked(cache[1], active) = local.y();
        }

        return { active, t };
    }

    Mask ray_test(const Ray3f &ray_, Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        Ray3f ray     = m_to_object * ray_;
        Float t      = -ray.o.z() / ray.d.z();
        Point3f local = ray(t);

        // Is intersection within ray segment and rectangle?
        return active && t >= ray.mint
                      && t <= ray.maxt
                      && local.x()*local.x() + local.y()*local.y() <= 1;
    }

    void fill_surface_interaction(const Ray3f &ray, const Float *cache,
                                  SurfaceInteraction3f &si_out, Mask active) const override {
        MTS_MASK_ARGUMENT(active);

#if !defined(MTS_ENABLE_EMBREE)
        Float local_x = cache[0];
        Float local_y = cache[1];
#else
        ENOKI_MARK_USED(cache);
        Ray3f ray_    = m_to_object.transform_affine(ray);
        Float t       = -ray_.o.z() / ray_.d.z();
        Point3f local = ray_(t);
        Float local_x = local.x();
        Float local_y = local.y();
#endif

        SurfaceInteraction3f si(si_out);

        Float r = norm(Point2f(local_x, local_y)),
              inv_r = rcp(r);

        Float v = atan2(local_y, local_x) * math::InvTwoPi<Float>;
        masked(v, v < 0.f) += 1.f;

        Float cos_phi = select(neq(r, 0.f), local_x * inv_r, 1.f),
              sin_phi = select(neq(r, 0.f), local_y * inv_r, 0.f);

        si.dp_du      = m_to_world * Vector3f( cos_phi, sin_phi, 0.f);
        si.dp_dv      = m_to_world * Vector3f(-sin_phi, cos_phi, 0.f);

        si.n          = m_frame.n;
        si.sh_frame.n = m_frame.n;
        si.uv         = Point2f(r, v);
        si.p          = ray(si.t);
        si.time       = ray.time;

        si_out[active] = si;
    }

    std::pair<Vector3f, Vector3f> normal_derivative(const SurfaceInteraction3f & /*si*/,
                                                    bool /*shading_frame*/,
                                                    Mask /*active*/) const override {
        return { Vector3f(0.f), Vector3f(0.f) };
    }

    ScalarSize primitive_count() const override { return 1; }

    ScalarSize effective_primitive_count() const override { return 1; }

    void traverse(TraversalCallback *callback) override {
        // TODO
        Base::traverse(callback);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/) override {
        // TODO currently no parameters are exposed so nothing can change
        // m_inv_surface_area = 1.f / surface_area();
        // if (m_emitter)
        //     m_emitter->parameters_changed({"parent"});
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Disk[" << std::endl
            << "  center = "  << m_to_world * ScalarPoint3f(0.f, 0.f, 0.f) << "," << std::endl
            << "  n = "  << m_frame.n << "," << std::endl
            << "  du = "  << m_du << "," << std::endl
            << "  dv = "  << m_dv << "," << std::endl
            << "  " << string::indent(get_children_string()) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ScalarFrame3f m_frame;
    ScalarFloat m_du, m_dv;
    ScalarFloat m_inv_surface_area;
};

MTS_IMPLEMENT_CLASS_VARIANT(Disk, Shape)
MTS_EXPORT_PLUGIN(Disk, "Disk intersection primitive");
NAMESPACE_END(mitsuba)
