#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/mmap.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/scene_ir.h>

#include "ellipsoids.h"

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-ellipsoids:

Ellipsoids (:monosp:`ellipsoids`)
-------------------------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Specifies the PLY file containing the ellipsoid centers, scales, and quaternions.
     This parameter cannot be used if `data` or `centers` are provided.

 * - data
   - |tensor|
   - A tensor of shape (N, 10) or (N * 10) that defines the ellipsoid centers, scales, and quaternions.
     This parameter cannot be used if `filename` or `centers` are provided.

 * - centers
   - |tensor|
   - A tensor of shape (N, 3) specifying the ellipsoid centers.
     This parameter cannot be used if `filename` or `data` are provided.

 * - scales
   - |tensor|
   - A tensor of shape (N, 3) specifying the ellipsoid scales.
     This parameter cannot be used if `filename` or `data` are provided.

 * - quaternions
   - |tensor|
   - A tensor of shape (N, 3) specifying the ellipsoid quaternions.
     This parameter cannot be used if `filename` or `data` are provided.

 * - scale_factor
   - |float|
   - A scaling factor applied to all ellipsoids when loading from a PLY file. (Default: 1.0)

 * - extent
   - |float|
   - Specifies the extent of the ellipsoid. This effectively acts as an
     extra scaling factor on the ellipsoid, without having to alter the scale
     parameters. (Default: 3.0)
   - |readonly|

 * - extent_adaptive_clamping
   - |float|
   - If True, use adaptive extent values based on the `opacities` attribute of
     the volumetric primitives. (Default: False)
   - |readonly|

 * - to_world
   - |transform|
   - Specifies an optional linear object-to-world transformation to apply to
     all ellipsoids.
   - |exposed|, |differentiable|, |discontinuous|

 * - (Nested plugin)
   - |tensor|
   - Specifies arbitrary ellipsoids attribute as a tensor of shape (N, D) with D
     the dimensionality of the attribute. For instance this can be used to define
     an opacity value for each ellipsoids, or a set of spherical harmonic coefficients
     as used in the :monosp:`volprim_rf_basic` integrator.

This shape plugin defines a point cloud of anisotropic ellipsoid primitives
using specified centers, scales, and quaternions. It employs a closed-form
ray-intersection formula with backface culling. Although it is slower than the
:monosp:`ellipsoidsmesh` shape plugin, which uses tessellated ellipsoids for
ray-triangle intersection hardware acceleration, it offers greater flexibility
in computing intersections.

This shape also exposes an `extent` parameter, it acts as an extra scaling
factor for the ellipsoids' scales. Typically, this is used to define the support
of a kernel function defined within the ellipsoid. For example, the
`scale` parmaters of the ellipsoid will define the variances of a gaussian and
the `extent` will multiple those value to define the effictive radius of the
ellipsoid. When `extent_adaptive_clamping` is enabled, the extent is
additionally multiplied by an opacity-dependent factor::math:`\sqrt{2 * \log(opacity / 0.01)} / 3`

It is designed for use with volumetric primitive integrators, as detailed in
:cite:`Condor2024Gaussians`.

.. tabs::
    .. code-tab:: xml
        :name: ellipsoids-sphere

        <shape type="ellipsoids">
            <string name="filename" value="my_primitives.ply"/>
        </shape>

    .. code-tab:: python

        'primitives': {
            'type': 'ellipsoids',
            'filename': 'my_primitives.ply'
        }
 */

template <typename Float, typename Spectrum>
class Ellipsoids final : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_to_world, m_is_instance, m_shape_type, initialize,
                   mark_dirty, get_children_string)
    MI_IMPORT_TYPES()

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using FloatStorage  = DynamicBuffer<dr::replace_scalar_t<Float, float>>;
    using BoolStorage   = DynamicBuffer<Mask>;
    using UInt32Storage = DynamicBuffer<UInt32>;
    using ArrayXf       = dr::DynamicArray<Float>;

    /// Box layout of the GPU acceleration structure builders
    struct PackedBoundingBox { float min[3], max[3]; };

    static constexpr bool IsGPU = dr::is_cuda_v<Float> || dr::is_metal_v<Float>;
    using BoundingBoxType =
        std::conditional_t<IsGPU, PackedBoundingBox, ScalarBoundingBox3f>;

    Ellipsoids(const Properties &props) : Base(props) {
        m_shape_type = ShapeType::Ellipsoids;

        Timer timer;

        m_ellipsoids = EllipsoidsData<Float, Spectrum>(props);

        size_t ellipsoids_count_bytes = EllipsoidStructSize * sizeof(float);
        Log(Debug, "Read %i ellipsoids (%s in %s)",
            m_ellipsoids.count(),
            util::mem_string(m_ellipsoids.count() * ellipsoids_count_bytes),
            util::time_string((float) timer.value())
        );

        recompute_bbox();
        initialize();
    }

    ~Ellipsoids() {
        jit_free(m_device_bboxes);
        jit_free(m_host_bboxes);
    }

    void traverse(TraversalCallback *cb) override {
        m_ellipsoids.traverse(cb);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        m_ellipsoids.parameters_changed();

        if (keys.empty() || string::contains(keys, "data")) {
            recompute_bbox();
            mark_dirty();
        }

        Base::parameters_changed(keys);
    }

    ScalarSize primitive_count() const override { return (ScalarSize) m_ellipsoids.count(); }

    ScalarBoundingBox3f bbox() const override { return m_bbox; }

    ScalarBoundingBox3f bbox(ScalarIndex prim_index) const override {
        if constexpr (IsGPU)
            Throw("bbox(ScalarIndex) is not available on the GPU backends!");
        Assert(prim_index <= primitive_count());
        auto bbox = ((BoundingBoxType*) m_host_bboxes)[prim_index];

        return ScalarBoundingBox3f(
            ScalarPoint3f(bbox.min[0], bbox.min[1], bbox.min[2]),
            ScalarPoint3f(bbox.max[0], bbox.max[1], bbox.max[2])
        );
    }

    // =============================================================

    // =============================================================
    // Sampling routines (not implemented!)
    // =============================================================

    PositionSample3f sample_position(Float, const Point2f &, Mask) const override { return dr::zeros<PositionSample3f>(); }

    Float pdf_position(const PositionSample3f &, Mask) const override { return 0; }

    DirectionSample3f sample_direction(const Interaction3f &, const Point2f &, Mask) const override { return dr::zeros<DirectionSample3f>(); }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &, Mask) const override { return 0; }

    SurfaceInteraction3f eval_parameterization(const Point2f &, uint32_t, Mask) const override { return dr::zeros<SurfaceInteraction3f>(); }

    // =============================================================

    // =============================================================
    // Attribute routines
    // =============================================================

    Mask has_attribute(std::string_view name, Mask active) const override {
        if (m_ellipsoids.has_attribute(name))
            return true;

        return Base::has_attribute(name, active);
    }

    Float eval_attribute_1(std::string_view name,
                           const SurfaceInteraction3f &si,
                           Mask active) const override {
        MI_MASK_ARGUMENT(active);
        try {
            return m_ellipsoids.eval_attribute_1(name, si, active);
        } catch (...) {
            return Base::eval_attribute_1(name, si, active);
        }
    }

    Color3f eval_attribute_3(std::string_view name,
                             const SurfaceInteraction3f &si,
                             Mask active) const override {
        MI_MASK_ARGUMENT(active);
        try {
            return m_ellipsoids.eval_attribute_3(name, si, active);
        } catch (...) {
            return Base::eval_attribute_3(name, si, active);
        }
    }

    ArrayXf eval_attribute_x(std::string_view name,
                             const SurfaceInteraction3f &si,
                             Mask active) const override {
        MI_MASK_ARGUMENT(active);
        try {
            return m_ellipsoids.eval_attribute_x(name, si, active);
        } catch (...) {
            return Base::eval_attribute_x(name, si, active);
        }
    }

    // =============================================================

    // =============================================================
    // Ray tracing routines
    // =============================================================

    template <typename FloatP, typename Ray3fP, typename Ellipsoid>
    std::pair<FloatP, dr::mask_t<FloatP>>
    ray_ellipsoid_intersection(const Ray3fP &ray,
                               Ellipsoid ellipsoid,
                               dr::mask_t<FloatP> active) const {
        using Value = dr::float32_array_t<FloatP>;
        using Value3 = Vector<Value, 3>;

        auto rot = dr::quat_to_matrix<dr::Matrix<Value, 3>>(ellipsoid.quat);

        // Transform space such that the ellipsoid is now a unit sphere centered
        // at the origin
        Value3 l = transpose(rot) * (ray.o - ellipsoid.center);
        Value3 d = transpose(rot) * ray.d;

        Value3 scale_rcp = dr::rcp(ellipsoid.scale);
        l *= scale_rcp;
        d *= scale_rcp;

        // Shift the ray origin to the point closest to the center (see sphere.cpp)
        Value A = dr::squared_norm(d),
              t_offset = -dr::dot(l, d) / A;
        Value3 o = dr::fmadd(d, t_offset, l);

        Value B = dr::scalar_t<Value>(2) * dr::dot(o, d),
              C = dr::squared_norm(o) - dr::scalar_t<Value>(1);

        auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

        // Undo the origin shift
        near_t += t_offset;

        // Only the nearest root counts: a ray that starts inside the
        // ellipsoid sees its backface, which is culled
        Value maxt = Value(ray.maxt);
        active &= dr::mask_t<FloatP>(solution_found && near_t >= Value(0) &&
                                     near_t <= maxt);
        FloatP t = FloatP(near_t);

        return { dr::select(active, t, dr::Infinity<FloatP>), active };
    }

    template <typename FloatP, typename Ray3fP>
    std::tuple<dr::mask_t<FloatP>, FloatP, Point<FloatP, 2>,
               dr::uint32_array_t<FloatP>, dr::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray,
                                   dr::uint32_array_t<FloatP> prim_index,
                                   dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);
        using Value = dr::float32_array_t<FloatP>;
        auto ellipsoid = m_ellipsoids.template get_ellipsoid<Value>(prim_index, active);
        ellipsoid.scale *= m_ellipsoids.template extents<Value>(prim_index, active);
        auto [t, valid] = ray_ellipsoid_intersection<FloatP, Ray3fP>(ray, ellipsoid, active);
        return { valid, t, dr::zeros<Point<FloatP, 2>>(), ((uint32_t) -1), prim_index };
    }

    template <typename FloatP, typename Ray3fP>
    dr::mask_t<FloatP> ray_test_impl(const Ray3fP &ray,
                                     dr::uint32_array_t<FloatP> prim_index,
                                     dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);
        return std::get<0>(ray_intersect_preliminary_impl<FloatP>(ray, prim_index, active));
    }

    MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape);
        // If necessary, temporally suspend gradient tracking for all shape
        // parameters to construct a surface interaction completely detach from
        // the shape.
        dr::suspend_grad<Float> scope(detach_shape, m_ellipsoids.data());
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

        auto ellipsoid = m_ellipsoids.template get_ellipsoid<Float>(pi.prim_index, active);
        ellipsoid.scale *= m_ellipsoids.template extents<Float>(pi.prim_index, active);
        auto rot = dr::quat_to_matrix<Matrix3f>(ellipsoid.quat);

        // The local coordinates are static as the ellipsoid moves
        si.t = pi.t;
        si.p = dr::detach(ray(pi.t));
        Point3f local_d =
            dr::detach(dr::transpose(rot) * (si.p - ellipsoid.center));

        si.n = dr::detach(
            dr::normalize(rot * (local_d / dr::square(ellipsoid.scale))));
        si.p_err = ray_error(ray, pi.t, si.n);

        Point3f p_att = ellipsoid.center +
                        rot * (ellipsoid.scale *
                               (local_d / dr::detach(ellipsoid.scale)));

        si.attach_motion(ray, p_att, ray_flags);

        Point3f local = dr::transpose(rot) * (si.p - ellipsoid.center);
        si.sh_frame.n = dr::normalize(rot * (local / dr::square(ellipsoid.scale)));

        si.n = si.sh_frame.n;

        si.prim_index = pi.prim_index;
        si.shape      = this;
        si.instance_index = 0;

        return si;
    }

    void describe(ShapeIR &g) const override {
        Base::describe(g);
        g.aabb_buffer = m_device_bboxes;
    }

    // =============================================================

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Ellipsoids[" << std::endl
            << "  bbox = " << string::indent(m_bbox) << "," << std::endl
            << "  ellipsoid_count = " << primitive_count() << "," << std::endl
            << "  " << string::indent(get_children_string()) << std::endl;

        if (m_ellipsoids.attributes().size() > 0) {
                oss << "  Ellipsoid attributes = {";
            for (auto& [name, attr]: m_ellipsoids.attributes())
                oss << " " << name << "[" << attr << "],";
            oss << "  }," << std::endl;
        }

        oss << "]";
        return oss.str();
    }

    void traverse_cb(void *payload, const dr::TraverseVisitor &cb) override {
        if (cb.role != dr::TraverseRole::Freeze)
            return;

        Base::traverse_cb(payload, cb);
        dr::traverse_fn(m_ellipsoids.data(), payload, cb, "m_ellipsoids");
        dr::traverse_fn(m_ellipsoids.extents_data(), payload, cb, "m_ellipsoids_extents");
        auto &attr_map = m_ellipsoids.attributes();
        for (auto it = attr_map.begin(); it != attr_map.end(); ++it) {
            dr::traverse_fn(it.value(), payload, cb, "m_ellipsoids_attributes");
        }
    }

    MI_DECLARE_CLASS(Ellipsoids)

private:

    /// Helper routine to recompute the bounding boxes of all ellipsoids.
    void recompute_bbox() {
        Timer timer;
        size_t ellipsoid_count = primitive_count();

        Log(Debug, "Recomputing bounding boxes for \"%s\" ellipsoids", ellipsoid_count);

        if constexpr (dr::is_jit_v<Float>) {
            UInt32 idx = dr::arange<UInt32>(m_ellipsoids.count());
            auto ellipsoid = m_ellipsoids.template get_ellipsoid<Float>(idx);
            ellipsoid.scale *= m_ellipsoids.extents_data();
            auto rot = dr::quat_to_matrix<Matrix3f>(ellipsoid.quat);

            Vector3f delta = Vector3f(
                dr::norm(rot[0] * ellipsoid.scale),
                dr::norm(rot[1] * ellipsoid.scale),
                dr::norm(rot[2] * ellipsoid.scale)
            );

            BoundingBox3f bbox(ellipsoid.center - delta, ellipsoid.center + delta);

            uint32_t size = (uint32_t) sizeof(BoundingBoxType) / sizeof(ScalarFloat);
            uint32_t stride = (uint32_t) offsetof(BoundingBoxType, max) / sizeof(ScalarFloat);

            Float data = dr::empty<Float>(ellipsoid_count * size);
            for (int i = 0; i < 3; i++) {
                dr::scatter(data, bbox.min[i], idx * size + i, true, ReduceMode::NoConflicts);
                dr::scatter(data, bbox.max[i], idx * size + i + stride, true, ReduceMode::NoConflicts);
            }

            if constexpr (IsGPU) {
                constexpr JitBackend backend = dr::backend_v<Float>;
                jit_free(m_device_bboxes);
                m_device_bboxes = jit_malloc(backend, sizeof(BoundingBoxType) * ellipsoid_count);
                jit_memcpy(backend, m_device_bboxes, data.data(),
                           sizeof(BoundingBoxType) * ellipsoid_count);
            }

            if constexpr (dr::is_llvm_v<Float>) {
                jit_free(m_host_bboxes);
                m_host_bboxes = jit_malloc(JitBackend::None, sizeof(BoundingBoxType) * ellipsoid_count);
                jit_memcpy(JitBackend::LLVM, m_host_bboxes, data.data(),
                           sizeof(BoundingBoxType) * ellipsoid_count);
            }

            m_bbox = ScalarBoundingBox3f(
                ScalarVector3f(
                    dr::slice<ScalarFloat>(dr::min(bbox.min[0])),
                    dr::slice<ScalarFloat>(dr::min(bbox.min[1])),
                    dr::slice<ScalarFloat>(dr::min(bbox.min[2]))
                ),
                ScalarVector3f(
                    dr::slice<ScalarFloat>(dr::max(bbox.max[0])),
                    dr::slice<ScalarFloat>(dr::max(bbox.max[1])),
                    dr::slice<ScalarFloat>(dr::max(bbox.max[2]))
                )
            );
        } else {
            const float *ptr = m_ellipsoids.data().data();
            const float *ptr_extents = m_ellipsoids.extents_data().data();

            BoundingBoxType *host_bboxes = (BoundingBoxType *) jit_malloc(
                JitBackend::None, sizeof(BoundingBoxType) * ellipsoid_count);

            m_bbox.reset();

            for (size_t i = 0; i < ellipsoid_count; ++i) {
                size_t idx = i * EllipsoidStructSize;
                ScalarPoint3f center(ptr[idx + 0], ptr[idx + 1], ptr[idx + 2]);
                ScalarVector3f scale(ptr[idx + 3], ptr[idx + 4], ptr[idx + 5]);
                ScalarQuaternion4f quat(ptr[idx + 6], ptr[idx + 7], ptr[idx + 8], ptr[idx + 9]);
                scale *= ptr_extents[i];
                auto rot = dr::quat_to_matrix<ScalarMatrix3f>(quat);

                ScalarVector3f delta = ScalarVector3f(
                    dr::norm(rot[0] * scale),
                    dr::norm(rot[1] * scale),
                    dr::norm(rot[2] * scale)
                );
                auto prim_bbox = ScalarBoundingBox3f(center - delta, center + delta);

                // Append the ellipsoid bounding box to the list
                host_bboxes[i] = BoundingBoxType(prim_bbox);

                // Expand the shape's bounding box
                m_bbox.expand(prim_bbox);
            }

            jit_free(m_host_bboxes);
            m_host_bboxes = host_bboxes;
        }

        Log(Debug, "Finished recomputing bounding boxes in %s", util::time_string((float) timer.value()));
    }

private:
    /// Object holding the ellipsoids data and attributes
    EllipsoidsData<Float, Spectrum> m_ellipsoids;

    /// The bounding box of the overall shape
    ScalarBoundingBox3f m_bbox;

    /// Per-ellipsoid boxes on the host (CPU backends) or the device (GPU
    /// backends, see describe())
    void *m_host_bboxes   = nullptr;
    void *m_device_bboxes = nullptr;
};

MI_EXPORT_PLUGIN(Ellipsoids)
NAMESPACE_END(mitsuba)
