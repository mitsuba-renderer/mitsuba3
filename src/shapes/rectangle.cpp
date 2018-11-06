#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Simple analytic rectangle, for use e.g. as a ground plane or area emitter.
 *
 * By default, the rectangle covers the XY-range [-1, 1]^2 and has a surface
 * normal that points into the positive $Z$ direction.
 * To change the rectangle scale, rotation, or translation, use the
 * \ref to_world parameter.
 */
class Rectangle : public Shape {
public:
    Rectangle(const Properties &props) : Shape(props) {
        m_object_to_world = props.transform("to_world", Transform4f());
        if (props.bool_("flip_normals", false))
            m_object_to_world = m_object_to_world * Transform4f::scale(Vector3f(1.f, 1.f, -1.f));

        m_world_to_object = m_object_to_world.inverse();

        m_dp_du = m_object_to_world * Vector3f(2.f, 0.f, 0.f);
        m_dp_dv = m_object_to_world * Vector3f(0.f, 2.f, 0.f);
        Normal3f normal = normalize(m_object_to_world * Normal3f(0.f, 0.f, 1.f));
        m_frame = Frame<Vector3f>(normalize(m_dp_du), normalize(m_dp_dv), normal);

        m_inv_surface_area = 1.f / surface_area();
        if (abs(dot(normalize(m_dp_du), normalize(m_dp_dv))) > math::Epsilon)
            Throw("The `to_world` transformation contains shear, which is not"
                  " supported by the Rectangle shape.");

        if (is_emitter())
            emitter()->set_shape(this);
    }

    BoundingBox3f bbox() const override {
        BoundingBox3f bbox;
        bbox.expand(m_object_to_world * Point3f(-1.f, -1.f, 0.f));
        bbox.expand(m_object_to_world * Point3f( 1.f, -1.f, 0.f));
        bbox.expand(m_object_to_world * Point3f( 1.f,  1.f, 0.f));
        bbox.expand(m_object_to_world * Point3f(-1.f,  1.f, 0.f));
        return bbox;
    }

    Float surface_area() const override {
        return norm(m_dp_du) * norm(m_dp_dv);
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================
    template <typename Point2, typename Value = value_t<Point2>>
    auto sample_position_impl(Value time, const Point2 &sample,
                              const mask_t<Value> &/*active*/) const {
        using Point3   = point3_t<Point2>;
        using Normal3  = normal3_t<Point2>;

        PositionSample<Point3> ps;
        ps.p = m_object_to_world * Point3(sample.x() * 2.f - 1.f,
                                          sample.y() * 2.f - 1.f,
                                          0.f);
        ps.n    = Normal3(m_frame.n);
        ps.pdf  = m_inv_surface_area;
        ps.uv   = sample;
        ps.time = time;

        return ps;
    }

    template <typename PositionSample3,
              typename Value = typename PositionSample3::Value>
    Value pdf_position_impl(PositionSample3 &/*p_rec*/,
                            const mask_t<Value> &/*active*/) const {
        return m_inv_surface_area;
    }

    template <typename Interaction3, typename Point2,
              typename Value = value_t<Point2>>
    auto sample_direction_impl(const Interaction3 &it, const Point2 &sample,
                               const mask_t<Value> &active) const {
        using DirectionSample = DirectionSample<typename Interaction3::Point3>;

        DirectionSample ds = sample_position_impl(it.time, sample, active);
        ds.d = ds.p - it.p;

        Value dist_squared = squared_norm(ds.d);
        ds.dist  = sqrt(dist_squared);
        ds.d    /= ds.dist;
        Value dp = abs_dot(ds.d, ds.n);
        ds.pdf  *= select(neq(dp, 0.f), dist_squared / dp, Value(0.f));

        return ds;
    }

    template <typename Interaction3, typename DirectionSample3,
              typename Value = typename Interaction3::Value>
    Value pdf_direction_impl(const Interaction3 &/*it*/, const DirectionSample3 &ds,
                             const mask_t<Value> &active) const {
        Value pdf = pdf_position_impl(ds, active),
              dp  = abs_dot(ds.d, ds.n);

        return pdf * select(neq(dp, 0.f), (ds.dist * ds.dist) / dp, 0.f);
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    template <typename Ray3, typename Value = typename Ray3::Value>
    std::pair<mask_t<Value>, Value> ray_intersect_impl(
            const Ray3 &ray_, Value *cache, mask_t<Value> active) const {
        using Point3 = Point<Value, 3>;

        Ray3 ray     = m_world_to_object * ray_;
        Value t      = -ray.o.z() / ray.d.z();
        Point3 local = ray(t);

        // Intersection is within ray segment and within rectangle.
        active = active && t >= ray.mint
                        && t <= ray.maxt
                        && abs(local.x()) <= 1.f
                        && abs(local.y()) <= 1.f;

        t = select(active, t, Value(math::Infinity));

        if (cache) {
            masked(cache[0], active) = local.x();
            masked(cache[1], active) = local.y();
        }

        return { active, t };
    }

    template <typename Ray3, typename Value = typename Ray3::Value>
    mask_t<Value> ray_test_impl(const Ray3 &ray_, mask_t<Value> active) const {
        using Point3 = Point<Value, 3>;

        Ray3 ray     = m_world_to_object * ray_;
        Value t      = -ray.o.z() / ray.d.z();
        Point3 local = ray(t);
        // Intersection is within ray segment and within rectangle.
        return active && t >= ray.mint
                      && t <= ray.maxt
                      && abs(local.x()) <= 1.f
                      && abs(local.y()) <= 1.f;
    }

    template <typename Ray3,
              typename SurfaceInteraction3,
              typename Value = typename SurfaceInteraction3::Value>
    void fill_intersection_record_impl(const Ray3 &ray, const Value *cache,
                                       SurfaceInteraction3 &si,
                                       mask_t<Value> active) const {
        using Point2  = typename SurfaceInteraction3::Point2;

        masked(si.n,          active) = m_frame.n;
        masked(si.sh_frame.n, active) = m_frame.n;
        masked(si.shape,      active) = this;
        masked(si.dp_du,      active) = m_dp_du;
        masked(si.dp_dv,      active) = m_dp_dv;
        masked(si.p,          active) = ray(si.t);
        masked(si.time,       active) = ray.time;
        masked(si.instance,   active) = nullptr;
        masked(si.uv,         active) = Point2(.5f * (cache[0] + 1.f),
                                               .5f * (cache[1] + 1.f));
    }

    template <typename SurfaceInteraction3,
              typename Value   = typename SurfaceInteraction3::Value,
              typename Vector3 = typename SurfaceInteraction3::Vector3>
    std::pair<Vector3, Vector3> normal_derivative_impl(
            const SurfaceInteraction3 &/*si*/, bool, const mask_t<Value> &) const {
        return { Vector3(0.f), Vector3(0.f) };
    }

    Size primitive_count() const override { return 1; }

    Size effective_primitive_count() const override { return 1; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Rectangle[" << std::endl
            << "  object_to_world = " << string::indent(m_object_to_world) << "," << std::endl
            << "  frame = " << string::indent(m_frame) << "," << std::endl
            << "  inv_surface_area = " << m_inv_surface_area << "," << std::endl
            << "  bsdf = " << string::indent(bsdf()->to_string()) << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_SHAPE()
    MTS_DECLARE_CLASS()
private:
    Transform4f m_object_to_world;
    Transform4f m_world_to_object;
    Frame<Vector3f> m_frame;
    Vector3f m_dp_du, m_dp_dv;
    Float m_inv_surface_area;
};

MTS_IMPLEMENT_CLASS(Rectangle, Shape)
MTS_EXPORT_PLUGIN(Rectangle, "Rectangle intersection primitive");

NAMESPACE_END(mitsuba)
