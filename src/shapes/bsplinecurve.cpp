#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/mmap.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <iostream>

#include <drjit/texture.h>

#if defined(MI_ENABLE_EMBREE)
#include <embree3/rtcore.h>
#endif

NAMESPACE_BEGIN(mitsuba)

/**
 * NOTES:
 * - Gradients seem to match, there are a few issues still:
 *      - The gradients at the curve extremities do not have the same magnitude,
 *        this might just be because the Epislon used in the FD approach isn't
 *        small enough
 *      - In CUDA variants, something is numerically unstable in the AD graph.
 *        The forward-mode gradients will result in some NaNs.
 *
 * - Internally, we should only make use of `m_vertex` and `m_radius` for
 *   gradient tracking, otherwise the mi.render() function will behave
 *   "un-expectedly".
 */

template <typename Float, typename Spectrum>
class BSplineCurve final : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_to_world, m_to_object, m_is_instance, initialize,
                   mark_dirty, get_children_string, parameters_grad_enabled)
    MI_IMPORT_TYPES()

    using typename Base::ScalarIndex;
    using typename Base::ScalarSize;

    using InputFloat = float;
    using InputPoint3f  = Point<InputFloat, 3>;
    using InputVector3f = Vector<InputFloat, 3>;
    using FloatStorage = DynamicBuffer<dr::replace_scalar_t<Float, InputFloat>>;

    using UInt32Storage = DynamicBuffer<UInt32>;
    using Index = typename CoreAliases::UInt32;


    BSplineCurve(const Properties &props) : Base(props) {

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        std::string m_name = file_path.filename().string();

        // used for throwing an error later
        auto fail = [&](const char *descr, auto... args) {
            Throw(("Error while loading bspline curve file \"%s\": " + std::string(descr))
                      .c_str(), m_name, args...);
        };

        Log(Debug, "Loading a bspline curve file from \"%s\" ..", m_name);
        if (!fs::exists(file_path))
            fail("file not found");

        ref<MemoryMappedFile> mmap = new MemoryMappedFile(file_path);
        ScopedPhase phase(ProfilerPhase::LoadGeometry);

        // temporary buffer for vertices and per-vertex radius
        std::vector<InputVector3f> vertices;
        std::vector<InputFloat> radius;
        ScalarSize vertex_guess = mmap->size() / 100;
        vertices.reserve(vertex_guess);

        // load data from the given .txt file
        const char *ptr = (const char *) mmap->data();
        const char *eof = ptr + mmap->size();
        char buf[1025];
        Timer timer;

        while (ptr < eof) {
            // Determine the offset of the next newline
            const char *next = ptr;
            advance<false>(&next, eof, "\n");

            // Copy buf into a 0-terminated buffer
            ScalarSize size = next - ptr;
            if (size >= sizeof(buf) - 1)
                fail("file contains an excessively long line! (%i characters)", size);
            memcpy(buf, ptr, size);
            buf[size] = '\0';

            // handle current line: v.x v.y v.z radius
            // skip whitespace
            const char *cur = buf, *eol = buf + size;
            advance<true>(&cur, eol, " \t\r");
            bool parse_error = false;

            // Vertex position
            InputPoint3f p;
            InputFloat r;
            for (ScalarSize i = 0; i < 3; ++i) {
                const char *orig = cur;
                p[i] = string::strtof<InputFloat>(cur, (char **) &cur);
                parse_error |= cur == orig;
            }

            // parse per-vertex radius
            const char *orig = cur;
            r = string::strtof<InputFloat>(cur, (char **) &cur);
            parse_error |= cur == orig;

            p = m_to_world.scalar().transform_affine(p);

            if (unlikely(!all(dr::isfinite(p))))
                fail("bspline control point contains invalid vertex position data");
            if (unlikely(!dr::isfinite(r)))
                fail("bspline control point contains invalid radius data");

            // TODO: how to calculate bspline's bbox
            // just expand using control points for now
            m_bbox.expand(p);

            vertices.push_back(p);
            radius.push_back(r);

            if (unlikely(parse_error))
                fail("could not parse line \"%s\"", buf);
            ptr = next + 1;
        }

        m_control_point_count = vertices.size();
        m_segment_count = m_control_point_count - 3;
        if (unlikely(m_control_point_count < 4))
            fail("bspline must have at least four control points");
        for (ScalarIndex i = 0; i < m_control_point_count; i++)
            Log(Debug, "Loaded a control point %s with radius %f",
                vertices[i], radius[i]);

        // store the data from the previous temporary buffer
        std::unique_ptr<float[]> vertex_positions_radius(new float[m_control_point_count * 4]);
        std::unique_ptr<ScalarIndex[]> indices(new ScalarIndex[m_segment_count]);

        // for OptiX
        std::unique_ptr<float[]> vertex_position(new float[m_control_point_count * 3]);
        std::unique_ptr<float[]> vertex_radius(new float[m_control_point_count * 1]);

        for (ScalarIndex i = 0; i < vertices.size(); i++) {
            InputFloat* position_ptr = vertex_positions_radius.get() + i * 4;
            InputFloat* radius_ptr   = vertex_positions_radius.get() + i * 4 + 3;

            dr::store(position_ptr, vertices[i]);
            dr::store(radius_ptr, radius[i]);

            // OptiX
            position_ptr = vertex_position.get() + i * 3;
            radius_ptr = vertex_radius.get() + i;
            dr::store(position_ptr, vertices[i]);
            dr::store(radius_ptr, radius[i]);
        }

        for (ScalarIndex i = 0; i < m_segment_count; i++) {
            u_int32_t* index_ptr = indices.get() + i;
            dr::store(index_ptr, i);
        }

        m_vertex_with_radius = dr::load<FloatStorage>(vertex_positions_radius.get(), m_control_point_count * 4);
        m_indices = dr::load<UInt32Storage>(indices.get(), m_segment_count);

        // OptiX
        m_vertex = dr::load<FloatStorage>(vertex_position.get(), m_control_point_count * 3);
        m_radius = dr::load<FloatStorage>(vertex_radius.get(), m_control_point_count * 1);

        ScalarSize vertex_data_bytes = 8 * sizeof(InputFloat);
        Log(Debug, "\"%s\": read %i control points (%s in %s)",
            m_name, m_control_point_count,
            util::mem_string(m_control_point_count * vertex_data_bytes),
            util::time_string((float) timer.value())
        );

        initialize();
    }

    template <bool Negate, size_t N>
    void advance(const char **start_, const char *end, const char (&delim)[N]) {
        const char *start = *start_;

        while (true) {
            bool is_delim = false;
            for (size_t i = 0; i < N; ++i)
                if (*start == delim[i])
                    is_delim = true;
            if ((is_delim ^ Negate) || start == end)
                break;
            ++start;
        }

        *start_ = start;
    }

    Float globalu_to_u(Float global_u) const {
        return (global_u - 1.5f / m_control_point_count) /
               (1.f - 3.f / m_control_point_count);
    }

    Float u_to_globalu(Float u) const {
        return u * (1.f - 3.f / m_control_point_count) + 1.5f /
               m_control_point_count;
    }

    SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                               uint32_t ray_flags,
                                               Mask active) const override {
        ScalarFloat delta = 1.f / m_control_point_count;
        PreliminaryIntersection3f pi = dr::zeros<PreliminaryIntersection3f>();
        Float eps = dr::Epsilon<Float>;

        // Global u to local u
        Float u_global = u_to_globalu(uv.x());
        dr::masked(u_global, u_global < delta * 1.5f + eps) += eps;
        dr::masked(u_global, u_global > 1.f - delta * 1.5f - eps) -= eps;
        UInt32 segment_id = dr::floor2int<UInt32>((u_global - delta * 1.5f) / delta);
        Float u_local = u_global * m_control_point_count - 1.5f - segment_id;

        pi.prim_uv.x() = u_local;
        pi.prim_uv.y() = uv.y();
        pi.prim_index = segment_id;
        pi.shape = this;
        dr::masked(pi.t, active) = eps * 10;

        /* Create a ray at the intersection point and offset it by epsilon in
         the direction of the local surface normal */

        Point3f c;
        Vector3f dc_du;
        Float radius;
        Float dr_du;
        Vector3f dc_duu;
        Float dr_duu;
        std::tie(c, dc_du, dc_duu, radius, dr_du, dr_duu) = cubic_interpolation(u_local, segment_id, active);
        Vector3f dc_du_normalized = dr::normalize(dc_du);

        Vector3f v_rot, v_rad;
        std::tie(v_rot, v_rad) = local_frame(dc_du_normalized);

        auto [sin_v, cos_v] = dr::sincos(uv.y() * dr::TwoPi<Float>);
        Point3f o = c + cos_v * v_rad * (radius + pi.t) + sin_v * v_rot * (radius + pi.t);

        Vector3f rad_vec = o - c;
        Normal3f d = -dr::normalize(dr::norm(dc_du) * rad_vec - (dr_du * radius) * dc_du_normalized);

        Ray3f ray(o, d, 0, Wavelength(0));

        SurfaceInteraction3f si =
            compute_surface_interaction(ray, pi, ray_flags, 0, active);
        si.finalize_surface_interaction(pi, ray, ray_flags, active);

        return si;
    }

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     uint32_t recursion_depth,
                                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);
        constexpr bool IsDiff = dr::is_diff_v<Float>;

        // Early exit when tracing isn't necessary
        if (!m_is_instance && recursion_depth > 0)
            return dr::zeros<SurfaceInteraction3f>();

        // Fields requirement dependencies
        bool need_dn_duv  = has_flag(ray_flags, RayFlags::dNSdUV) ||
                            has_flag(ray_flags, RayFlags::dNGdUV);
        bool need_dp_duv  = has_flag(ray_flags, RayFlags::dPdUV) || need_dn_duv;
        bool need_uv      = has_flag(ray_flags, RayFlags::UV) || need_dp_duv;
        bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape);
        bool follow_shape = has_flag(ray_flags, RayFlags::FollowShape);

        const Transform4f& to_world = m_to_world.value();
        const Transform4f& to_object = m_to_object.value();

        /* If necessary, temporally suspend gradient tracking for all shape
           parameters to construct a surface interaction completely detach from
           the shape. */
        dr::suspend_grad<Float> scope(detach_shape, to_world, to_object,
                                      m_vertex, m_radius);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

        Float u_local = pi.prim_uv.x();
        Index idx = pi.prim_index;

        Point3f c;
        Vector3f dc_du;
        Vector3f dc_duu;
        Float radius;
        Float dr_du;
        Float dr_duu;
        std::tie(c, dc_du, dc_duu, radius, dr_du, dr_duu) =
            cubic_interpolation(u_local, idx, active);
        Vector3f dc_du_normalized = dr::normalize(dc_du);

        Vector3f v_rot, v_rad;
        std::tie(v_rot, v_rad) = local_frame(dc_du_normalized);

        if constexpr (IsDiff) {
            Point3f p = ray(pi.t);
            Vector3f rad_vec = p - c;
            Vector3f rad_vec_normalized = dr::normalize(rad_vec);

            Float v = dr::atan2(dr::dot(v_rot, rad_vec_normalized),
                                dr::dot(v_rad, rad_vec_normalized));
            v += dr::select(v < 0.f, dr::TwoPi<Float>, 0.f);
            v *= dr::InvTwoPi<Float>;
            v = dr::detach(v); // v has no motion for an attached intersection point

            // Differentiable intersection point (w.r.t curve parameters)
            auto [sin_v, cos_v] = dr::sincos(v * dr::TwoPi<Float>);
            Point3f p_diff =
                c + cos_v * v_rad * radius + sin_v * v_rot * radius;
            p = dr::replace_grad(p, p_diff);

            if (follow_shape) {
                /* FollowShape glues the interaction point with the shape.
                   Therefore, to also account for a possible differential motion
                   of the shape, the interaction point must be completely
                   differentiable w.r.t. the curve parameters. */
                si.p = p;
                si.t = dr::sqrt(dr::squared_norm(si.p - ray.o) /
                                dr::squared_norm(ray.d));
            } else {
                /* To ensure that the differential interaction point stays along
                   the traced ray, we first recompute the intersection distance
                   in a differentiable way (w.r.t. the curve parameters) and
                   then compute the corresponding point along the ray. (Instead
                   of computing an intersection with the curve, we compute an
                   intersection with the tangent plane.) */
                Vector3f rad_vec_diff = si.p - c;
                rad_vec = dr::replace_grad(rad_vec, rad_vec_diff);
                // Differentiable tangent plane normal
                Float correction = dr::dot(rad_vec, dc_duu);
                Vector3f n = dr::normalize(
                    (dr::squared_norm(dc_du) - correction) * rad_vec -
                    (dr_du * radius) * dc_du
                );

                Float t_diff = dr::dot(p - ray.o, n) / dr::dot(n, ray.d);
                si.t = dr::replace_grad(pi.t, t_diff);
                si.p = ray(si.t);
            }
        } else {
            si.t = pi.t;
            si.p = ray(si.t);
        }

        si.t = dr::select(active, si.t, dr::Infinity<Float>);

        // Normal
        Vector3f rad_vec = si.p - c;
        Vector3f rad_vec_normalized = dr::normalize(rad_vec);
        Float correction = dr::dot(rad_vec, dc_duu);  // curvature correction
        Normal3f n = dr::normalize(
            (dr::squared_norm(dc_du) - correction) * rad_vec -
            (dr_du * radius) * dc_du
        );
        si.n = si.sh_frame.n = n;

        if (need_uv) {
            // Convert segment-local u to curve-global u
            Float u_global = (u_local + idx + 1.5f) / m_control_point_count;
            Float u = globalu_to_u(u_global);

            Float v = dr::atan2(dr::dot(v_rot, rad_vec_normalized),
                                dr::dot(v_rad, rad_vec_normalized));
            v += dr::select(v < 0.f, dr::TwoPi<Float>, 0.f);
            v *= dr::InvTwoPi<Float>;

            // TODO: U gradients are wrong w/o FollowShape
            // how to compute to compute U from si.p?
            // does set_grad(u, dc_du) work?
            si.uv = Point2f(u, v);
            if (follow_shape)
                si.uv = dr::detach(si.uv);

        }

        if (likely(need_dp_duv)) {
            // TODO: gradients are wrong w/o FollowShape? need to recompute them w.r.t si.p
            si.dp_du = (dc_du + rad_vec_normalized * dr_du) / (1.f - 3.f / m_control_point_count);
            si.dp_dv = dr::cross(rad_vec_normalized, dc_du_normalized) * dr::TwoPi<Float> * radius;
        }

        if (need_dn_duv) {
            si.dn_du = si.dn_dv = Vector3f(0);  // TODO
        }

        si.shape = this;
        si.instance = nullptr;

        if (unlikely(has_flag(ray_flags, RayFlags::BoundaryTest)))
            si.boundary_test = dr::dot(si.sh_frame.n, -ray.d);


        return si;
    }

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
        callback->put_parameter("control_point_count", m_control_point_count, +ParamFlags::NonDifferentiable);
        callback->put_parameter("control_points",      m_vertex,              ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_parameter("radiuses",            m_radius,              ParamFlags::Differentiable | ParamFlags::Discontinuous);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "control_points") || string::contains(keys, "radiuses")) {
             update();
             mark_dirty();
        }
        Base::parameters_changed();
    }

    bool parameters_grad_enabled() const override {
        return dr::grad_enabled(m_radius) || dr::grad_enabled(m_vertex);
    }


#if defined(MI_ENABLE_EMBREE)
    RTCGeometry embree_geometry(RTCDevice device) override {
        RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_ROUND_BSPLINE_CURVE);

        dr::eval(m_vertex_with_radius); // Make sure m_vertex_with_radius is up to date
        rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT4,
                                   m_vertex_with_radius.data(), 0, 4 * sizeof(InputFloat),
                                   m_control_point_count);
        rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT,
                                   m_indices.data(), 0, 1 * sizeof(ScalarIndex),
                                   m_segment_count);
        rtcCommitGeometry(geom);
        return geom;
    }
#endif


#if defined(MI_ENABLE_CUDA)
    void optix_prepare_geometry() override { }

    void optix_build_input(OptixBuildInput &build_input) const override {
        m_vertex_buffer_ptr = (void*) m_vertex.data(); // triggers dr::eval()
        m_radius_buffer_ptr = (void*) m_radius.data(); // triggers dr::eval()
        m_index_buffer_ptr = (void*) m_indices.data(); // triggers dr::eval()

        build_input.type = OPTIX_BUILD_INPUT_TYPE_CURVES;
        build_input.curveArray.curveType = OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE;
        build_input.curveArray.numPrimitives        = m_segment_count;

        build_input.curveArray.vertexBuffers        = (CUdeviceptr*) &m_vertex_buffer_ptr;
        build_input.curveArray.numVertices          = m_control_point_count;
        build_input.curveArray.vertexStrideInBytes  = sizeof( InputFloat ) * 3;

        build_input.curveArray.widthBuffers         = (CUdeviceptr*) &m_radius_buffer_ptr;
        build_input.curveArray.widthStrideInBytes   = sizeof( InputFloat );

        build_input.curveArray.indexBuffer          = (CUdeviceptr) m_index_buffer_ptr;
        build_input.curveArray.indexStrideInBytes   = sizeof( ScalarIndex );

        build_input.curveArray.normalBuffers        = 0;
        build_input.curveArray.normalStrideInBytes  = 0;
        build_input.curveArray.flag                 = OPTIX_GEOMETRY_FLAG_NONE;
        build_input.curveArray.primitiveIndexOffset = 0;
        build_input.curveArray.endcapFlags          = OPTIX_CURVE_ENDCAP_ON;
    }
#endif

    void update() {
        UInt32 idx = dr::arange<UInt32>(m_control_point_count);
        dr::scatter(m_vertex_with_radius, dr::gather<Float>(m_vertex, idx * 3u + 0u), idx * 4u + 0u);
        dr::scatter(m_vertex_with_radius, dr::gather<Float>(m_vertex, idx * 3u + 1u), idx * 4u + 1u);
        dr::scatter(m_vertex_with_radius, dr::gather<Float>(m_vertex, idx * 3u + 2u), idx * 4u + 2u);
        dr::scatter(m_vertex_with_radius, dr::gather<Float>(m_radius, idx * 1u + 0u), idx * 4u + 3u);
        dr::eval(m_vertex_with_radius);

        m_to_object = m_to_world.value().inverse();
    }

    bool is_curve() const override {
        return true;
    }

    bool is_bspline_curve() const override {
        return true;
    }

    ScalarBoundingBox3f bbox() const override {
        return m_bbox;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "BSpline[" << std::endl
            << "  " << string::indent(get_children_string()) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

private:
    std::tuple<Point3f, Vector3f, Vector3f, Float, Float, Float>
    cubic_interpolation(const Float u, const Index &idx, Mask active) const {
        Point3f v0 = dr::gather<Point3f>(m_vertex, idx + 0, active),
                v1 = dr::gather<Point3f>(m_vertex, idx + 1, active),
                v2 = dr::gather<Point3f>(m_vertex, idx + 2, active),
                v3 = dr::gather<Point3f>(m_vertex, idx + 3, active);
        Float r0 = dr::gather<Float>(m_radius, idx + 0, active),
              r1 = dr::gather<Float>(m_radius, idx + 1, active),
              r2 = dr::gather<Float>(m_radius, idx + 2, active),
              r3 = dr::gather<Float>(m_radius, idx + 3, active);

        Float u2 = dr::sqr(u), u3 = u2 * u;
        Float multiplier = 1.f / 6.f;

        Point3f c = (-u3 + 3.f * u2 - 3.f * u + 1.f) * v0 +
                    (3.f * u3 - 6.f * u2 + 4.f) * v1 +
                    (-3.f * u3 + 3.f * u2 + 3.f * u + 1.f) * v2 +
                    u3 * v3;
        c *= multiplier;

        Float radius = (-u3 + 3.f * u2 - 3.f * u + 1.f) * r0 +
                       (3.f * u3 - 6.f * u2 + 4.f) * r1 +
                       (-3.f * u3 + 3.f * u2 + 3.f * u + 1.f) * r2 +
                       u3 * r3;
        radius *= multiplier;

        Vector3f dc_du =
            (-3.f * u2 + 6.f * u - 3.f) * v0 +
            (9.f * u2 - 12.f * u) * v1 +
            (-9.f * u2 + 6.f * u + 3.f) * v2 +
            (3.f * u2) * v3;
        dc_du *= multiplier;

        Float dr_du = (-3.f * u2 + 6.f * u - 3.f) * r0 +
            (9.f * u2 - 12.f * u) * r1 +
            (-9.f * u2 + 6.f * u + 3.f) * r2 +
            (3.f * u2) * r3;
        dr_du *= multiplier;

        Vector3f dc_duu =
            (-1.f * u + 1.f) * v0 +
            (3.f * u  - 2.f) * v1 +
            (-3.f * u + 1.f) * v2 +
            (1.f * u) * v3;

        Float dr_duu =
            (-1.f * u + 1.f) * r0 +
            (3.f * u  - 2.f) * r1 +
            (-3.f * u + 1.f) * r2 +
            (1.f * u) * r3;

        return { c, dc_du, dc_duu, radius, dr_du, dr_duu};
    }

    std::tuple<Vector3f, Vector3f>
    local_frame(const Vector3f &dc_du_normalized) const {
        // Define consistent local frame
        // (1) Consistently define a rotation axis (`v_rot`) that lies in the hemisphere defined by `guide`
        // (2) Rotate `dc_du` by 90 degrees on `v_rot` to obtain `v_rad`
        Vector3f guide = Vector3f(0, 0, 1);
        Vector3f v_rot = dr::normalize(
            guide - dc_du_normalized * dr::dot(dc_du_normalized, guide));
        Mask singular_mask =
            dr::eq(dr::abs(dr::dot(guide, dc_du_normalized)), 1.f);
        dr::masked(v_rot, singular_mask) =
            Vector3f(0, 1, 0); // non-consistent at singular points
        Vector3f v_rad = dr::cross(v_rot, dc_du_normalized);

        return { v_rot, v_rad };
    }

private:
    ScalarBoundingBox3f m_bbox;

    ScalarSize m_control_point_count = 0;
    ScalarSize m_segment_count = 0;

    // storage for Embree & mitsuba
    mutable FloatStorage m_vertex_with_radius;
    mutable UInt32Storage m_indices;

    // separate storage of control points and per-vertex radius for OptiX
    mutable FloatStorage m_vertex;
    mutable FloatStorage m_radius;

    // for OptiX build input
#if defined(MI_ENABLE_CUDA)
    mutable void* m_vertex_buffer_ptr = nullptr;
    mutable void* m_radius_buffer_ptr = nullptr;
    mutable void* m_index_buffer_ptr = nullptr;
#endif
};

MI_IMPLEMENT_CLASS_VARIANT(BSplineCurve, Shape)
MI_EXPORT_PLUGIN(BSplineCurve, "B-spline curve intersection primitive");
NAMESPACE_END(mitsuba)
