#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

/*!\plugin{sphere}{Sphere intersection primitive}
* \order{1}
* \parameters{
*     \parameter{center}{\Point}{
*       Center of the sphere in object-space \default{(0, 0, 0)}
*     }
*     \parameter{radius}{\Float}{
*       Radius of the sphere in object-space units \default{1}
*     }
*     \parameter{to_world}{\Transform\Or\Animation}{
*        Specifies an optional linear object-to-world transformation.
*        Note that non-uniform scales are not permitted!
*        \default{none (i.e. object space $=$ world space)}
*     }
*     \parameter{flip_normals}{\Boolean}{
*        Is the sphere inverted, i.e. should the normal vectors
*        be flipped? \default{\code{false}, i.e. the normals point outside}
*     }
* }
*
* \renderings{
*     \rendering{Basic example, see \lstref{sphere-basic}}
*         {shape_sphere_basic}
*     \rendering{A textured sphere with the default parameterization}
*         {shape_sphere_parameterization}
* }
*
* This shape plugin describes a simple sphere intersection primitive. It should
* always be preferred over sphere approximations modeled using triangles.
*
* \begin{xml}[caption={A sphere can either be configured using a linear
* \code{to_world} transformation or the \code{center} and \code{radius} parameters (or both).
*    The above two declarations are equivalent.}, label=lst:sphere-basic]
* <shape type="sphere">
*     <transform name="to_world">
*         <scale value="2"/>
*         <translate x="1" y="0" z="0"/>
*     </transform>
*     <bsdf type="diffuse"/>
* </shape>
*
* <shape type="sphere">
*     <point name="center" x="1" y="0" z="0"/>
*     <float name="radius" value="2"/>
*     <bsdf type="diffuse"/>
* </shape>
* \end{xml}
* When a \pluginref{sphere} shape is turned into an \pluginref{area} light source,
* Mitsuba switches to an efficient sampling strategy \cite{Shirley91Direct} that
* has particularly low variance. This makes it a good default choice for lighting
* new scenes (\figref{spherelight}).
* \renderings{
*     \rendering{Spherical area light modeled using triangles}
*         {shape_sphere_arealum_tri}
*     \rendering{Spherical area light modeled using the \pluginref{sphere} plugin}
*         {shape_sphere_arealum_analytic}
*
*     \caption{
*         \label{fig:spherelight}
*         Area lights built from the combination of the \pluginref{area}
*         and \pluginref{sphere} plugins produce renderings that have an
*         overall lower variance.
*     }
* }
* \begin{xml}[caption=Instantiation of a sphere emitter]
* <shape type="sphere">
*     <point name="center" x="0" y="1" z="0"/>
*     <float name="radius" value="1"/>

*     <emitter type="area">
*         <blackbody name="intensity" temperature="7000K"/>
*     </emitter>
* </shape>
* \end{xml}
*/
class Sphere : public Shape {
public:
    Sphere(const Properties &props) : Shape(props) {
        m_object_to_world =
            Transform4f::translate(Vector3f(props.point3f("center", Point3f(0.0f))));
        m_radius = props.float_("radius", 1.0f);

        if (props.has_property("to_world")) {
            Transform4f object_to_world = props.transform("to_world");
            Float radius = norm(object_to_world * Vector3f(1, 0, 0));
            // Remove the scale from the object-to-world transform
            m_object_to_world =
                object_to_world
                * Transform4f::scale(Vector3f(1.0f / radius))
                * m_object_to_world;
            m_radius *= radius;
        }

        /// Are the sphere normals pointing inwards? default: no
        m_flip_normals = props.bool_("flip_normals", false);
        m_center = m_object_to_world * Point3f(0, 0, 0);
        m_world_to_object = m_object_to_world.inverse();
        m_inv_surface_area = 1.0f / (4.0f * math::Pi * m_radius * m_radius);

        if (m_radius <= 0.0f)
            Throw("Cannot create spheres of radius <= 0");

        if (is_emitter())
            emitter()->set_shape(this);
    }

    BoundingBox3f bbox() const override {
        BoundingBox3f bbox;
        bbox.min = m_center - Vector3f(m_radius);
        bbox.max = m_center + Vector3f(m_radius);
        return bbox;
    }

    Float surface_area() const override {
        return 4.0f * math::Pi * m_radius * m_radius;
    }

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    template <typename Ray3, typename Value>
    std::pair<mask_t<Value>, Value> ray_intersect_impl(
            const Ray3 &ray, Value mint_, Value maxt_,
            void * /*cache*/, const mask_t<Value> &active) const {
        using Double  = like_t<Value, double>;
        using Vector3 = Vector<Double, 3>;
        using Mask    = mask_t<Value>;

        Double mint(mint_);
        Double maxt(maxt_);

        Vector3 o = Vector3(ray.o) - Vector3(m_center);
        Vector3 d(ray.d);

        Double A = squared_norm(d);
        Double B = 2.0 * dot(o, d);
        Double C = squared_norm(o) - (double) (m_radius * m_radius);

        Mask solution_found;
        Double near_t, far_t;
        std::tie(solution_found, near_t, far_t) = math::solve_quadratic(A, B, C);

        // Sphere doesn't intersect with the segment on the ray
        Mask out_bounds = !((near_t <= maxt) && (far_t >= mint)); // NaN-aware conditionals
        // Sphere fully contains the segment of the ray
        Mask in_bounds = (near_t < mint) && (far_t > maxt);

        Mask valid_intersection = solution_found && (!out_bounds) && (!in_bounds);
        Value t = select(near_t < mint, far_t, near_t);

        return { valid_intersection && active, t };
    }

    template <typename Ray3, typename Value>
    mask_t<Value> ray_intersect_impl(const Ray3 &ray, Value mint_, Value maxt_,
                                     const mask_t<Value> &active) const {
        using Double  = like_t<Value, double>;
        using Vector3 = Vector<Double, 3>;
        using Mask    = mask_t<Value>;

        Double mint(mint_);
        Double maxt(maxt_);

        Vector3 o = Vector3(ray.o) - Vector3(m_center);
        Vector3 d(ray.d);

        Double A = squared_norm(d);
        Double B = 2.0 * dot(o, d);
        Double C = squared_norm(o) - (double) (m_radius * m_radius);

        Mask solution_found;
        Double near_t, far_t;
        std::tie(solution_found, near_t, far_t) = math::solve_quadratic(A, B, C);

        // Sphere doesn't intersect with the segment on the ray
        Mask out_bounds = !((near_t <= maxt) && (far_t >= mint)); // NaN-aware conditionals
        // Sphere fully contains the segment of the ray
        Mask in_bounds  = ((near_t < mint) && (far_t > maxt));

        return solution_found && (!out_bounds) && (!in_bounds) && active;
    }

    template <typename Ray3,
              typename SurfaceInteraction3,
              typename Value = typename SurfaceInteraction3::Value>
    void fill_intersection_record_impl(const Ray3 &ray, const void * /*cache*/,
                                       SurfaceInteraction3 &its,
                                       const mask_t<Value> &active) const {
        using Vector3 = typename SurfaceInteraction3::Vector3;
        using Mask    = mask_t<Value>;

        its.p = ray(its.t);

        // Re-project onto the sphere to limit cancellation effects
        its.p = m_center + normalize(its.p - m_center) * m_radius;

        Vector3 local = m_world_to_object * (its.p - m_center);
        Value   theta = safe_acos(local.z() / m_radius);
        Value   phi   = atan2(local.y(), local.x());

        masked(phi, phi < 0.0f) += 2.0f * math::Pi;

        its.uv.x() = phi * math::InvTwoPi;
        its.uv.y() = theta * math::InvPi;
        its.dp_du = m_object_to_world * Vector3(
            -local.y(), local.x(), 0.0f) * (2.0f * math::Pi);
        its.sh_frame.n = normalize(its.p - m_center);
        Value z_rad = sqrt(local.x() * local.x() + local.y() * local.y());
        its.shape = this;

        Mask z_rad_gt_0  = (z_rad > 0.0f);
        Mask z_rad_leq_0 = !z_rad_gt_0;

        if (any(z_rad_gt_0 && active)) {
            Value inv_z_rad = rcp(z_rad),
                  cos_phi   = local.x() * inv_z_rad,
                  sin_phi   = local.y() * inv_z_rad;
            masked(its.dp_dv, z_rad_gt_0) = m_object_to_world
                                            * Vector3(local.z() * cos_phi,
                                                      local.z() * sin_phi,
                                                      -sin(theta) * m_radius) * math::Pi;
            masked(its.sh_frame.s, z_rad_gt_0) = normalize(its.dp_du);
            masked(its.sh_frame.t, z_rad_gt_0) = normalize(its.dp_dv);
        }

        if (any(z_rad_leq_0 && active)) {
            // avoid a singularity
            const Value cos_phi = 0.0f, sin_phi = 1.0f;
            masked(its.dp_dv, z_rad_leq_0) = m_object_to_world
                                             * Vector3(local.z() * cos_phi,
                                                       local.z() * sin_phi,
                                                       -sin(theta) * m_radius) * math::Pi;
            Vector3 a, b;
            std::tie(a, b) = coordinate_system(its.sh_frame.n);
            masked(its.sh_frame.s, z_rad_leq_0) = a;
            masked(its.sh_frame.t, z_rad_leq_0) = b;
        }

        if (m_flip_normals)
            its.sh_frame.n *= -1.0f;

        its.n = its.sh_frame.n;
        its.has_uv_partials = false;
        its.instance = nullptr;
        its.time = ray.time;
    }

    template <typename SurfaceInteraction3,
              typename Value   = typename SurfaceInteraction3::Value,
              typename Vector3 = typename SurfaceInteraction3::Vector3>
    std::pair<Vector3, Vector3> normal_derivative_impl(
            const SurfaceInteraction3 &its, bool /*shading_frame*/,
            const mask_t<Value> &/*active*/) const {
        Value inv_radius = (m_flip_normals ? -1.0f : 1.0f) / m_radius;
        Vector3 dn_du = its.dp_du * inv_radius;
        Vector3 dn_dv = its.dp_dv * inv_radius;
        return { dn_du , dn_dv };
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================
    template <typename PositionSample3,
              typename Point2,
              typename Value = value_t<Point2>>
    void sample_position_impl(PositionSample3 &p_rec,
                              const Point2 &sample,
                              const mask_t<Value> &active) const {
        using Point3   = point3_t<Point2>;
        using Normal3  = normal3_t<Point2>;
        using Vector3  = vector3_t<Point2>;

        Vector3 v = warp::square_to_uniform_sphere(sample);

        masked(p_rec.p, active) = Point3(v * m_radius) + m_center;
        masked(p_rec.n, active) = Normal3(v);

        if (m_flip_normals)
            masked(p_rec.n, active) *= -1;

        masked(p_rec.pdf,     active) = m_inv_surface_area;
        masked(p_rec.measure, active) = EArea;
    }

    template <typename PositionSample3,
              typename Value = typename PositionSample3::Value>
    Value pdf_position_impl(PositionSample3 &/*p_rec*/,
                            const mask_t<Value> &/*active*/) const {
        return m_inv_surface_area;
    }

    /**
     * Improved sampling strategy given in
     * "Monte Carlo techniques for direct lighting calculations" by
     * Shirley, P. and Wang, C. and Zimmerman, K. (TOG 1996)
     */
    template <typename DirectSample3,
              typename Point2,
              typename Value = value_t<Point2>>
    void sample_direct_impl(DirectSample3 &d_rec,
                            const Point2 &sample,
                            const mask_t<Value> &active) const {
        using Vector3 = vector3_t<Point2>;
        using Normal3 = normal3_t<Point2>;
        using Point3  = point3_t<Point2>;
        using Frame3  = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        const Vector3 ref_to_center = m_center - d_rec.ref_p;
        const Value ref_dist_2 = squared_norm(ref_to_center);
        const Value inv_ref_dist = rcp(sqrt(ref_dist_2));

        // Sine of the angle of the cone containing the
        // sphere as seen from 'd_rec.ref'
        const Value sin_alpha = m_radius * inv_ref_dist;

        Mask ref_outside     = sin_alpha < (1.0f - math::Epsilon);
        Mask not_ref_outside = !ref_outside;
        // Only keep the active lanes
        ref_outside     = ref_outside && active;
        not_ref_outside = not_ref_outside && active;

        if (any(ref_outside)) {
            // The reference point lies outside of the sphere.
            // => sample based on the projected cone.

            Value cos_alpha = enoki::safe_sqrt(1.0f - sin_alpha * sin_alpha);

            masked(d_rec.d, ref_outside) = Frame3(ref_to_center * inv_ref_dist).to_world(
                                                  warp::square_to_uniform_cone(sample, cos_alpha));
            masked(d_rec.pdf, ref_outside) = warp::square_to_uniform_cone_pdf(Vector3(0, 0, 0), cos_alpha);

            // Distance to the projection of the sphere center
            // onto the ray (d_rec.ref, d_rec.d)
            const Value proj_dist = dot(ref_to_center, d_rec.d);

            // To avoid numerical problems move the query point to the
            // intersection of the of the original direction ray and a plane
            // with normal ref_to_center which goes through the sphere's center
            const Value base_t = ref_dist_2 / proj_dist;
            const Point3 query = d_rec.ref_p + d_rec.d * base_t;

            const Vector3 query_to_center = m_center - query;
            const Value   query_dist_2    = squared_norm(query_to_center);
            const Value   query_proj_dist = dot(query_to_center, d_rec.d);

            // Try to find the intersection point between the sampled ray and the sphere.
            Value A = 1.0f,
                  B = -2.0f * query_proj_dist,
                  C = query_dist_2 - m_radius * m_radius;

            Mask solution_found;
            Value near_t, far_t;
            std::tie(solution_found, near_t, far_t) = math::solve_quadratic(A, B, C);

            // The intersection couldn't be found due to roundoff errors..
            // Don't give up -- one workaround is to project the closest
            // ray position onto the sphere
            masked(near_t, !solution_found) = query_proj_dist;

            masked(d_rec.dist, ref_outside) = base_t + near_t;
            masked(d_rec.n,    ref_outside) = normalize(d_rec.d * near_t - query_to_center);
            masked(d_rec.p,    ref_outside) = m_center + d_rec.n * m_radius;
        }

        if (any(!ref_outside)) {
            // The reference point lies inside the sphere
            // => use uniform sphere sampling.
            Vector3 d = warp::square_to_uniform_sphere(sample);

            masked(d_rec.p, not_ref_outside) = m_center + d * m_radius;
            masked(d_rec.n, not_ref_outside) = Normal3(d);
            masked(d_rec.d, not_ref_outside) = d_rec.p - d_rec.ref_p;

            Value dist2 = squared_norm(d_rec.d);
            masked(d_rec.dist, not_ref_outside) = sqrt(dist2);
            masked(d_rec.d,    not_ref_outside) /= d_rec.dist;
            masked(d_rec.pdf,  not_ref_outside) = m_inv_surface_area * dist2
                                                  / abs_dot(d_rec.d, d_rec.n);
        }

        if (m_flip_normals)
            d_rec.n *= -1.0f;

        d_rec.measure = ESolidAngle;
    }

    template <typename DirectSample3,
              typename Vector3 = typename DirectSample3::Vector3,
              typename Value   = value_t<Vector3>>
    Value pdf_direct_impl(const DirectSample3 &d_rec, const mask_t<Value> &active) const {
        using Mask = mask_t<Value>;

        const Vector3 ref_to_center = m_center - d_rec.ref_p;
        const Value inv_ref_dist = rcp(norm(ref_to_center));

        // Sine of the angle of the cone containing the
        // sphere as seen from 'd_rec.ref'
        const Value sin_alpha = m_radius * inv_ref_dist;

        Mask ref_outside = sin_alpha < (1 - math::Epsilon);
        Mask not_ref_outside = !ref_outside;
        // Only keep the active lanes
        ref_outside     = ref_outside && active;
        not_ref_outside = not_ref_outside && active;

        Value res(0.0f);
        if (any(ref_outside)) {
            // The reference point lies outside the sphere
            Value cos_alpha = enoki::safe_sqrt(1 - sin_alpha * sin_alpha);
            Value pdf_sa = warp::square_to_uniform_cone_pdf(Vector3(0, 0, 0), cos_alpha);

            masked(res, ref_outside && eq(d_rec.measure, ESolidAngle)) = pdf_sa;
            masked(res, ref_outside && eq(d_rec.measure, EArea))
                = pdf_sa * abs_dot(d_rec.d, d_rec.n) / (d_rec.dist * d_rec.dist);
        }

        if (any(not_ref_outside)) {
            // The reference point lies inside the sphere
            masked(res, not_ref_outside && eq(d_rec.measure, ESolidAngle))
                = m_inv_surface_area * d_rec.dist * d_rec.dist / abs_dot(d_rec.d, d_rec.n);
            masked(res, not_ref_outside && eq(d_rec.measure, EArea)) = m_inv_surface_area;
        }
        return res;
    }

    //! @}
    // =============================================================

    Size primitive_count() const override { return 1; }

    size_t effective_primitive_count() const override { return 1; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Sphere[" << std::endl
            << "  radius = "  << m_radius << "," << std::endl
            << "  center = "  << m_center << "," << std::endl
            << "  bsdf = "    << string::indent(bsdf()->to_string()) << "," << std::endl
            << "  emitter = " << string::indent(emitter()->to_string()) << "," << std::endl
            << "  sensor = "  << string::indent(sensor()->to_string())  << "," << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_SHAPE()
    MTS_DECLARE_CLASS()
private:
    Transform4f m_object_to_world;
    Transform4f m_world_to_object;
    Point3f m_center;
    Float   m_radius;
    Float   m_inv_surface_area;
    bool    m_flip_normals;
};

MTS_IMPLEMENT_CLASS(Sphere, Shape)
MTS_EXPORT_PLUGIN(Sphere, "Sphere intersection primitive");

NAMESPACE_END(mitsuba)
