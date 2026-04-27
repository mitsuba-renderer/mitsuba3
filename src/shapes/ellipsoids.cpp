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

#include "ellipsoids.h"

#if defined(MI_ENABLE_CUDA)
    #include "optix/ellipsoids.cuh"
#endif

#if defined(MI_ENABLE_METAL)
    #include <mitsuba/render/metal/shapes.h>
#endif

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
        :name: sphere

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
    #if defined(MI_ENABLE_CUDA)
        using BoundingBoxType =
            typename std::conditional<dr::is_cuda_v<Float>,
                                      optix::BoundingBox3f,
                                      ScalarBoundingBox3f>::type;
    #else
            using BoundingBoxType = ScalarBoundingBox3f;
    #endif

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
        if constexpr (dr::is_cuda_v<Float>)
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

    ScalarBoundingBox3f bbox(ScalarIndex index) const override {
        if constexpr (dr::is_cuda_v<Float>)
            Throw("bbox(ScalarIndex) is not available in CUDA mode!");
        Assert(index <= primitive_count());
        auto bbox = ((BoundingBoxType*) m_host_bboxes)[index];

        return ScalarBoundingBox3f(
            ScalarPoint3f(bbox.min[0], bbox.min[1], bbox.min[2]),
            ScalarPoint3f(bbox.max[0], bbox.max[1], bbox.max[2])
        );
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Sampling routines (not implemented!)
    // =============================================================

    PositionSample3f sample_position(Float, const Point2f &, Mask) const override { return dr::zeros<PositionSample3f>(); }

    Float pdf_position(const PositionSample3f &, Mask) const override { return 0; }

    DirectionSample3f sample_direction(const Interaction3f &, const Point2f &, Mask) const override { return dr::zeros<DirectionSample3f>(); }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &, Mask) const override { return 0; }

    SurfaceInteraction3f eval_parameterization(const Point2f &, uint32_t, Mask) const override { return dr::zeros<SurfaceInteraction3f>(); }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Attribute routines
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

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    template <typename FloatP, typename Ray3fP, typename Ellipsoid>
    std::pair<FloatP, dr::mask_t<FloatP>>
    ray_ellipsoid_intersection(const Ray3fP &ray,
                               Ellipsoid ellipsoid,
                               dr::mask_t<FloatP> active) const {
        using Value = dr::float32_array_t<FloatP>;
        using Value3 = Vector<Value, 3>;

        auto rot = dr::quat_to_matrix<dr::Matrix<Value, 3>>(ellipsoid.quat);

        Value maxt = Value(ray.maxt);

        // Transform space such that the ellipsoid is now a unit sphere centered
        // at the origin
        Value3 o = transpose(rot) * (ray.o - ellipsoid.center);
        Value3 d = transpose(rot) * ray.d;

        Value3 scale_rcp = dr::rcp(ellipsoid.scale);
        o *= scale_rcp;
        d *= scale_rcp;

        Ray3fP ray_relative(o, d);

        // We define a plane which is perpendicular to the ray direction and
        // contains the ellipsoid center and intersect it. We then solve the
        // ray-sphere intersection as if the ray origin was this new
        // intersection point. This additional step makes the whole intersection
        // routine numerically more robust.

        Value plane_t = dot(-o, d) / norm(d);
        Value3 plane_p = ray_relative(FloatP(plane_t));

        Value A = dr::squared_norm(d);
        Value B = dr::scalar_t<Value>(2.0) * dr::dot(plane_p, d);
        Value C = dr::squared_norm(plane_p) - Value(1.0);
        auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

        // Adjust distances for plane intersection
        near_t += plane_t;
        far_t += plane_t;

        // Ellipsoid doesn't intersect with the segment on the ray
        dr::mask_t<FloatP> out_bounds = !(near_t <= maxt && far_t >= Value(0.0)); // NaN-aware conditionals

        // Ellipsoid fully contains the segment of the ray
        dr::mask_t<FloatP> in_bounds = near_t < Value(0.0) && far_t > maxt;

        // Ellipsoid is backfacing
        dr::mask_t<FloatP> backfacing = (near_t < Value(0.0));

        active &= solution_found && !out_bounds && !in_bounds && !backfacing;

        FloatP t = dr::select(near_t < Value(0.0), FloatP(far_t), FloatP(near_t));
        t =  dr::select(active, t, dr::Infinity<FloatP>);

        return { t, active };
    }

    template <typename FloatP, typename Ray3fP>
    std::tuple<FloatP, Point<FloatP, 2>, dr::uint32_array_t<FloatP>,
               dr::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray,
                                   ScalarIndex prim_index,
                                   dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);
        using Value = dr::float32_array_t<FloatP>;
        using ScalarValue = dr::scalar_t<Value>;
        auto ellipsoid = m_ellipsoids.template get_ellipsoid<ScalarValue>(prim_index, active);
        ellipsoid.scale *= m_ellipsoids.template extents<ScalarValue>(prim_index);
        auto [t, valid] = ray_ellipsoid_intersection<FloatP, Ray3fP>(ray, ellipsoid, active);
        return { t, dr::zeros<Point<FloatP, 2>>(), ((uint32_t) -1), prim_index };
    }

    template <typename FloatP, typename Ray3fP>
    dr::mask_t<FloatP> ray_test_impl(const Ray3fP &ray,
                                     ScalarIndex prim_index,
                                     dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);
        using Value = dr::float32_array_t<FloatP>;
        using ScalarValue = dr::scalar_t<Value>;
        auto ellipsoid = m_ellipsoids.template get_ellipsoid<ScalarValue>(prim_index, active);
        ellipsoid.scale *= m_ellipsoids.template extents<ScalarValue>(prim_index);
        auto [t, valid] = ray_ellipsoid_intersection<FloatP>(ray, ellipsoid, active);
        return valid;
    }

    MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     uint32_t recursion_depth,
                                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        // Early exit when tracing isn't necessary
        if (!m_is_instance && recursion_depth > 0)
            return dr::zeros<SurfaceInteraction3f>();

        bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape);
        /* If necessary, temporally suspend gradient tracking for all shape
           parameters to construct a surface interaction completely detach from
           the shape. */
        dr::suspend_grad<Float> scope(detach_shape, m_ellipsoids.data());
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

        auto ellipsoid = m_ellipsoids.template get_ellipsoid<Float>(pi.prim_index, active);
        ellipsoid.scale *= m_ellipsoids.template extents<Float>(pi.prim_index, active);
        auto rot = dr::quat_to_matrix<Matrix3f>(ellipsoid.quat);

        si.t = dr::select(active, pi.t, dr::Infinity<Float>);
        si.p = ray(pi.t);

        Point3f local = dr::transpose(rot) * (si.p - ellipsoid.center);
        si.sh_frame.n = dr::normalize(rot * (local / dr::square(ellipsoid.scale)));

        si.n = si.sh_frame.n;
        si.uv    = Point2f(0.f, 0.f);
        si.dp_du = Vector3f(0.f);
        si.dp_dv = Vector3f(0.f);
        si.dn_du = si.dn_dv = dr::zeros<Vector3f>();

        si.prim_index = pi.prim_index;
        si.shape      = this;
        si.instance   = nullptr;

        return si;
    }

#if defined(MI_ENABLE_CUDA)
    using Base::m_optix_data_ptr;

    void optix_prepare_geometry() override {
        if constexpr (dr::is_cuda_v<Float>) {
            if (!m_optix_data_ptr)
                m_optix_data_ptr =
                    jit_malloc(AllocType::Device, sizeof(OptixEllipsoidsData));

            OptixEllipsoidsData data = {
                m_bbox,
                m_ellipsoids.extents_data().data(),
                m_ellipsoids.data().data()
            };
            jit_memcpy(JitBackend::CUDA, m_optix_data_ptr, &data, sizeof(OptixEllipsoidsData));
        }
    }

    void optix_build_input(OptixBuildInput &build_input) const override {
        build_input.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
        build_input.customPrimitiveArray.aabbBuffers   = (CUdeviceptr*) &m_device_bboxes;
        build_input.customPrimitiveArray.numPrimitives = primitive_count();
        build_input.customPrimitiveArray.strideInBytes = 6 * sizeof(float);
        build_input.customPrimitiveArray.flags         = optix_geometry_flags;
        build_input.customPrimitiveArray.primitiveIndexOffset = 0;
        build_input.customPrimitiveArray.numSbtRecords = 1;
    }

    static constexpr uint32_t optix_geometry_flags[1] = { OPTIX_GEOMETRY_FLAG_NONE };
#endif

#if defined(MI_ENABLE_METAL)
    // Multi-primitive shape: one AABB + one EllipsoidData per ellipsoid.
    size_t metal_aabb_count() const override {
        return (size_t) m_ellipsoids.count();
    }

    size_t metal_primitive_data_size() const override {
        return sizeof(metal_shape::EllipsoidData);
    }

    uint32_t metal_intersection_function_index() const override {
        return metal_shape::INTERSECTION_FN_ELLIPSOIDS;
    }

    void metal_fill_aabb_data(void *out) const override {
        if constexpr (dr::is_metal_v<Float>) {
            // Copy the ellipsoid arrays into a host-side temporary so the
            // host CPU can read them. Metal device pointers are not always
            // host-readable.
            size_t n = (size_t) m_ellipsoids.count();
            std::vector<float> data_h(n * 10), ext_h(n);

            dr::eval(m_ellipsoids.data(), m_ellipsoids.extents_data());
            dr::sync_thread();

            jit_memcpy(JitBackend::Metal, data_h.data(),
                       m_ellipsoids.data().data(), n * 10 * sizeof(float));
            jit_memcpy(JitBackend::Metal, ext_h.data(),
                       m_ellipsoids.extents_data().data(), n * sizeof(float));

            float *dst = (float *) out;
            for (size_t i = 0; i < n; ++i)
                metal_compute_ellipsoid_aabb(data_h.data() + i * 10, ext_h[i],
                                              dst + i * 6);
        } else {
            (void) out;
        }
    }

    void metal_fill_primitive_data(void *out) const override {
        if constexpr (dr::is_metal_v<Float>) {
            size_t n = (size_t) m_ellipsoids.count();
            std::vector<float> data_h(n * 10), ext_h(n);

            dr::eval(m_ellipsoids.data(), m_ellipsoids.extents_data());
            dr::sync_thread();

            jit_memcpy(JitBackend::Metal, data_h.data(),
                       m_ellipsoids.data().data(), n * 10 * sizeof(float));
            jit_memcpy(JitBackend::Metal, ext_h.data(),
                       m_ellipsoids.extents_data().data(), n * sizeof(float));

            metal_shape::EllipsoidData *dst =
                (metal_shape::EllipsoidData *) out;

            for (size_t i = 0; i < n; ++i)
                metal_compute_ellipsoid_data(data_h.data() + i * 10, ext_h[i],
                                              dst[i]);
        } else {
            (void) out;
        }
    }

private:
    // Build the per-ellipsoid AABB. Mirrors recompute_bbox()'s formula:
    // delta = (||R^T_row[0]·diag(s)||, ||R^T_row[1]·diag(s)||,
    //          ||R^T_row[2]·diag(s)||) but computed per-ellipsoid on host.
    static void metal_compute_ellipsoid_aabb(const float *e10, float ext,
                                             float *out_bbox) {
        float cx = e10[0], cy = e10[1], cz = e10[2];
        float sx = e10[3] * ext, sy = e10[4] * ext, sz = e10[5] * ext;
        float qx = e10[6], qy = e10[7], qz = e10[8], qw = e10[9];

        float xx = qx*qx, yy = qy*qy, zz = qz*qz;
        float xy = qx*qy, xz = qx*qz, yz = qy*qz;
        float xw = qx*qw, yw = qy*qw, zw = qz*qw;
        float R[3][3];
        R[0][0] = 1 - 2*(yy + zz); R[0][1] = 2*(xy - zw);     R[0][2] = 2*(xz + yw);
        R[1][0] = 2*(xy + zw);     R[1][1] = 1 - 2*(xx + zz); R[1][2] = 2*(yz - xw);
        R[2][0] = 2*(xz - yw);     R[2][1] = 2*(yz + xw);     R[2][2] = 1 - 2*(xx + yy);

        float dx = std::sqrt(R[0][0]*R[0][0]*sx*sx + R[0][1]*R[0][1]*sy*sy + R[0][2]*R[0][2]*sz*sz);
        float dy = std::sqrt(R[1][0]*R[1][0]*sx*sx + R[1][1]*R[1][1]*sy*sy + R[1][2]*R[1][2]*sz*sz);
        float dz = std::sqrt(R[2][0]*R[2][0]*sx*sx + R[2][1]*R[2][1]*sy*sy + R[2][2]*R[2][2]*sz*sz);

        out_bbox[0] = cx - dx; out_bbox[1] = cy - dy; out_bbox[2] = cz - dz;
        out_bbox[3] = cx + dx; out_bbox[4] = cy + dy; out_bbox[5] = cz + dz;
    }

    // Build the per-ellipsoid `to_object` 4x4 (row-major) that maps
    // world-space points into the unit-sphere frame:
    //   to_object = diag(1/scale_eff) * R^T * translate(-center)
    // where R is the object-to-world rotation built from the quaternion.
    static void metal_compute_ellipsoid_data(const float *e10, float ext,
                                             metal_shape::EllipsoidData &dst) {
        float cx = e10[0], cy = e10[1], cz = e10[2];
        float sx = e10[3] * ext, sy = e10[4] * ext, sz = e10[5] * ext;
        float qx = e10[6], qy = e10[7], qz = e10[8], qw = e10[9];

        float xx = qx*qx, yy = qy*qy, zz = qz*qz;
        float xy = qx*qy, xz = qx*qz, yz = qy*qz;
        float xw = qx*qw, yw = qy*qw, zw = qz*qw;
        float R[3][3];
        R[0][0] = 1 - 2*(yy + zz); R[0][1] = 2*(xy - zw);     R[0][2] = 2*(xz + yw);
        R[1][0] = 2*(xy + zw);     R[1][1] = 1 - 2*(xx + zz); R[1][2] = 2*(yz - xw);
        R[2][0] = 2*(xz - yw);     R[2][1] = 2*(yz + xw);     R[2][2] = 1 - 2*(xx + yy);

        // Fill bbox (mirrors metal_compute_ellipsoid_aabb).
        float dx = std::sqrt(R[0][0]*R[0][0]*sx*sx + R[0][1]*R[0][1]*sy*sy + R[0][2]*R[0][2]*sz*sz);
        float dy = std::sqrt(R[1][0]*R[1][0]*sx*sx + R[1][1]*R[1][1]*sy*sy + R[1][2]*R[1][2]*sz*sz);
        float dz = std::sqrt(R[2][0]*R[2][0]*sx*sx + R[2][1]*R[2][1]*sy*sy + R[2][2]*R[2][2]*sz*sz);
        dst.bbox.min[0] = cx - dx; dst.bbox.min[1] = cy - dy; dst.bbox.min[2] = cz - dz;
        dst.bbox.max[0] = cx + dx; dst.bbox.max[1] = cy + dy; dst.bbox.max[2] = cz + dz;

        float inv_s[3] = { 1.0f / sx, 1.0f / sy, 1.0f / sz };
        float center[3] = { cx, cy, cz };

        // to_object[row][col] (row-major). The 3x3 linear part is
        // R^T scaled by inv_s on the rows; the translation is
        //   - inv_s[row] * (R^T * center)[row].
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col)
                dst.to_object[row * 4 + col] = R[col][row] * inv_s[row];
            float t = R[0][row] * center[0] +
                      R[1][row] * center[1] +
                      R[2][row] * center[2];
            dst.to_object[row * 4 + 3] = -inv_s[row] * t;
        }
        // Last row: (0, 0, 0, 1) — apply_affine_point ignores it but keep
        // the layout sane.
        dst.to_object[12] = 0.f;
        dst.to_object[13] = 0.f;
        dst.to_object[14] = 0.f;
        dst.to_object[15] = 1.f;
    }

public:
#endif

    //! @}
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

    void traverse_1_cb_ro(void *payload, dr::detail::traverse_callback_ro fn) const override {
        // Only traverse the scene for frozen functions, since accidentally
        // traversing the scene in loops or vcalls can cause errors with variable
        // size mismatches, and backpropagation of gradients.
        if (!jit_flag(JitFlag::EnableObjectTraversal))
            return;

        Object::traverse_1_cb_ro(payload, fn);
        dr::traverse_1(this->traverse_1_cb_fields_(), [payload, fn](auto &x) {
            dr::traverse_1_fn_ro(x, payload, fn);
        });

        dr::traverse_1_fn_ro(m_ellipsoids.data(), payload, fn);
        dr::traverse_1_fn_ro(m_ellipsoids.extents_data(), payload, fn);
        auto &attr_map = m_ellipsoids.attributes();
        for (auto it = attr_map.begin(); it != attr_map.end(); ++it) {
            dr::traverse_1_fn_ro(it.value(), payload, fn);
        }
    }

    void traverse_1_cb_rw(void *payload, dr::detail::traverse_callback_rw fn) override {
        // Only traverse the scene for frozen functions, since accidentally
        // traversing the scene in loops or vcalls can cause errors with variable
        // size mismatches, and backpropagation of gradients.
        if (!jit_flag(JitFlag::EnableObjectTraversal))
            return;

        Object::traverse_1_cb_rw(payload, fn);
        dr::traverse_1(this->traverse_1_cb_fields_(), [payload, fn](auto &x) {
            dr::traverse_1_fn_rw(x, payload, fn);
        });

        dr::traverse_1_fn_rw(m_ellipsoids.data(), payload, fn);
        dr::traverse_1_fn_rw(m_ellipsoids.extents_data(), payload, fn);
        auto &attr_map = m_ellipsoids.attributes();
        for (auto it = attr_map.begin(); it != attr_map.end(); ++it) {
            dr::traverse_1_fn_rw(it.value(), payload, fn);
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

            if constexpr (dr::is_cuda_v<Float>) {
                jit_free(m_device_bboxes);
                m_device_bboxes = jit_malloc(AllocType::Device, sizeof(BoundingBoxType) * ellipsoid_count);
                jit_memcpy(JitBackend::CUDA, m_device_bboxes, data.data(),
                           sizeof(BoundingBoxType) * ellipsoid_count);
            }

            if constexpr (dr::is_llvm_v<Float>) {
                jit_free(m_host_bboxes);
                m_host_bboxes = jit_malloc(AllocType::Host, sizeof(BoundingBoxType) * ellipsoid_count);
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
                AllocType::Host, sizeof(BoundingBoxType) * ellipsoid_count);

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

    /// The pointer to the bounding box data above (used in Embree and OptiX)
    void *m_host_bboxes   = nullptr;
    void *m_device_bboxes = nullptr;
};

MI_EXPORT_PLUGIN(Ellipsoids)
NAMESPACE_END(mitsuba)
