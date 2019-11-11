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
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Flat disk shape.
 *
 * By default, the disk has unit radius and is located at the origin. Its
 * surface normal points into the positive $Z$ direction. To change the disk
 * scale, rotation, or translation, use the 'to_world' parameter.
 */
template <typename Float, typename Spectrum>
class Disk final : public Shape<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(Disk, Shape)
    MTS_USING_BASE(Shape, bsdf, emitter, is_emitter);
    MTS_IMPORT_TYPES()
    using Size  = typename Base::Size;

    Disk(const Properties &props) : Base(props) {
        m_object_to_world = props.transform("to_world", ScalarTransform4f());
        if (props.bool_("flip_normals", false))
            m_object_to_world =
                m_object_to_world * ScalarTransform4f::scale(ScalarVector3f(1.f, 1.f, -1.f));

        m_world_to_object = m_object_to_world.inverse();

        m_dp_du = m_object_to_world * ScalarVector3f(1.f, 0.f, 0.f);
        m_dp_dv = m_object_to_world * ScalarVector3f(0.f, 1.f, 0.f);
        Normal3f normal = normalize(m_object_to_world * ScalarNormal3f(0.f, 0.f, 1.f));
        m_frame = ScalarFrame3f(normalize(m_dp_du), normalize(m_dp_dv), normal);

        m_inv_surface_area = 1.f / surface_area();
        if (abs_dot(m_frame.s, m_frame.t) > math::Epsilon<Float> ||
            abs_dot(m_frame.s, m_frame.n) > math::Epsilon<Float>)
            Throw("The `to_world` transformation contains shear, which is not"
                  " supported by the Disk shape.");

        if (is_emitter())
            emitter()->set_shape(this);
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox;
        bbox.expand(m_object_to_world.transform_affine(ScalarPoint3f( 1.f,  0.f, 0.f)));
        bbox.expand(m_object_to_world.transform_affine(ScalarPoint3f(-1.f,  0.f, 0.f)));
        bbox.expand(m_object_to_world.transform_affine(ScalarPoint3f( 0.f,  1.f, 0.f)));
        bbox.expand(m_object_to_world.transform_affine(ScalarPoint3f( 0.f, -1.f, 0.f)));
        return bbox;
    }

    ScalarFloat surface_area() const override {
        return math::Pi<ScalarFloat> * norm(m_dp_du) * norm(m_dp_dv);
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask /*active*/) const override {
        Point2f p = warp::square_to_uniform_disk_concentric(sample);

        PositionSample3f ps;
        ps.p    = m_object_to_world.transform_affine(Point3f(p.x(), p.y(), 0.f));
        ps.n    = m_frame.n;
        ps.pdf  = m_inv_surface_area;
        ps.time = time;
        ps.delta = false;

        return ps;
    }

    Float pdf_position(const PositionSample3f & /*ps*/, Mask /*active*/) const override {
        return m_inv_surface_area;
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    std::pair<Mask, Float> ray_intersect(const Ray3f &ray_, Float *cache,
                                         Mask active) const override {
        Ray3f ray     = m_world_to_object.transform_affine(ray_);
        Float t      = -ray.o.z() / ray.d.z();
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
        Ray3f ray     = m_world_to_object * ray_;
        Float t      = -ray.o.z() / ray.d.z();
        Point3f local = ray(t);

        // Is intersection within ray segment and rectangle?
        return active && t >= ray.mint
                      && t <= ray.maxt
                      && local.x()*local.x() + local.y()*local.y() <= 1;
    }

    void fill_surface_interaction(const Ray3f &ray, const Float *cache,
                                  SurfaceInteraction3f &si_out, Mask active) const override {
        SurfaceInteraction3f si(si_out);

        Float r = norm(Point2f(cache[0], cache[1])),
              inv_r = rcp(r);

        Float v = atan2(cache[1], cache[0]) * math::InvTwoPi<Float>;
        masked(v, v < 0.f) += 1.f;

        Float cos_phi = select(neq(r, 0.f), cache[0] * inv_r, 1.f),
              sin_phi = select(neq(r, 0.f), cache[1] * inv_r, 0.f);

        si.dp_du      = m_object_to_world * Vector3f( cos_phi, sin_phi, 0.f);
        si.dp_dv      = m_object_to_world * Vector3f(-sin_phi, cos_phi, 0.f);

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

    Size primitive_count() const override { return 1; }

    Size effective_primitive_count() const override { return 1; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Disk[" << std::endl
            << "  center = "  << m_object_to_world * ScalarPoint3f(0.f, 0.f, 0.f) << "," << std::endl
            << "  n = "  << m_object_to_world * ScalarNormal3f(0.f, 0.f, 1.f) << "," << std::endl
            << "  du = "  << norm(m_object_to_world * ScalarNormal3f(1.f, 0.f, 0.f)) << "," << std::endl
            << "  dv = "  << norm(m_object_to_world * ScalarNormal3f(0.f, 1.f, 0.f)) << "," << std::endl
            << "  bsdf = " << string::indent(bsdf()->to_string()) << std::endl
            << "]";
        return oss.str();
    }

private:
    ScalarTransform4f m_object_to_world;
    ScalarTransform4f m_world_to_object;
    ScalarFrame3f m_frame;
    ScalarVector3f m_dp_du, m_dp_dv;
    ScalarFloat m_inv_surface_area;
};

MTS_IMPLEMENT_PLUGIN(Disk, Shape, "Disk intersection primitive");
NAMESPACE_END(mitsuba)
