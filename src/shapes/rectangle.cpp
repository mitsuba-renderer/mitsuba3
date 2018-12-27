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
 * \brief Flat, rectangular shape, e.g. for use as a ground plane or area emitter.
 *
 * By default, the rectangle covers the XY-range [-1, 1]^2 and has a surface
 * normal that points into the positive $Z$ direction. To change the rectangle
 * scale, rotation, or translation, use the 'to_world' parameter.
 */
class Rectangle final : public Shape {
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

        m_inv_surface_area = rcp(surface_area());
        if (abs(dot(normalize(m_dp_du), normalize(m_dp_dv))) > math::Epsilon)
            Throw("The `to_world` transformation contains shear, which is not"
                  " supported by the Rectangle shape.");

        if (is_emitter())
            emitter()->set_shape(this);
    }

    BoundingBox3f bbox() const override {
        BoundingBox3f bbox;
        bbox.expand(m_object_to_world.transform_affine(Point3f(-1.f, -1.f, 0.f)));
        bbox.expand(m_object_to_world.transform_affine(Point3f( 1.f, -1.f, 0.f)));
        bbox.expand(m_object_to_world.transform_affine(Point3f( 1.f,  1.f, 0.f)));
        bbox.expand(m_object_to_world.transform_affine(Point3f(-1.f,  1.f, 0.f)));
        return bbox;
    }

    Float surface_area() const override {
        return norm(m_dp_du) * norm(m_dp_dv);
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    template <typename Point2, typename Value = value_t<Point2>>
    MTS_INLINE auto sample_position_impl(Value time, const Point2 &sample,
                                         mask_t<Value> /* active */) const {
        using Point3   = point3_t<Point2>;

        PositionSample<Point3> ps;
        ps.p = m_object_to_world.transform_affine(
            Point3(sample.x() * 2.f - 1.f, sample.y() * 2.f - 1.f, 0.f));
        ps.n    = m_frame.n;
        ps.pdf  = m_inv_surface_area;
        ps.uv   = sample;
        ps.time = time;

        return ps;
    }

    template <typename PositionSample3,
              typename Value = typename PositionSample3::Value>
    MTS_INLINE Value pdf_position_impl(const PositionSample3 &/* ps */,
                                       mask_t<Value> /* active */) const {
        return m_inv_surface_area;
    }

    template <typename Interaction3, typename Point2,
              typename Value = value_t<Point2>>
    MTS_INLINE auto sample_direction_impl(const Interaction3 &it, const Point2 &sample,
                                          mask_t<Value> active) const {
        using DirectionSample = DirectionSample<typename Interaction3::Point3>;

        DirectionSample ds = sample_position_impl(it.time, sample, active);
        ds.d = ds.p - it.p;

        Value dist_squared = squared_norm(ds.d);
        ds.dist  = sqrt(dist_squared);
        ds.d    /= ds.dist;

        Value dp = abs_dot(ds.d, ds.n);
        ds.pdf *= select(neq(dp, 0.f), dist_squared / dp, Value(0.f));

        return ds;
    }

    template <typename Interaction3, typename DirectionSample3,
              typename Value = typename Interaction3::Value>
    MTS_INLINE Value pdf_direction_impl(const Interaction3 &/* it */,
                                        const DirectionSample3 &ds,
                                        mask_t<Value> active) const {
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
    MTS_INLINE std::pair<mask_t<Value>, Value> ray_intersect_impl(
        const Ray3 &ray_, Value *cache, mask_t<Value> active) const {
        using Point3 = Point<Value, 3>;

        Ray3 ray     = m_world_to_object.transform_affine(ray_);
        Value t      = -ray.o.z() * ray.d_rcp.z();
        Point3 local = ray(t);

        // Is intersection within ray segment and rectangle?
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
    MTS_INLINE mask_t<Value> ray_test_impl(const Ray3 &ray_, mask_t<Value> active) const {
        using Point3 = Point<Value, 3>;

        Ray3 ray     = m_world_to_object.transform_affine(ray_);
        Value t      = -ray.o.z() * ray.d_rcp.z();
        Point3 local = ray(t);

        // Is intersection within ray segment and rectangle?
        return active && t >= ray.mint
                      && t <= ray.maxt
                      && abs(local.x()) <= 1.f
                      && abs(local.y()) <= 1.f;
    }

    template <typename Ray3,
              typename SurfaceInteraction3,
              typename Value = typename SurfaceInteraction3::Value>
    MTS_INLINE void fill_surface_interaction_impl(const Ray3 &ray, const Value *cache,
                                                  SurfaceInteraction3 &si_out,
                                                  mask_t<Value> active) const {
        using Point2  = typename SurfaceInteraction3::Point2;

        SurfaceInteraction3 si(si_out);

        si.n          = m_frame.n;
        si.sh_frame.n = m_frame.n;
        si.dp_du      = m_dp_du;
        si.dp_dv      = m_dp_dv;
        si.p          = ray(si.t);
        si.time       = ray.time;
        si.uv         = Point2(.5f * (cache[0] + 1.f),
                               .5f * (cache[1] + 1.f));

        si_out[active] = si;
    }

    template <typename SurfaceInteraction3,
              typename Value   = typename SurfaceInteraction3::Value,
              typename Vector3 = typename SurfaceInteraction3::Vector3>
    MTS_INLINE std::pair<Vector3, Vector3> normal_derivative_impl(
            const SurfaceInteraction3 &/* si */, bool, mask_t<Value>) const {
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
