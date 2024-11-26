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

#include <drjit/texture.h>

#if defined(MI_ENABLE_EMBREE)
#include <embree3/rtcore.h>
#endif

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-linearcurve:

Linear curve (:monosp:`linearcurve`)
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
   - Starting indices of a linear segment
   - |exposed|

 * - control_points
   - :paramtype:`float[]`
   - Flattened control points buffer pre-multiplied by the object-to-world transformation.
     Each control point in the buffer is structured as follows: position_x, position_y, position_z, radius
   - |exposed|

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/shape_linearcurve_basic.jpg
   :caption: Basic example
.. subfigure:: ../../resources/data/docs/images/render/shape_linearcurve_parameterization.jpg
   :caption: A textured linear curve with the default parameterization
.. subfigend::
   :label: fig-linearcurve

This shape plugin describes multiple linear curves. They are hollow
cylindrical tubes which can have varying radii along their length. The linear
segments are connected by a smooth spherical joint, and they are also
terminated by a spherical endcap. This shape should always be preferred over
curve approximations modeled using triangles.

Although it is possible to define multiple curves as multiple separate objects,
this plugin was intended to be used as an aggregate of curves. Of course,
if the individual curves need different materials or other individual
characteristics they need to be defined in separate objects.

The file from which curves are loaded defines a single control point per line
using four real numbers. The first three encode the position and the last one is
the radius of the control point. At least two control points need to be
specified for a single curve. Empty lines between control points are used to
indicate the beginning of a new curve. Here is an example of two curves, the
first with 2 control points and static radii and the second with 4 control
points and increasing radii::

    -1.0 0.1 0.1 0.5
     1.0 1.4 1.2 0.5

    -1.0 5.0 2.2 1
    -2.3 4.0 2.3 2
     4.0 1.0 2.2 5
     4.0 0.0 2.3 6

.. tabs::
    .. code-tab:: xml
        :name: linearcurve

        <shape type="linearcurve">
            <transform name="to_world">
                <translate x="1" y="0" z="0"/>
                <scale value="2"/>
            </transform>
            <string name="filename" type="curves.txt"/>
        </shape>

    .. code-tab:: python

        'curves': {
            'type': 'linearcurve',
            'to_world': mi.ScalarTransform4f().scale([2, 2, 2]).translate([1, 0, 0]),
            'filename': 'curves.txt'
        },

.. note:: The backfaces of the curves are culled. It is therefore impossible to
          intersect the curve with a ray that's origin is inside of the curve.
 */

template <typename Float, typename Spectrum>
class LinearCurve final : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_to_world, m_to_object, m_is_instance, m_shape_type,
                   initialize, mark_dirty, get_children_string,
                   parameters_grad_enabled)
    MI_IMPORT_TYPES()

    using typename Base::ScalarIndex;
    using typename Base::ScalarSize;

    using InputFloat = float;
    using InputPoint3f  = Point<InputFloat, 3>;
    using InputVector3f = Vector<InputFloat, 3>;
    using FloatStorage = DynamicBuffer<dr::replace_scalar_t<Float, InputFloat>>;
    using UInt32Storage = DynamicBuffer<UInt32>;
    using Index = typename CoreAliases::UInt32;

    LinearCurve(const Properties &props) : Base(props) {
#if !defined(MI_ENABLE_EMBREE)
        if constexpr (!dr::is_jit_v<Float>)
            Throw("The linear curve is only available with Embree in scalar "
                  "variants!");
#endif

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        std::string m_name = file_path.filename().string();

        // used for throwing an error later
        auto fail = [&](const char *descr, auto... args) {
            Throw(("Error while loading linear curve(s) from \"%s\": " + std::string(descr))
                      .c_str(), m_name, args...);
        };

        Log(Debug, "Loading linear curve(s) from \"%s\" ..", m_name);
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
                if (unlikely((num_control_points < 2) && (num_control_points > 0)))
                    fail("Linear curves must have at least two control points!");
                if (likely(num_control_points > 0))
                    segment_count += (num_control_points - 1);
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
                fail("Control point contains invalid position data (line: \"%s\")!", buf);
            if (unlikely(!dr::isfinite(r)))
                fail("Control point contains invalid radius data (line: \"%s\")!", buf);

            vertices.push_back(p);
            radius.push_back(r);

            if (unlikely(parse_error))
                fail("Could not parse line \"%s\"!", buf);
            ptr = next + 1;
        }
        if (curve_1st_idx.size() == 0)
            fail("Empty curve file: no control points were read!");
        finish_curve();

        m_control_point_count = (ScalarSize) vertices.size();

        std::unique_ptr<ScalarIndex[]> indices = std::make_unique<ScalarIndex[]>(segment_count);
        size_t segment_index = 0;
        for (size_t i = 0; i < curve_1st_idx.size(); ++i) {
            size_t next_curve_idx = i + 1 < curve_1st_idx.size() ? curve_1st_idx[i + 1] : vertices.size();
            size_t curve_segment_count = next_curve_idx - curve_1st_idx[i] - 1;
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

        m_shape_type = ShapeType::LinearCurve;

        initialize();
    }

    ScalarSize primitive_count() const override { return (ScalarSize) dr::width(m_indices); }

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     uint32_t recursion_depth,
                                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        // Early exit when tracing isn't necessary
        if (!m_is_instance && recursion_depth > 0)
            return dr::zeros<SurfaceInteraction3f>();

        bool need_uv = has_flag(ray_flags, RayFlags::UV);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.t = dr::select(active, pi.t, dr::Infinity<Float>);
        si.p = ray(pi.t);

        Float v_local = pi.prim_uv.x();
        UInt32 prim_idx = pi.prim_index;

        // It seems that the `v_local` given by Embree and OptiX has already
        // taken into account the changing radius: `v_local` is shifted such
        // that the normal can be easily computed as `si.p - c`, 
        // where `c = (1 - c_local) * cp1 + v_local * cp2`
        UInt32 idx = dr::gather<UInt32>(m_indices, prim_idx, active);
        Point4f c0 = dr::gather<Point4f>(m_control_points, idx, active),
                c1 = dr::gather<Point4f>(m_control_points, idx + 1, active);
        Point3f p0 = Point3f(c0.x(), c0.y(), c0.z()),
                p1 = Point3f(c1.x(), c1.y(), c1.z());

        Vector3f u_rot, u_rad;
        std::tie(u_rot, u_rad) = local_frame(dr::normalize(p1 - p0));
        
        Point3f c = p0 * (1.f - v_local) + p1 * v_local;
        si.n = si.sh_frame.n = dr::normalize(si.p - c);

        if (need_uv) {
            Vector3f rad_vec = si.p - c;
            Vector3f rad_vec_normalized = dr::normalize(rad_vec);

            Float u = dr::atan2(dr::dot(u_rot, rad_vec_normalized),
                                dr::dot(u_rad, rad_vec_normalized));
            u += dr::select(u < 0.f, dr::TwoPi<Float>, 0.f);
            u *= dr::InvTwoPi<Float>;
            Float v = (v_local + prim_idx) / dr::width(m_indices);

            si.uv = Point2f(u, v);
        }

        si.shape    = this;
        si.instance = nullptr;

        return si;
    }

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
        callback->put_parameter("control_point_count", m_control_point_count, +ParamFlags::NonDifferentiable);
        callback->put_parameter("segment_indices",     m_indices,             +ParamFlags::NonDifferentiable);
        callback->put_parameter("control_points",      m_control_points,      +ParamFlags::NonDifferentiable);
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
        RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_ROUND_LINEAR_CURVE);

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

        build_input.type = OPTIX_BUILD_INPUT_TYPE_CURVES;
        build_input.curveArray.curveType = OPTIX_PRIMITIVE_TYPE_ROUND_LINEAR;
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

    ScalarBoundingBox3f bbox() const override {
        return m_bbox;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "LinearCurve[" << std::endl
            << "  control_point_count = " << m_control_point_count << "," << std::endl
            << "  segment_count = " << dr::width(m_indices) << "," << std::endl
            << "  " << string::indent(get_children_string()) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

private:
    template <bool Negate, ScalarSize N>
    void advance(const char **start_, const char *end, const char (&delim)[N]) {
        const char *start = *start_;

        while (true) {
            bool is_delim = false;
            for (ScalarSize i = 0; i < N; ++i)
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

    std::tuple<Vector3f, Vector3f>
    local_frame(const Vector3f &dc_dv_normalized) const {
        // Define consistent local frame
        // (1) Consistently define a rotation axis (`v_rot`) that lies in the hemisphere defined by `guide`
        // (2) Rotate `dc_du` by 90 degrees on `v_rot` to obtain `v_rad`
        Vector3f guide = Vector3f(0, 0, 1);
        Vector3f v_rot = dr::normalize(
            guide - dc_dv_normalized * dr::dot(dc_dv_normalized, guide));
        Mask singular_mask = dr::abs(dr::dot(guide, dc_dv_normalized)) == 1.f;
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
    mutable void* m_vertex_buffer_ptr = nullptr;
    mutable void* m_radius_buffer_ptr = nullptr;
    mutable void* m_index_buffer_ptr = nullptr;
#endif
};

MI_IMPLEMENT_CLASS_VARIANT(LinearCurve, Shape)
MI_EXPORT_PLUGIN(LinearCurve, "Linear curve intersection primitive");
NAMESPACE_END(mitsuba)

