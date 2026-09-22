#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/scene_ir.h>

#include "curve.h"

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
   - Filename of the curves to be loaded, either a text file or a
     ``.packed`` container

 * - index
   - |int|
   - Index of the entry to load from a ``.packed`` container. (Default: 0)

 * - name
   - |string|
   - Alternatively, the name of the entry to load from a ``.packed``
     container.

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

Large sets of curves are better stored in a ``.packed`` container, which
holds the control points in a compressed binary form that loads considerably
faster than the text format. The :ref:`file format description
<sec-packed-curve>` documents the layout of a curve entry. When
:monosp:`filename` refers to a container, the ``index`` or ``name``
parameter selects the entry.

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
            'to_world': mi.ScalarAffineTransform4f().scale([2, 2, 2]).translate([1, 0, 0]),
            'filename': 'curves.txt'
        },

.. note:: The backfaces of curves are always culled. It is therefore impossible
          to intersect the curve with a ray that's origin is inside of the curve.
*/

template <typename Float, typename Spectrum>
class LinearCurve final : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_to_world, m_shape_type, initialize, mark_dirty,
                   get_children_string)
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

        ScopedPhase phase(ProfilerPhase::LoadGeometry);
        Timer timer;

        constexpr JitBackend Backend = dr::backend_v<Float>;

        CurveData data = load_curves(Backend, props, m_to_world.scalar(), 2);
        m_control_point_count = (ScalarSize) data.control_point_count();

        // Every curve with n control points contributes n - 1 segments
        size_t curve_count = data.curve_count(),
               segment_count = m_control_point_count - curve_count;
        drjit::unique_buffer<ScalarIndex> indices(Backend, segment_count,
                                                  /* shared = */ true);
        size_t segment_index = 0;
        for (size_t i = 0; i < curve_count; ++i)
            for (uint32_t j = data.offsets[i]; j + 1 < data.offsets[i + 1]; ++j)
                indices[segment_index++] = (ScalarIndex) j;

        // Reduce on the host while the control points are still staged there
        m_bbox = reduce_bbox_host<ScalarPoint3f, 4, 3>(
            data.control_points.data(), m_control_point_count);

        m_indices = adopt<UInt32Storage>(indices);
        m_control_points = adopt<FloatStorage>(data.control_points);

        Log(Debug, "\"%s\": read %i control points (%s in %s)",
            data.name, m_control_point_count,
            util::mem_string(m_control_point_count * 4 * sizeof(InputFloat)),
            util::time_string((float) timer.value())
        );

        m_shape_type = ShapeType::LinearCurve;

        initialize();
    }

    ScalarSize primitive_count() const override { return (ScalarSize) dr::width(m_indices); }

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        bool shading = has_flag(ray_flags, RayFlags::Shading);

        // If necessary, temporally suspend gradient tracking for all shape
        // parameters to construct a surface interaction completely detached
        // from the shape.
        dr::suspend_grad<Float> scope(
            has_flag(ray_flags, RayFlags::DetachShape), m_control_points);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

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

        Vector3f axis = dr::normalize(p1 - p0);

        Vector3f u_rot, u_rad;
        std::tie(u_rot, u_rad) = local_frame(axis);

        Point3f c = p0 * (1.f - v_local) + p1 * v_local;

        si.t = pi.t;
        si.p = dr::detach(ray(pi.t));

        Vector3f rad_vec_d = si.p - dr::detach(c);
        si.n = dr::normalize(rad_vec_d);
        si.p_err = ray_error(ray, pi.t, si.n);

        // Surface position at the detached parameterization: the offset from the
        // center line is held fixed in the curve's frame, so that it rotates
        // along with the segment.
        Point3f p_att = c + dr::dot(rad_vec_d, dr::detach(u_rad)) * u_rad +
                            dr::dot(rad_vec_d, dr::detach(u_rot)) * u_rot +
                            dr::dot(rad_vec_d, dr::detach(axis))  * axis;

        si.attach_motion(ray, p_att, ray_flags);

        if constexpr (dr::is_diff_v<Float>) {
            if (!has_flag(ray_flags, RayFlags::FollowShape)) {
                // Let the curve parameter follow the sliding of the interaction
                // point across the moving surface
                Vector3f dp_dv = dr::detach(
                    (p1 - p0) + (c1.w() - c0.w()) * dr::normalize(rad_vec_d));

                v_local = dr::replace_grad(
                    v_local, v_local + dr::dot(si.p - p_att, dp_dv) /
                                           dr::squared_norm(dp_dv));

                // Recompute the center line with the correct motion
                c = p0 * (1.f - v_local) + p1 * v_local;
            }
        }

        si.n = dr::normalize(si.p - c);

        // Embree and OptiX cull linear-curve backfaces at trace time; Metal's
        // HW intersector reports both sides. Drop inside hits to match (a no-op
        // on backends that already cull).
        this->cull_backface(si, ray, active);

        if (shading) {
            Vector3f rad_vec_normalized = dr::normalize(si.p - c);

            Float u = dr::atan2(dr::dot(u_rot, rad_vec_normalized),
                                dr::dot(u_rad, rad_vec_normalized));
            u += dr::select(u < 0.f, dr::TwoPi<Float>, 0.f);
            u *= dr::InvTwoPi<Float>;
            Float v = (v_local + prim_idx) / dr::width(m_indices);

            si.uv = Point2f(u, v);

            // Tangents of the (u, v) parameterization: ``u`` runs around the
            // curve, ``v`` along all segments.
            Float segment_count = (Float) dr::width(m_indices);
            si.dp_du = dr::TwoPi<Float> * dr::cross(axis, si.p - c);
            si.dp_dv = segment_count *
                       ((p1 - p0) + (c1.w() - c0.w()) * rad_vec_normalized);

            si.sh_frame.n = si.n;
            si.sh_frame.s = si.dp_du;
        }

        si.prim_index = pi.prim_index;
        si.shape    = this;
        si.instance_index = 0;

        return si;
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("control_point_count", m_control_point_count, ParamFlags::NonDifferentiable);
        cb->put("segment_indices",     m_indices,             ParamFlags::NonDifferentiable);
        cb->put("control_points",      m_control_points,      ParamFlags::NonDifferentiable);
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

    void describe(ShapeIR &g) const override {
        Base::describe(g);
        g.kind = ShapeIR::Kind::LinearCurve;
        g.cp_count = (size_t) m_control_point_count;
        g.seg_count = (size_t) dr::width(m_indices);
        g.cp_ptr  = m_control_points.data();
        g.seg_ptr = m_indices.data();
    }

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

    MI_DECLARE_CLASS(LinearCurve)

private:
    void recompute_bbox() {
        m_bbox = reduce_bbox<
            /* Type = */ ScalarPoint3f,
            /* Stride = */ 4,
            /* RadiusOffset = */ 3>(m_control_points, m_control_point_count);
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

    MI_TRAVERSE_CB(Base, m_indices, m_control_points)
};

MI_EXPORT_PLUGIN(LinearCurve)
NAMESPACE_END(mitsuba)
