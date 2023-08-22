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

/**!

.. _shape-bsplinecurve:

B-spline curve (:monosp:`bsplinecurve`)
-------------------------------------------------

.. pluginparameters::
 :extra-rows: 2

 * - filename
   - |string|
   - Filename of the curves to be loaded

 * - to_world
   - |transform|
   - Specifies a linear object-to-world transformation. Note that the control
     points' raddii are invariant to this transformation!

 * - control_point_count
   - |int|
   - Total number of control points
   - |exposed|

 * - segment_indices
   - :paramtype:`uint32[]`
   - Starting indices of a B-Spline segment
   - |exposed|

 * - control_points
   - :paramtype:`float[]`
   - Flattened control points buffer pre-multiplied by the object-to-world transformation.
     Each control point in the buffer is structured as follows: position_x, position_y, position_z, radius
   - |exposed|, |differentiable|, |discontinuous|

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/shape_bsplinecurve_basic.jpg
   :caption: Basic example
.. subfigure:: ../../resources/data/docs/images/render/shape_bsplinecurve_parameterization.jpg
   :caption: A textured B-spline curve with the default parameterization
.. subfigend::
   :label: fig-bsplinecurve

This shape plugin describes multiple cubic B-spline curves. They are hollow
cylindrical tubes which can have varying radii along their length and are
open-ended: they do not have endcaps. They can be made watertight by setting the
radii of the extremities to 0. This shape should always be preferred over curve
approximations modeled using triangles.

Although it is possible to define multiple curves as multiple separate objects,
this plugin was intended to be used as an aggregate of curves. Of course,
if the individual curves need different materials or other individual
characteristics they need to be defined in separate objects.

The file from which curves are loaded defines a single control point per line
using four real numbers. The first three encode the position and the last one is
the radius of the control point. At least four control points need to be
specified for a single curve. Empty lines between control points are used to
indicate the beginning of a new curve. Here is an example of two curves, the
first with 4 control points and static radii and the second with 6 control
points and increasing radii::

    -1.0 0.1 0.1 0.5
    -0.3 1.2 1.0 0.5
     0.3 0.3 1.1 0.5
     1.0 1.4 1.2 0.5

    -1.0 5.0 2.2 1
    -2.3 4.0 2.3 2
     3.3 3.0 2.2 3
     4.0 2.0 2.3 4
     4.0 1.0 2.2 5
     4.0 0.0 2.3 6

.. tabs::
    .. code-tab:: xml
        :name: bsplinecurve

        <shape type="bsplinecurve">
            <transform name="to_world">
                <scale value="2"/>
                <translate x="1" y="0" z="0"/>
            </transform>
            <string name="filename" type="curves.txt"/>
        </shape>

    .. code-tab:: python

        'curves': {
            'type': 'bsplinecurve',
            'to_world': mi.ScalarTransform4f.translate([1, 0, 0]).scale([2, 2, 2]),
            'filename': 'curves.txt'
        },

.. note:: The backfaces of the curves are culled. It is therefore impossible to
          intersect the curve with a ray that's origin is inside of the curve.
          In addition, prior to the NVIDIA v531.18 drivers for Windows and
          v530.30.02 drivers for Linux, `important inconsistencies <https://forums.developer.nvidia.com/t/orthographic-camera-with-b-spline-curves/238650>`_
          in the ray intersection code have been identified.
          We recommend updating to newer drivers.
 */

template <typename Float, typename Spectrum>
class BSplineCurve final : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_to_world, m_is_instance, initialize,
                   mark_dirty, get_children_string, parameters_grad_enabled)
    MI_IMPORT_TYPES()

    using typename Base::ScalarIndex;
    using typename Base::ScalarSize;

    using InputFloat = float;
    using InputPoint3f = dr::replace_scalar_t<ScalarPoint3f, InputFloat>;
    using FloatStorage = DynamicBuffer<dr::replace_scalar_t<Float, InputFloat>>;

    using UInt32Storage = DynamicBuffer<UInt32>;

    BSplineCurve(const Properties &props) : Base(props) {
#if !defined(MI_ENABLE_EMBREE)
        if constexpr (!dr::is_jit_v<Float>)
            Throw("The B-spline curve is only available with Embree in scalar "
                  "variants!");
#endif

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        std::string m_name = file_path.filename().string();

        // used for throwing an error later
        auto fail = [&](const char *descr, auto... args) {
            Throw(("Error while loading B-spline curve(s) from \"%s\": " + std::string(descr))
                      .c_str(), m_name, args...);
        };

        Log(Debug, "Loading B-spline curve(s) from \"%s\" ..", m_name);
        if (!fs::exists(file_path))
            fail("file not found!");

        ref<MemoryMappedFile> mmap = new MemoryMappedFile(file_path);
        ScopedPhase phase(ProfilerPhase::LoadGeometry);

        // Temporary buffers for vertices and radius
        std::vector<InputPoint3f> vertices;
        std::vector<InputFloat> radius;
        ScalarSize vertex_guess = (ScalarSize) mmap->size() / 100;
        vertices.reserve(vertex_guess);
        radius.reserve(vertex_guess);

        // Load data from the given file
        const char *ptr = (const char *) mmap->data();
        const char *eof = ptr + mmap->size();
        char buf[1025];
        Timer timer;

        size_t segment_count = 0;
        std::vector<size_t> curve_1st_idx;
        curve_1st_idx.reserve(vertex_guess / 4);
        bool new_curve = true;

        auto finish_curve = [&]() {
            if (!new_curve) {
                size_t num_control_points = vertices.size() - curve_1st_idx[curve_1st_idx.size() - 1];
                if (unlikely((num_control_points < 4) && (num_control_points > 0)))
                    fail("B-spline curves must have at least four control points!");
                if (likely(num_control_points > 0))
                    segment_count += (num_control_points - 3);
            }
        };

        while (ptr < eof) {
            // Determine the offset of the next newline
            const char *next = ptr;
            advance<false>(&next, eof, "\n");

            // Copy buf into a 0-terminated buffer
            ScalarSize size = (ScalarSize) (next - ptr);
            if (size >= sizeof(buf) - 1)
                fail("file contains an excessively long line! (%i characters)!", size);
            memcpy(buf, ptr, size);
            buf[size] = '\0';

            // Skip whitespace(s)
            const char *cur = buf, *eol = buf + size;
            advance<true>(&cur, eol, " \t\r");
            bool parse_error = false;

            // Empty line
            if (*cur == '\0') {
                finish_curve();
                new_curve = true;
                ptr = next + 1;
                continue;
            }

            // Handle current line: v.x v.y v.z radius
            if (new_curve) {
                curve_1st_idx.push_back(vertices.size());
                new_curve = false;
            }

            // Vertex position
            InputPoint3f p;
            for (ScalarSize i = 0; i < 3; ++i) {
                const char *orig = cur;
                p[i] = string::strtof<InputFloat>(cur, (char **) &cur);
                parse_error |= cur == orig;
            }
            p = m_to_world.scalar().transform_affine(p);

            // Vertex radius
            InputFloat r;
            const char *orig = cur;
            r = string::strtof<InputFloat>(cur, (char **) &cur);
            parse_error |= cur == orig;

            if (unlikely(!all(dr::isfinite(p))))
                fail("B-spline control point contains invalid position data (line: \"%s\")!", buf);
            if (unlikely(!dr::isfinite(r)))
                fail("B-spline control point contains invalid radius data (line: \"%s\")!", buf);

            vertices.push_back(p);
            radius.push_back(r);

            if (unlikely(parse_error))
                fail("Could not parse line \"%s\"!", buf);
            ptr = next + 1;
        }
        if (curve_1st_idx.size() == 0)
            fail("Empty B-spline file: no control points were read!");
        finish_curve();

        m_control_point_count = (ScalarSize) vertices.size();

        std::unique_ptr<ScalarIndex[]> indices = std::make_unique<ScalarIndex[]>(segment_count);
        size_t segment_index = 0;
        for (size_t i = 0; i < curve_1st_idx.size(); ++i) {
            size_t next_curve_idx = i + 1 < curve_1st_idx.size() ? curve_1st_idx[i + 1] : vertices.size();
            size_t curve_segment_count = next_curve_idx - curve_1st_idx[i] - 3;
            for (size_t j = 0; j < curve_segment_count; ++j)
                indices[segment_index++] = (ScalarIndex) (curve_1st_idx[i] + j);
        }
        m_indices = dr::load<UInt32Storage>(indices.get(), segment_count);

        std::unique_ptr<InputFloat[]> positions =
            std::make_unique<InputFloat[]>(m_control_point_count * 3);
        for (ScalarIndex i = 0; i < vertices.size(); i++) {
            InputFloat *vertex_ptr = positions.get() + i * 3;
            dr::store(vertex_ptr, vertices[i]);
        }

        // Merge buffers into m_control_points
        m_control_points = dr::empty<FloatStorage>(m_control_point_count * 4);
        FloatStorage vertex_buffer = dr::load<FloatStorage>(positions.get(), m_control_point_count * 3);
        FloatStorage radius_buffer = dr::load<FloatStorage>(radius.data(), m_control_point_count * 1);

        if constexpr (dr::is_jit_v<Float>) {
            DynamicBuffer<UInt32> idx = dr::arange<DynamicBuffer<UInt32>>(m_control_point_count);
            dr::scatter(m_control_points, dr::gather<FloatStorage>(vertex_buffer, idx * 3u + 0u), idx * 4u + 0u);
            dr::scatter(m_control_points, dr::gather<FloatStorage>(vertex_buffer, idx * 3u + 1u), idx * 4u + 1u);
            dr::scatter(m_control_points, dr::gather<FloatStorage>(vertex_buffer, idx * 3u + 2u), idx * 4u + 2u);
            dr::scatter(m_control_points, dr::gather<FloatStorage>(radius_buffer, idx * 1u + 0u), idx * 4u + 3u);
        } else {
            for (size_t i = 0; i < m_control_point_count; ++i) {
                m_control_points[i * 4 + 0] = vertex_buffer[i * 3 + 0];
                m_control_points[i * 4 + 1] = vertex_buffer[i * 3 + 1];
                m_control_points[i * 4 + 2] = vertex_buffer[i * 3 + 2];
                m_control_points[i * 4 + 3] = radius_buffer[i * 1 + 0];
            }
        }

        // Compute bounding box
        m_bbox.reset();
        for (ScalarSize i = 0; i < m_control_point_count; ++i) {
            ScalarPoint3f p(positions[3 * i + 0], positions[3 * i + 1],
                            positions[3 * i + 2]);
            ScalarFloat r(radius[i]);
            m_bbox.expand(p + r * ScalarVector3f(-1, 0, 0));
            m_bbox.expand(p + r * ScalarVector3f(1, 0, 0));
            m_bbox.expand(p + r * ScalarVector3f(0, -1, 0));
            m_bbox.expand(p + r * ScalarVector3f(0, 1, 0));
            m_bbox.expand(p + r * ScalarVector3f(0, 0, -1));
            m_bbox.expand(p + r * ScalarVector3f(0, 0, 1));
        }

        ScalarSize control_point_bytes = 4 * sizeof(InputFloat);
        Log(Debug, "\"%s\": read %i control points (%s in %s)",
            m_name, m_control_point_count,
            util::mem_string(m_control_point_count * control_point_bytes),
            util::time_string((float) timer.value())
        );

        initialize();
    }

    ScalarSize primitive_count() const override { return (ScalarSize) dr::width(m_indices); }

    SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                               uint32_t ray_flags,
                                               Mask active) const override {
        PreliminaryIntersection3f pi = dr::zeros<PreliminaryIntersection3f>();
        Float eps = dr::Epsilon<Float>;

        // Convert global v to segment-local v
        Float v_global = uv.y();
        size_t segment_count = dr::width(m_indices);
        UInt32 segment_id = dr::floor2int<UInt32>(v_global * segment_count);
        Float v_local = v_global * segment_count - segment_id;

        pi.prim_uv.x() = v_local;
        pi.prim_uv.y() = 0;
        pi.prim_index = segment_id;
        pi.shape = this;
        dr::masked(pi.t, active) = eps * 10;

        /* Create a ray at the intersection point and offset it by epsilon in
         the direction of the local surface normal */
        Point3f c;
        Vector3f dc_dv, dc_dvv;
        Float radius, dr_dv;
        std::tie(c, dc_dv, dc_dvv, std::ignore, radius, dr_dv, std::ignore) =
            cubic_interpolation(v_local, segment_id, active);
        Vector3f dc_dv_normalized = dr::normalize(dc_dv);

        Vector3f u_rot, u_rad;
        std::tie(u_rot, u_rad) = local_frame(dc_dv_normalized);

        auto [sin_u, cos_u] = dr::sincos(uv.x() * dr::TwoPi<Float>);
        Point3f o = c + cos_u * u_rad * (radius + pi.t) + sin_u * u_rot * (radius + pi.t);

        Vector3f rad_vec = o - c;
        Normal3f d = -dr::normalize(dr::norm(dc_dv) * rad_vec - (dr_dv * radius) * dc_dv_normalized);

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
        bool need_dn_duv = has_flag(ray_flags, RayFlags::dNSdUV) ||
                           has_flag(ray_flags, RayFlags::dNGdUV);
        bool need_dp_duv  = has_flag(ray_flags, RayFlags::dPdUV) || need_dn_duv;
        bool need_uv      = has_flag(ray_flags, RayFlags::UV) || need_dp_duv;
        bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape);
        bool follow_shape = has_flag(ray_flags, RayFlags::FollowShape);
        bool need_boundary_test = has_flag(ray_flags, RayFlags::BoundaryTest);

        /* If necessary, temporally suspend gradient tracking for all shape
           parameters to construct a surface interaction completely detach from
           the shape. */
        dr::suspend_grad<Float> scope(detach_shape, m_control_points);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

        Float v_local = pi.prim_uv.x();
        UInt32 prim_idx = pi.prim_index;

        Point3f c;
        Vector3f dc_dv, dc_dvv;
        Float radius, dr_dv;
        std::tie(c, dc_dv, dc_dvv, std::ignore, radius, dr_dv, std::ignore) =
            cubic_interpolation(v_local, prim_idx, active);
        Vector3f dc_dv_normalized = dr::normalize(dc_dv);

        Vector3f u_rot, u_rad;
        std::tie(u_rot, u_rad) = local_frame(dc_dv_normalized);

        if constexpr (IsDiff) {
            // Compute attached interaction point (w.r.t curve parameters)
            Point3f p = ray(pi.t);
            Vector3f rad_vec = p - c;
            Vector3f rad_vec_normalized = dr::normalize(rad_vec);

            Float u = dr::atan2(dr::dot(u_rot, rad_vec_normalized),
                                dr::dot(u_rad, rad_vec_normalized));
            u += dr::select(u < 0.f, dr::TwoPi<Float>, 0.f);
            u *= dr::InvTwoPi<Float>;
            u = dr::detach(u); // u has no motion

            auto [sin_v, cos_v] = dr::sincos(u * dr::TwoPi<Float>);
            Point3f p_diff = c + cos_v * u_rad * radius + sin_v * u_rot * radius;
            p = dr::replace_grad(p, p_diff);

            if (follow_shape) {
                /* FollowShape glues the interaction point with the shape.
                   Therefore, to also account for a possible differential motion
                   of the shape, the interaction point must be completely
                   differentiable w.r.t. the curve parameters. */
                si.p = p;
                Float t_diff = dr::sqrt(dr::squared_norm(si.p - ray.o) /
                                        dr::squared_norm(ray.d));
                si.t = dr::replace_grad(pi.t, t_diff);
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
                Float correction = dr::dot(rad_vec, dc_dvv);
                Vector3f n = dr::normalize(
                    (dr::squared_norm(dc_dv) - correction) * rad_vec -
                    (dr_dv * radius) * dc_dv
                );

                // Tangent plane intersection
                Float t_diff = dr::dot(p - ray.o, n) / dr::dot(n, ray.d);
                si.t = dr::replace_grad(pi.t, t_diff);
                si.p = ray(si.t);

                // Compute `v_local` with correct (hit point) motion
                Float v_global = (v_local + prim_idx) / dr::width(m_indices);
                Vector3f dp_dv;
                std::tie(std::ignore, dp_dv, std::ignore, std::ignore) =
                    partials(Point2f(u, v_global), active);
                dp_dv = dr::detach(dp_dv);
                Float dp_dv_sqrnorm = dr::squared_norm(dp_dv);
                Float v_diff = dr::dot(si.p - p_diff, dp_dv) / dp_dv_sqrnorm;
                v_global = dr::replace_grad(v_global, v_diff);
                Float v_local_diff = v_global * dr::width(m_indices) - prim_idx;;
                v_local =  dr::replace_grad(v_local, v_local_diff);

                // Recompute values with new `v_local` motion
                std::tie(c, dc_dv, dc_dvv, std::ignore, radius, dr_dv, std::ignore) =
                    cubic_interpolation(v_local, prim_idx, active);
                dc_dv_normalized = dr::normalize(dc_dv);
                std::tie(u_rot, u_rad) = local_frame(dc_dv_normalized);
            }
        } else {
            si.t = pi.t;
            si.p = ray(si.t);
        }

        si.t = dr::select(active, si.t, dr::Infinity<Float>);

        // Normal
        Vector3f rad_vec = si.p - c;
        Vector3f rad_vec_normalized = dr::normalize(rad_vec);
        Float correction = dr::dot(rad_vec, dc_dvv);  // curvature correction
        Normal3f n = dr::normalize(
            (dr::squared_norm(dc_dv) - correction) * rad_vec -
            (dr_dv * radius) * dc_dv
        );
        si.n = si.sh_frame.n = n;

        if (need_uv) {
            Float u = dr::atan2(dr::dot(u_rot, rad_vec_normalized),
                                dr::dot(u_rad, rad_vec_normalized));
            u += dr::select(u < 0.f, dr::TwoPi<Float>, 0.f);
            u *= dr::InvTwoPi<Float>;
            Float v = (v_local + prim_idx) / dr::width(m_indices);

            si.uv = Point2f(u, v);
        }

        if (need_dp_duv) {
            Vector3f dp_du, dp_dv, dn_du, dn_dv;
            std::tie(dp_du, dp_dv, dn_du, dn_dv) =
                partials(si.uv, active);
            si.dp_du = dp_du;
            si.dp_dv = dp_dv;
            if (need_dn_duv) {
                si.dn_du = dn_du;
                si.dn_dv = dn_dv;
            }
        }

        si.shape = this;
        si.instance = nullptr;

        if (need_boundary_test)
            si.boundary_test = dr::dot(si.sh_frame.n, -ray.d);

        return si;
    }

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
        callback->put_parameter("control_point_count", m_control_point_count, +ParamFlags::NonDifferentiable);
        callback->put_parameter("segment_indices",     m_indices,             +ParamFlags::NonDifferentiable);
        callback->put_parameter("control_points",      m_control_points,       ParamFlags::Differentiable | ParamFlags::Discontinuous);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "control_points")) {
            recompute_bbox();
            mark_dirty();
        }
        Base::parameters_changed();
    }

    bool parameters_grad_enabled() const override {
        return dr::grad_enabled(m_control_points);
    }

#if defined(MI_ENABLE_EMBREE)
    RTCGeometry embree_geometry(RTCDevice device) override {
        dr::eval(m_control_points); // Make sure the buffer is evaluated
        RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_ROUND_BSPLINE_CURVE);
        rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT4,
                                   m_control_points.data(), 0, 4 * sizeof(InputFloat),
                                   m_control_point_count);
        rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT,
                                   m_indices.data(), 0, 1 * sizeof(ScalarIndex),
                                   dr::width(m_indices));
        rtcCommitGeometry(geom);
        return geom;
    }
#endif

#if defined(MI_ENABLE_CUDA)
    void optix_prepare_geometry() override { }

    void optix_build_input(OptixBuildInput &build_input) const override {
        dr::eval(m_control_points); // Make sure the buffer is evaluated
        m_vertex_buffer_ptr = (CUdeviceptr*) m_control_points.data();
        m_radius_buffer_ptr = (CUdeviceptr*) (m_control_points.data() + 3);
        m_index_buffer_ptr  = (CUdeviceptr*) m_indices.data();

        build_input.type                            = OPTIX_BUILD_INPUT_TYPE_CURVES;
        build_input.curveArray.curveType            = OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE;
        build_input.curveArray.numPrimitives        = (unsigned int) dr::width(m_indices);

        build_input.curveArray.vertexBuffers        = (CUdeviceptr*) &m_vertex_buffer_ptr;
        build_input.curveArray.numVertices          = m_control_point_count;
        build_input.curveArray.vertexStrideInBytes  = sizeof( InputFloat ) * 4;

        build_input.curveArray.widthBuffers         = (CUdeviceptr*) &m_radius_buffer_ptr;
        build_input.curveArray.widthStrideInBytes   = sizeof( InputFloat ) * 4;

        build_input.curveArray.indexBuffer          = (CUdeviceptr) m_index_buffer_ptr;
        build_input.curveArray.indexStrideInBytes   = sizeof( ScalarIndex );

        build_input.curveArray.normalBuffers        = 0;
        build_input.curveArray.normalStrideInBytes  = 0;
        build_input.curveArray.flag                 = OPTIX_GEOMETRY_FLAG_NONE;
        build_input.curveArray.primitiveIndexOffset = 0;
        build_input.curveArray.endcapFlags          = OPTIX_CURVE_ENDCAP_DEFAULT;
    }
#endif

    bool is_bspline_curve() const override {
        return true;
    }

    ScalarBoundingBox3f bbox() const override {
        return m_bbox;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "BSpline[" << std::endl
            << "  control_point_count = " << m_control_point_count << "," << std::endl
            << "  segment_count = " << dr::width(m_indices) << "," << std::endl
            << "  " << string::indent(get_children_string()) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

private:
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

    void recompute_bbox() {
        auto&& control_points = dr::migrate(m_control_points, AllocType::Host);
        if constexpr (dr::is_jit_v<Float>)
            dr::sync_thread();
        const InputFloat *ptr = control_points.data();

        m_bbox.reset();
        for (ScalarSize i = 0; i < m_control_point_count; ++i) {
            ScalarPoint3f p(ptr[4 * i + 0], ptr[4 * i + 1], ptr[4 * i + 2]);
            ScalarFloat r(ptr[4 * i + 3]);
            m_bbox.expand(p + r * ScalarVector3f(-1, 0, 0));
            m_bbox.expand(p + r * ScalarVector3f(1, 0, 0));
            m_bbox.expand(p + r * ScalarVector3f(0, -1, 0));
            m_bbox.expand(p + r * ScalarVector3f(0, 1, 0));
            m_bbox.expand(p + r * ScalarVector3f(0, 0, -1));
            m_bbox.expand(p + r * ScalarVector3f(0, 0, 1));
        }
    }

    std::tuple<Point3f, Vector3f, Vector3f, Vector3f, Float, Float, Float>
    cubic_interpolation(const Float v, const UInt32 prim_idx, Mask active) const {
        UInt32 idx = dr::gather<UInt32>(m_indices, prim_idx, active);
        Point4f c0 = dr::gather<Point4f>(m_control_points, idx + 0, active),
                c1 = dr::gather<Point4f>(m_control_points, idx + 1, active),
                c2 = dr::gather<Point4f>(m_control_points, idx + 2, active),
                c3 = dr::gather<Point4f>(m_control_points, idx + 3, active);
        Point3f p0 = Point3f(c0.x(), c0.y(), c0.z()),
                p1 = Point3f(c1.x(), c1.y(), c1.z()),
                p2 = Point3f(c2.x(), c2.y(), c2.z()),
                p3 = Point3f(c3.x(), c3.y(), c3.z());
        Float r0 = c0.w(),
              r1 = c1.w(),
              r2 = c2.w(),
              r3 = c3.w();

        Float v2 = dr::sqr(v), v3 = v2 * v;
        Float multiplier = 1.f / 6.f;

        Point3f c = (-v3 + 3.f * v2 - 3.f * v + 1.f) * p0 +
                    (3.f * v3 - 6.f * v2 + 4.f) * p1 +
                    (-3.f * v3 + 3.f * v2 + 3.f * v + 1.f) * p2 +
                    v3 * p3;
        c *= multiplier;

        Vector3f dc_dv =
            (-3.f * v2 + 6.f * v - 3.f) * p0 +
            (9.f * v2 - 12.f * v) * p1 +
            (-9.f * v2 + 6.f * v + 3.f) * p2 +
            (3.f * v2) * p3;
        dc_dv *= multiplier;

        Vector3f dc_dvv =
            (-1.f * v + 1.f) * p0 +
            (3.f * v  - 2.f) * p1 +
            (-3.f * v + 1.f) * p2 +
            (1.f * v) * p3;

        Vector3f dc_dvvv = -p0 + 3.f * p1 - 3.f * p2 + p3;

        Float radius = (-v3 + 3.f * v2 - 3.f * v + 1.f) * r0 +
                       (3.f * v3 - 6.f * v2 + 4.f) * r1 +
                       (-3.f * v3 + 3.f * v2 + 3.f * v + 1.f) * r2 +
                       v3 * r3;
        radius *= multiplier;

        Float dr_dv = (-3.f * v2 + 6.f * v - 3.f) * r0 +
                      (9.f * v2 - 12.f * v) * r1 +
                      (-9.f * v2 + 6.f * v + 3.f) * r2 +
                      (3.f * v2) * r3;
        dr_dv *= multiplier;

        Float dr_dvv = (-1.f * v + 1.f) * r0 +
                       (3.f * v - 2.f) * r1 +
                       (-3.f * v + 1.f) * r2 +
                       (1.f * v) * r3;

        return { c, dc_dv, dc_dvv, dc_dvvv, radius, dr_dv, dr_dvv};
    }

    std::tuple<Vector3f, Vector3f, Vector3f, Vector3f>
    partials(Point2f uv, Mask active) const {
        /* To compute the partial devriatives of a point on the curve and of its
           normal, we start by building the Frenet-Serret (TNB) frame. From the
           frame we can compute the curves' firt fundamental form. The first
           fundamental form gives us the position partials. Finally, these are
           then used in the Weingarten equations to get the normal's partials.
         */
        Float v_global = uv.y();
        size_t segment_count = dr::width(m_indices);
        UInt32 segment_id = dr::floor2int<UInt32>(v_global * segment_count);
        Float v_local = v_global * segment_count - segment_id;

        Point3f C;
        Vector3f Cv, Cvv, Cvvv;
        Float radius, rv, rvv;
        std::tie(C, Cv, Cvv, Cvvv, radius, rv, rvv) =
            cubic_interpolation(v_local, segment_id, active);

        // Frenet-Serret (TNB) frame
        Float Cv_norm = dr::norm(Cv);
        Vector3f CvCvv = dr::cross(Cv, Cvv),
                 Cv_normalized = Cv / Cv_norm;
        Float Cv_sqrnorm = dr::sqr(Cv_norm),
              CvCvv_norm = dr::norm(CvCvv),
              kappa = CvCvv_norm / (Cv_norm * Cv_sqrnorm),
              tau = dr::dot(Cvvv, CvCvv) / dr::sqr(CvCvv_norm);
        Vector3f T = Cv / Cv_norm,
                 N = dr::normalize(dr::cross(CvCvv, Cv)),
                 B = dr::normalize(dr::cross(T, N));

        // Degenerated TNB frame
        Mask degenerate = kappa < dr::Epsilon<Float>;
        dr::masked(kappa, degenerate) = 0.f;
        dr::masked(tau, degenerate) = 0.f;
        Normal3f Tn(T);
        Frame3f frame(Tn);
        dr::masked(N, degenerate) = frame.s;
        dr::masked(B, degenerate) = frame.t;

        // Consistent local frame
        auto [dir_rot, dir_rad] = local_frame(Cv_normalized);
        auto [s_, c_] = dr::sincos(uv.x() * dr::TwoPi<Float>);
        Vector3f rad = c_ * dir_rad + s_ * dir_rot;
        Float cos_theta_u = dr::dot(N, rad),
              sin_theta_u = dr::dot(B, rad);
        Normal3f n = dr::normalize(
            Cv_norm * (1.f - radius * kappa * cos_theta_u) * rad - rv * T);

        // Position partials
        Vector3f radu  = -sin_theta_u * N + cos_theta_u * B,
                 radv  = Cv_norm * cos_theta_u * (-kappa * T + tau * B) + Cv_norm * sin_theta_u * (-tau * N),
                 radvv = Cv_sqrnorm * cos_theta_u * (-kappa * kappa - tau * tau) * N +
                         Cv_sqrnorm * sin_theta_u * (kappa * tau * T - tau * tau * B),
                 raduv = -Cv_norm * sin_theta_u * (-kappa * T + tau * B) + Cv_norm * cos_theta_u * (-tau * N);
        Vector3f Pu  = radius * radu,
                 Pv  = Cv + rv * rad + radius * radv,
                 Puu = -radius * rad,
                 Pvv = Cvv + rvv * rad + 2 * rv * radv + radius * radvv,
                 Puv = rv * radu + radius * raduv;

        // Rescale (u: [0, 1) -> [0, 2pi), v: local -> global)
        Pu *= dr::TwoPi<Float>;
        Puv *= dr::TwoPi<Float>;
        Puu *= dr::sqr(dr::TwoPi<Float>);
        ScalarFloat ratio = (ScalarFloat) dr::width(m_indices),
                    ratio2 = ratio * ratio;
        Pv  *= ratio;
        Puv *= ratio;
        Pvv *= ratio2;

        // Fundamental form
        Float E = dr::squared_norm(Pu),
              F = dr::dot(Pu, Pv),
              G = dr::squared_norm(Pv),
              e = dr::dot(n, Puu),
              f = dr::dot(n, Puv),
              g = dr::dot(n, Pvv);

        // Normal partials
        Float detI = E * G - F * F;
        Vector3f Nu = ((f * F - e * G) * Pu + (e * F - f * E) * Pv) / detI,
                 Nv = ((g * F - f * G) * Pu + (f * F - g * E) * Pv) / detI;

        return {Pu, Pv, Nu, Nv};
    }

    std::tuple<Vector3f, Vector3f>
    local_frame(const Vector3f &dc_dv_normalized) const {
        // Define consistent local frame
        // (1) Consistently define a rotation axis (`v_rot`) that lies in the hemisphere defined by `guide`
        // (2) Rotate `dc_du` by 90 degrees on `v_rot` to obtain `v_rad`
        Vector3f guide = Vector3f(0, 0, 1);
        Vector3f v_rot = dr::normalize(
            guide - dc_dv_normalized * dr::dot(dc_dv_normalized, guide));
        Mask singular_mask =
            dr::eq(dr::abs(dr::dot(guide, dc_dv_normalized)), 1.f);
        dr::masked(v_rot, singular_mask) =
            Vector3f(0, 1, 0); // non-consistent at singular points
        Vector3f v_rad = dr::cross(v_rot, dc_dv_normalized);

        return { v_rot, v_rad };
    }

private:
    ScalarBoundingBox3f m_bbox;

    ScalarSize m_control_point_count = 0;

    mutable UInt32Storage m_indices;
    mutable FloatStorage m_control_points;

#if defined(MI_ENABLE_CUDA)
    // For OptiX build input
    mutable CUdeviceptr* m_vertex_buffer_ptr = nullptr;
    mutable CUdeviceptr* m_radius_buffer_ptr = nullptr;
    mutable CUdeviceptr* m_index_buffer_ptr = nullptr;
#endif
};

MI_IMPLEMENT_CLASS_VARIANT(BSplineCurve, Shape)
MI_EXPORT_PLUGIN(BSplineCurve, "B-spline curve intersection primitive");
NAMESPACE_END(mitsuba)
