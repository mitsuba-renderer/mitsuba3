#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/scene_ir.h>
#include <drjit/while_loop.h>
#include <mitsuba/render/volumegrid.h>

#include <drjit/tensor.h>

#if defined(MI_ENABLE_EMBREE)
#  include <embree3/rtcore.h>
#endif

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-sdfgrid:

SDF Grid (:monosp:`sdfgrid`)
-------------------------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the SDF grid data to be loaded. The expected file format
     aligns with a single-channel :ref:`grid-based volume data source <volume-gridvolume>`.
     If no filename is provided, the shape is initialised as an empty 2x2x2 grid.

 * - grid
   - |tensor|
   - Tensor array containing the grid data in single or half precision. This
     parameter can only be specified when building this plugin at runtime from
     Python or C++ and cannot be specified in the XML scene description.
   - |exposed|, |differentiable|, |discontinuous|

 * - normals
   - |string|
   - Specifies the method for computing shading normals. The options are
     :monosp:`analytic` or :monosp:`smooth`. (Default: :monosp:`smooth`)

 * - to_world
   - |transform|
   - Specifies a linear object-to-world transformation. (Default: none (i.e. object space = world space))
   - |exposed|, |differentiable|, |discontinuous|

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/shape_sdfgrid.jpg
   :caption: SDF grid with ``smooth`` shading normals
.. subfigure:: ../../resources/data/docs/images/render/shape_sdfgrid_analytic.jpg
   :caption: SDF grid with :code:`analytic` shading normals
.. subfigend::
   :label: fig-sdfgrid

This shape plugin describes a signed distance function (SDF) grid shape
primitive --- that is, an SDF sampled onto a three-dimensional grid.
The grid object-space is mapped over the range :math:`[0,1]^3`.

A smooth method for computing normals :cite:`Hansson-Soderlund2022SDF` is
selected as the default approach to ensure continuity across grid cells.

.. warning::
    Compared with the other available shape plugins, the SDF grid has a few
    important limitations. Namely:

    - It does not emit UV coordinates for texturing.
    - It cannot be used as an area emitter.

.. note::
    When differentiating this shape, it does not leverage the work presented in
    :cite:`Vicini2022sdf`. However, a Mitsuba 3-based implementation of that
    technique is available on `its project's page
    <https://github.com/rgl-epfl/differentiable-sdf-rendering>`_.

.. tabs::
    .. code-tab:: xml
        :name: sdfgrid

        <shape type="sdfgrid">
            <string name="filename" value="data.sdf"/>
            <bsdf type="diffuse"/>
        </shape>

    .. code-tab:: python

        'type': 'sdfgrid',
        'filename': 'data.sdf'
        'bsdf': {
            'type': 'diffuse'
        }
 */

template <typename Float, typename Spectrum>
class SDFGrid final : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_to_world, m_is_instance, m_shape_type,
                   initialize, mark_dirty, get_children_string,
                   parameters_grad_enabled)
    MI_IMPORT_TYPES()

    /// Termination threshold of the numerical root finder, in ray parameter units
    static constexpr float NumSolveEpsilon = 1e-5f;

    // Grid samples are stored in single or half precision
    using InputFloat     = dr::replace_scalar_t<Float, float>;
    using InputPoint3f   = Point<InputFloat, 3>;
    using InputTensorXf  = dr::Tensor<DynamicBuffer<InputFloat>>;
    using InputBoundingBox3f = BoundingBox<InputPoint3f>;
    using InputScalarBoundingBox3f = BoundingBox<Point<float, 3>>;

    using typename Base::ScalarIndex;
    using typename Base::ScalarSize;

    SDFGrid(const Properties &props) : Base(props) {
#if !defined(MI_ENABLE_EMBREE)
        if constexpr (!dr::is_jit_v<Float>)
            Throw("In scalar variants, the SDF grid is only available with "
                  "Embree!");
#endif

        std::string_view normals_mode_str = props.get<std::string_view>("normals", "smooth");
        if (normals_mode_str == "analytic")
            m_normal_method = Analytic;
        else if (normals_mode_str == "smooth")
            m_normal_method = Smooth;
        else
            Throw("Invalid normals mode \"%s\", must be one of: \"analytic\", "
                  "or \"smooth\"!",
                  normals_mode_str);

        auto check_shape = [](const auto &tensor) {
            if (tensor.ndim() != 4)
                Throw("SDF grid tensor has dimension %lu, expected 4",
                      tensor.ndim());
            if (tensor.shape(3) != 1)
                Throw("SDF grid shape at index 3 is %lu, expected 1",
                      tensor.shape(3));
        };

        if (props.has_property("filename")) {
            FileResolver *fs   = file_resolver();
            fs::path file_path = fs->resolve(props.get<std::string_view>("filename"));
            if (!fs::exists(file_path))
                Log(Error, "\"%s\": file does not exist!", file_path);
            VolumeGrid<float, Color<float, 3>> vol_grid(file_path);
            ScalarVector3i res = vol_grid.size();
            if (vol_grid.channel_count() != 1)
                Throw("SDF grid data source \"%s\" has %lu channels, expected 1.",
                      file_path, vol_grid.channel_count());

            m_grid = InputTensorXf(vol_grid.data(), { (size_t) res.z(),
                                                     (size_t) res.y(),
                                                     (size_t) res.x(), 1 });
        } else if (props.has_property("grid")) {
            const Any &any = props.get<Any>("grid");
            if (const TensorXf16 *tensor = any_cast<TensorXf16>(&any)) {
                check_shape(*tensor);
                m_grid_half = *tensor;
                m_half = true;
            } else if (const TensorXf *tensor = any_cast<TensorXf>(&any)) {
                check_shape(*tensor);
                m_grid = InputTensorXf(*tensor);
            } else {
                Throw("The \"grid\" parameter must be a single or half "
                      "precision tensor!");
            }
        } else {
            Throw("The SDF values must be specified with either the "
                  "\"filename\" or \"grid\" parameter!");
        }

        m_shape_type = ShapeType::SDFGrid;

        update();
        initialize();
    }

    ~SDFGrid() {
        if constexpr (!dr::is_jit_v<Float>) {
            jit_free(m_bboxes_ptr);
            jit_free(m_voxel_indices_ptr);
        }
    }

    void update() {
        auto [S, Q, T] =
            dr::transform_decompose(m_to_world.scalar().matrix, 25);
        if (dr::abs(Q[0]) > 1e-6f || dr::abs(Q[1]) > 1e-6f ||
            dr::abs(Q[2]) > 1e-6f || dr::abs(Q[3] - 1) > 1e-6f)
            Log(Warn, "'to_world' transform shouldn't perform any rotations, "
                      "use instancing (`shapegroup` and `instance` plugins) "
                      "instead!");


        // The tensor is packed [Z, Y, X, C]
        const auto &shape = m_half ? m_grid_half.shape() : m_grid.shape();
        m_res = ScalarVector3u((uint32_t) shape[2], (uint32_t) shape[1],
                               (uint32_t) shape[0]);

        if constexpr (!dr::is_jit_v<Float>){
            jit_free(m_bboxes_ptr);
            jit_free(m_voxel_indices_ptr);
        }
        std::tie(m_bboxes_ptr,
                 m_voxel_indices_ptr,
                 m_filled_voxel_count) = build_bboxes();
        if (m_filled_voxel_count == 0)
            Throw("SDFGrid should at least have one non-empty voxel!");

        mark_dirty();
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("to_world", m_to_world,              ParamFlags::NonDifferentiable);
        if (m_half)
            cb->put("grid", m_grid_half, ParamFlags::NonDifferentiable);
        else
            cb->put("grid", m_grid,      ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "to_world") ||
            string::contains(keys, "grid")) {
            // LLVM/Embree reads these parameters from host memory; wait for
            // in-flight kernels before overwriting (GPU uploads are queue-ordered).
            if constexpr (dr::is_llvm_v<Float>)
                dr::sync_thread();

            m_to_world = m_to_world.value().update();

            update();
        }

        Base::parameters_changed(keys);
    }

    ScalarSize primitive_count() const override { return m_filled_voxel_count; }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox;
        ScalarAffineTransform4f to_world = m_to_world.scalar();

        bbox.expand(to_world * ScalarPoint3f(0.f, 0.f, 0.f));
        bbox.expand(to_world * ScalarPoint3f(1.f, 0.f, 0.f));
        bbox.expand(to_world * ScalarPoint3f(0.f, 1.f, 0.f));
        bbox.expand(to_world * ScalarPoint3f(1.f, 1.f, 0.f));
        bbox.expand(to_world * ScalarPoint3f(0.f, 0.f, 1.f));
        bbox.expand(to_world * ScalarPoint3f(1.f, 0.f, 1.f));
        bbox.expand(to_world * ScalarPoint3f(0.f, 1.f, 1.f));
        bbox.expand(to_world * ScalarPoint3f(1.f, 1.f, 1.f));

        return bbox;
    }

    ScalarBoundingBox3f bbox(ScalarIndex prim_index) const override {
        if constexpr (dr::is_cuda_v<Float> || dr::is_metal_v<Float>)
            NotImplementedError("bbox(ScalarIndex prim_index)");

        return reinterpret_cast<InputScalarBoundingBox3f*>(m_bboxes_ptr)[prim_index];
    }

    Float surface_area() const override {
        return 0;
    }

    // =============================================================
    // Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);
        (void) time;
        (void) sample;
        PositionSample3f ps = dr::zeros<PositionSample3f>();
        return ps;
    }

    Float pdf_position(const PositionSample3f & /*ps*/,
                       Mask active) const override {
        MI_MASK_ARGUMENT(active);
        return 0;
    }

    SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                               uint32_t ray_flags,
                                               Mask active) const override {
        MI_MASK_ARGUMENT(active);
        (void) uv;
        (void) ray_flags;
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        return si;
    }

    // =============================================================

    // =============================================================
    // Ray tracing routines
    // =============================================================

    template <typename FloatP, typename Ray3fP>
    std::tuple<dr::mask_t<FloatP>, FloatP, Point<FloatP, 2>,
               dr::uint32_array_t<FloatP>, dr::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray_,
                                   dr::uint32_array_t<FloatP> prim_index,
                                   dr::mask_t<FloatP> active) const {
        return ray_intersect_preliminary_common_impl<FloatP>(ray_, prim_index,
                                                            active);
    }

    template <typename FloatP, typename Ray3fP>
    dr::mask_t<FloatP> ray_test_impl(const Ray3fP &ray_,
                                     dr::uint32_array_t<FloatP> prim_index,
                                     dr::mask_t<FloatP> active) const {
        auto [hit, t, uv, shape_index, p] =
            ray_intersect_preliminary_common_impl<FloatP>(ray_, prim_index, active);
        return hit;
    }

    MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    SurfaceInteraction3f
    compute_surface_interaction(const Ray3f &ray,
                                const PreliminaryIntersection3f &pi,
                                uint32_t ray_flags,
                                Mask active) const override {
        MI_MASK_ARGUMENT(active);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

        bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape);

        AffineTransform4f to_world  = m_to_world.value();
        AffineTransform4f to_object = to_world.inverse();

        dr::suspend_grad<Float> scope(detach_shape, to_world, to_object,
                                      m_grid.array(), m_grid_half.array());

        // Sample index of the intersected voxel's lower corner. Using it
        // instead of reconstructing the voxel from the hit point avoids
        // selecting a neighbor when rounding places the point across a
        // grid plane.
        UInt32 grid_index;
        if constexpr (dr::is_jit_v<Float>)
            grid_index = dr::gather<UInt32>(m_jit_voxel_indices, pi.prim_index, active);
        else
            grid_index = m_voxel_indices_ptr[pi.prim_index];

        Point3f local_p = dr::detach(to_object * ray(pi.t));
        auto [sdf_value, sdf_grad] = sdf_eval(local_p, grid_index, active);
        Vector3f local_grad = dr::detach(sdf_grad);
        Normal3f local_n = dr::normalize(local_grad);

        // Detached normal for the error bound, written so that its primal
        // ops coincide with those of the attached normal computed below
        si.t = pi.t;
        si.p = dr::detach(to_world) * local_p;
        si.n = dr::normalize(dr::detach(to_world) * Normal3f(local_grad));

        // The hit point is not reprojected onto the level set. Its error
        // grows with the ray extent, and the root finder tolerance displaces
        // it further along the ray.
        si.p_err = to_world.position_error(local_p, si.n) +
                   ray_error(ray, pi.t, si.n) +
                   dr::detach(dr::abs_dot(si.n, ray.d)) * NumSolveEpsilon;

        Point3f p_att = si.p;
        if constexpr (dr::is_diff_v<Float>) {
            // Position at the detached parameterization. Only a motion of the
            // entire shape truly glues the interaction point to the surface:
            // for a single voxel the motion is ambiguous.
            Point3f local_motion =
                sdf_value * (-local_n) / dr::dot(local_n, local_grad);
            p_att = to_world * dr::replace_grad(local_p, local_motion);
        }

        si.attach_motion(ray, p_att, ray_flags);

        // Local hit point whose derivative follows the attached world
        // position. Its primal value is 'local_p', so the grid lookups of
        // the attached normal are shared with those of 'local_grad'.
        Point3f local_p_att = local_p;
        if constexpr (dr::is_diff_v<Float>)
            local_p_att = dr::replace_grad(local_p, to_object * si.p);

        si.n = dr::normalize(to_world * Normal3f(
            sdf_eval(local_p_att, grid_index, active).second));

        if (likely(has_flag(ray_flags, RayFlags::Shading))) {
            switch (m_normal_method) {
                case Analytic:
                    si.sh_frame.n = si.n;
                    break;
                case Smooth:
                    si.sh_frame.n = smooth(local_p_att, grid_index, active);
                    break;
                default:
                    Throw("Unknown normal computation.");
            }
        }

        si.prim_index = pi.prim_index;
        si.shape    = this;
        si.instance_index = 0;

        return si;
    }

    /**
     * Smooth shading normal following
     *
     * Herman Hansson-Söderlund, Alex Evans, and Tomas Akenine-Möller, Ray
     * Tracing of Signed Distance Function Grids, Journal of Computer
     * Graphics Techniques (JCGT), vol. 11, no. 3, 94-113, 2022
     */
    Normal3f smooth(const Point3f &p, const UInt32 &grid_index,
                    Mask active) const {
        using Mask3 = dr::mask_t<Vector3f>;

        int32_t nx = (int32_t) m_res.x(), ny = (int32_t) m_res.y(),
                nz = (int32_t) m_res.z();
        Vector3f resolution(nx - 1.f, ny - 1.f, nz - 1.f);
        Point3f scaled_p = p * resolution;

        // The normal blends the gradients of the eight voxels around the grid
        // vertex nearest to 'p'. 'v' is the lower corner of these voxels, and
        // 'uvw' is the position of 'p' relative to their centers.
        Point3i v = Point3i(dr::round(scaled_p)) - 1;
        Vector3f uvw = scaled_p - Vector3f(v) - .5f;

        // Each voxel's gradient consists of finite differences of the
        // interpolant across the voxel's extent along one axis, evaluated at
        // the position of 'p' along the other two. A difference thus only
        // depends on the voxel's index along its axis, and it is a bilinear
        // interpolant of grid samples within the intersected voxel ('cell'),
        // whose corner values are shared with the geometric normal.
        Point3i cell(voxel_coords(grid_index));
        Vector3f w = scaled_p - Vector3f(cell);

        Float c[8];
        gather_corners(grid_index, active, c);

        auto sample = [&](const Point3i &q) -> Float {
            Point3i qc = dr::minimum(dr::maximum(q, 0), Point3i(nx - 1, ny - 1, nz - 1));
            return gather_grid<Float>(UInt32((qc.z() * ny + qc.y()) * nx + qc.x()), active);
        };

        // Interpolant on the cell face with coordinate 'hi' along axis 'a',
        // at the position of 'p' along the other two axes
        auto cell_plane = [&](int a, int hi) -> Float {
            int b = (a + 1) % 3, c2 = (a + 2) % 3;
            Float f[4];
            for (int i = 0; i < 4; ++i)
                f[i] = c[(hi << a) | ((i & 1) << b) | ((i >> 1) << c2)];
            return bilerp(f, w[b], w[c2]);
        };

        // Interpolant on the plane with sample index 'q' along axis 'a'
        auto plane = [&](int a, const Int32 &q) -> Float {
            int b = (a + 1) % 3, c2 = (a + 2) % 3;
            Float f[4];
            for (int i = 0; i < 4; ++i) {
                Point3i idx;
                idx[a] = q;
                idx[b] = cell[b] + (i & 1);
                idx[c2] = cell[c2] + (i >> 1);
                f[i] = sample(idx);
            }
            return bilerp(f, w[b], w[c2]);
        };

        // Differences of the voxels with the lower ('d0') and upper ('d1')
        // index along each axis. One of them spans the cell, the other one
        // extends to the neighboring sample plane on the side of the other
        // voxel. That difference vanishes at the grid boundary, where the
        // sample index is clamped.
        Vector3f d0, d1;
        for (int a = 0; a < 3; ++a) {
            Mask upper = cell[a] != v[a];
            Float g0 = cell_plane(a, 0),
                  g1 = cell_plane(a, 1),
                  gq = plane(a, cell[a] + dr::select(upper, -1, 2)),
                  dc = g1 - g0,
                  dq = dr::select(upper, g0 - gq, gq - g1);
            d0[a] = dr::select(upper, dq, dc) * resolution[a];
            d1[a] = dr::select(upper, dc, dq) * resolution[a];
        }

        // Voxels outside of the grid do not contribute, and the blending
        // weight along an axis is set so that they receive weight zero
        Mask3 inside = v >= 0;
        uvw = dr::select(inside, uvw, 1.f);

        Vector3f n[8];
        for (int i = 0; i < 8; ++i) {
            Mask3 hi((i & 1) != 0, (i & 2) != 0, (i & 4) != 0);
            n[i] = dr::select(dr::all(hi || inside),
                              dr::normalize(dr::select(hi, d1, d0)),
                              Vector3f(0.f));
        }

        Vector3f result = dr::lerp(
            dr::lerp(dr::lerp(n[0], n[1], uvw.x()), dr::lerp(n[2], n[3], uvw.x()), uvw.y()),
            dr::lerp(dr::lerp(n[4], n[5], uvw.x()), dr::lerp(n[6], n[7], uvw.x()), uvw.y()),
            uvw.z());

        return dr::normalize(m_to_world.value() * Normal3f(result));
    }

    bool parameters_grad_enabled() const override {
        return dr::grad_enabled(m_to_world);
    }

    void describe(ShapeIR &g) const override {
        Base::describe(g);
        // The GPU backends build from the tightly packed device boxes
        if constexpr (dr::is_cuda_v<Float> || dr::is_metal_v<Float>)
            g.aabb_buffer = m_bboxes_ptr;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SDFgrid[" << std::endl
            << "  to_world = " << string::indent(m_to_world, 13) << ","
            << std::endl
            << "  " << string::indent(get_children_string()) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(SDFGrid)
private:
    /// Shared implementation for ray_intersect_preliminary_impl and
    /// ray_test_impl
    template <typename FloatP, typename Ray3fP>
    std::tuple<dr::mask_t<FloatP>, FloatP, Point<FloatP, 2>,
               dr::uint32_array_t<FloatP>, dr::uint32_array_t<FloatP>>
        MI_INLINE ray_intersect_preliminary_common_impl(
            const Ray3fP &ray_, dr::uint32_array_t<FloatP> prim_index,
            dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);

        using UInt32P   = dr::uint32_array_t<FloatP>;
        using Vector3uP = Vector<UInt32P, 3>;
        using Vector3fP = Vector<FloatP, 3>;
        using MaskP3    = dr::mask_t<Vector3fP>;
        using Point3fP  = Point<FloatP, 3>;

        uint32_t shape_v[3] = { m_res.x(), m_res.y(), m_res.z() };

        // Scalar variants read host memory. JIT variants gather from the
        // evaluated buffers, which the recorded intersection function captures.
        Ray3fP ray;
        UInt32P grid_index;
        if constexpr (dr::is_jit_v<FloatP>) {
            ray = m_to_world.value().inverse() * ray_;
            grid_index = dr::gather<UInt32P>(m_jit_voxel_indices, prim_index, active);
        } else {
            ray = m_to_world.scalar().inverse() * ray_;
            grid_index = m_voxel_indices_ptr[prim_index];
        }

        Vector3fP grid_scale((float) (shape_v[0] - 1), (float) (shape_v[1] - 1),
                             (float) (shape_v[2] - 1)),
                  voxel_size(1.f / (float) (shape_v[0] - 1),
                             1.f / (float) (shape_v[1] - 1),
                             1.f / (float) (shape_v[2] - 1));

        // Sample index of the voxel's lower corner, which also serves as the
        // base address of the eight corner loads
        UInt32P yz = grid_index / shape_v[0];
        Vector3uP voxel_pos(grid_index % shape_v[0], yz % shape_v[1],
                            yz / shape_v[1]);
        Vector3fP voxel_pos_f(voxel_pos);

        // Corner values, indexed as x + 2y + 4z relative to the lower corner
        FloatP s[8];
        for (uint32_t i = 0; i < 8; ++i) {
            uint32_t offset = ((i >> 2) * shape_v[1] + ((i >> 1) & 1)) * shape_v[0] + (i & 1);
            s[i] = gather_grid<FloatP>(grid_index + offset, active);
        }

        // Voxel AABB in object space
        BoundingBox<Point3fP> bbox_local(
            Point3fP(voxel_pos_f * voxel_size),
            Point3fP((voxel_pos_f + 1.f) * voxel_size));

        // Slab test that also identifies the faces through which the ray
        // enters and exits the voxel. A ray parallel to a slab has infinite
        // distances of equal sign when it lies outside, and NaN distances
        // when it lies exactly in a boundary plane, which counts as inside.
        // Embree does not always test a ray against the bounding box of a
        // user primitive before calling its intersection function, which is
        // why the routine cannot rely on that test.
        Vector3fP d_rcp = dr::rcp(ray.d);
        MaskP3 d_pos = d_rcp >= 0;
        Vector3fP t_slab_min =
            (dr::select(d_pos, bbox_local.min, bbox_local.max) - ray.o) * d_rcp;
        Vector3fP t_slab_max =
            (dr::select(d_pos, bbox_local.max, bbox_local.min) - ray.o) * d_rcp;
        Vector3fP inf(dr::Infinity<FloatP>);
        t_slab_min = dr::select(dr::isnan(t_slab_min), -inf, t_slab_min);
        t_slab_max = dr::select(dr::isnan(t_slab_max), inf, t_slab_max);

        FloatP t_bbox_beg = dr::maximum(dr::max(t_slab_min), 0.f),
               t_bbox_end = dr::min(t_slab_max);

        // Rays that start inside the voxel have negative slab entry
        // distances and therefore do not lie on an entry face
        MaskP3 on_beg_face = t_slab_min == Vector3fP(t_bbox_beg),
               on_end_face = t_slab_max == Vector3fP(t_bbox_end);

        active &= t_bbox_beg < t_bbox_end && t_bbox_beg <= ray.maxt;

        // Convert ray to voxel-space [0, 1] x [0, 1] x [0, 1]. The direction
        // is scaled but not normalized, so distances along the ray are unchanged.
        ray.o = Point3fP(Vector3fP(ray.o) * grid_scale - voxel_pos_f);
        ray.d = ray.d * grid_scale;

        Vector3fP p_beg = ray(t_bbox_beg), p_end = ray(t_bbox_end);

        // SDF values where the ray enters and exits the voxel. The coordinate
        // of the crossed face is snapped to 0 or 1, where the interpolation
        // is exact, so that two neighboring voxels compute bitwise identical
        // values on their shared face. Evaluating the cubic below at these
        // points instead can yield opposite signs in the two voxels when the
        // surface passes close to the face, in which case neither voxel
        // reports the intersection.
        Vector3fP zero(0.f), one(1.f);
        FloatP f_beg = trilerp(s, dr::clip(dr::select(on_beg_face,
                                   dr::select(d_pos, zero, one), p_beg), 0.f, 1.f)),
               f_end = trilerp(s, dr::clip(dr::select(on_end_face,
                                   dr::select(d_pos, one, zero), p_end), 0.f, 1.f));

        /**
           Voxel intersection expressed as solution of cubic polynomial:

           Herman Hansson-Söderlund, Alex Evans, and Tomas Akenine-Möller, Ray
           Tracing of Signed Distance Function Grids, Journal of Computer
           Graphics Techniques (JCGT), vol. 11, no. 3, 94-113, 2022
        */
        FloatP c0;
        FloatP c1;
        FloatP c2;
        FloatP c3;
        {
            FloatP o_x = p_beg.x();
            FloatP o_y = p_beg.y();
            FloatP o_z = p_beg.z();

            FloatP d_x = ray.d.x();
            FloatP d_y = ray.d.y();
            FloatP d_z = ray.d.z();

            FloatP a  = s[5] - s[4];
            FloatP k0 = s[0];
            FloatP k1 = s[1] - s[0];
            FloatP k2 = s[2] - s[0];
            FloatP k3 = s[3] - s[2] - k1;
            FloatP k4 = s[4] - k0;
            FloatP k5 = a - k1;
            FloatP k6 = (s[6] - s[4]) - k2;
            FloatP k7 = (s[7] - s[6] - a) - k3;
            FloatP m0 = o_x * o_y;
            FloatP m1 = d_x * d_y;
            FloatP m2 = dr::fmadd(o_x, d_y, o_y * d_x);
            FloatP m3 = dr::fmadd(k5, o_z, k1);
            FloatP m4 = dr::fmadd(k6, o_z, k2);
            FloatP m5 = dr::fmadd(k7, o_z, k3);

            c0 = dr::fmadd(k4, o_z, k0) +
                 dr::fmadd(o_x, m3, dr::fmadd(o_y, m4, m0 * m5));
            c1 = dr::fmadd(d_x, m3, d_y * m4) + m2 * m5 +
                 d_z * (k4 + dr::fmadd(k5, o_x, dr::fmadd(k6, o_y, k7 * m0)));
            c2 = dr::fmadd(
                m1, m5,
                d_z * (dr::fmadd(k5, d_x, dr::fmadd(k6, d_y, k7 * m2))));
            c3 = k7 * m1 * d_z;
        }

        auto [hit, t] = sdf_solve_cubic(t_bbox_end - t_bbox_beg, f_beg, f_end,
                                        c3, c2, c1, c0, active);
        t += t_bbox_beg;

        // The solver keeps t within the voxel, and NaN fails this test
        active &= hit && t <= ray.maxt;

        return { active, dr::select(active, t, dr::Infinity<FloatP>),
                 Point<FloatP, 2>(0.f, 0.f), ((uint32_t) -1), prim_index };
    }

    /* Solve cubic polynomial that gives solution to voxel intersection
     *
     *  M ARMITT, G., K LEER , A., WALD , I., AND F RIEDRICH , H.
     *  2004. Fast and accurate ray-voxel intersection techniques for
     *  iso-surface ray tracing.
     */
    template <typename FloatP>
    MI_INLINE std::tuple<dr::mask_t<FloatP>, FloatP>
    sdf_solve_cubic(FloatP t_end, FloatP f_beg, FloatP f_end,
                    FloatP c3, FloatP c2, FloatP c1, FloatP c0,
                    dr::mask_t<FloatP> active) const {
        using MaskP   = dr::mask_t<FloatP>;
        using UInt32P = dr::uint32_array_t<FloatP>;

        // Coefficients of the derivative
        FloatP d2 = c3 * 3.f, d1 = c2 * 2.f;

        MaskP has_derivative_roots;
        FloatP root_0, root_1;
        std::tie(has_derivative_roots, root_0, root_1) =
            math::solve_quadratic(d2, d1, c1);

        auto eval_sdf = [&](FloatP t) -> FloatP {
            return dr::fmadd(dr::fmadd(dr::fmadd(c3, t, c2), t, c1), t, c0);
        };

        FloatP t_near = 0.f, t_far = t_end,
               f_near = f_beg, f_far = f_end;

        // Split the interval at the roots of the derivative so that the
        // remaining bracket is monotonic
        auto split = [&](const FloatP &root) {
            FloatP f_root = eval_sdf(root);
            MaskP valid = has_derivative_roots && t_near <= root && root <= t_far,
                  left  = f_near * f_root <= 0.f;
            dr::masked(t_far,  valid && left)  = root;
            dr::masked(f_far,  valid && left)  = f_root;
            dr::masked(t_near, valid && !left) = root;
            dr::masked(f_near, valid && !left) = f_root;
        };

        split(root_0);
        split(root_1);

        active &= f_near * f_far <= 0.f;

        // Start from the secant estimate, falling back to the midpoint if
        // both endpoints vanish or rounding puts the estimate outside
        FloatP t = t_near + (t_far - t_near) * (-f_near / (f_far - f_near));
        t = dr::select(t >= t_near && t <= t_far, t, .5f * (t_near + t_far));

        // Newton's method, safeguarded by bisection of the bracketing
        // interval. Only the sign of f_near is needed below, and it does
        // not change as the bracket shrinks. Lanes without a bracketed
        // root do not iterate.
        static constexpr uint32_t num_solve_max_iter = 50;
        UInt32P i = 0;
        MaskP done = !active;

        // Runs as a plain loop in scalar variants and symbolically otherwise
        dr::tie(t, t_near, t_far, i, done) = dr::while_loop(
            dr::make_tuple(t, t_near, t_far, i, done),
            [](const FloatP &, const FloatP &, const FloatP &,
               const UInt32P &, const MaskP &done) { return !done; },
            [&](FloatP &t, FloatP &t_near, FloatP &t_far, UInt32P &i,
                MaskP &done) {
                FloatP f_t = eval_sdf(t);
                MaskP left = f_t * f_near <= 0.f;
                t_far  = dr::select(left, t, t_far);
                t_near = dr::select(left, t_near, t);

                // Bisect when the Newton step leaves the bracket or is NaN.
                // An exact root ends the iteration at its position.
                FloatP deriv = dr::fmadd(dr::fmadd(d2, t, d1), t, c1),
                       t_next = t - f_t / deriv;
                MaskP root = f_t == 0.f,
                      inside = t_next > t_near && t_next < t_far;
                t_next = dr::select(root, t,
                    dr::select(inside, t_next, .5f * (t_near + t_far)));

                i += 1;
                // Negated comparison so that NaN lanes terminate
                done = !(dr::abs(t_next - t) >= NumSolveEpsilon) ||
                       (num_solve_max_iter < i);
                t = t_next;
            },
            "SDFGrid::numerical_solve");

        return { active, t };
    }

    /// Bilinear interpolant of the values 'f' (indexed u + 2v) at (u, v).
    /// dr::lerp() reproduces the endpoint values exactly at 0 and 1, which
    /// the face-consistent evaluation of the intersection routine relies on.
    template <typename Value>
    static MI_INLINE Value bilerp(const Value (&f)[4], const Value &u,
                                  const Value &v) {
        return dr::lerp(dr::lerp(f[0], f[1], u), dr::lerp(f[2], f[3], u), v);
    }

    /// Trilinear interpolant of the corner values 'f' (indexed x + 2y + 4z)
    /// at a position within the unit cube
    template <typename Value, typename Vec3>
    static MI_INLINE Value trilerp(const Value (&f)[8], const Vec3 &p) {
        Value f0[4] = { f[0], f[1], f[2], f[3] },
              f1[4] = { f[4], f[5], f[6], f[7] };
        return dr::lerp(bilerp(f0, p.x(), p.y()), bilerp(f1, p.x(), p.y()), p.z());
    }

    /// Gradient of the trilinear interpolant, whose partial derivatives are
    /// bilinear interpolants of the differences along the four parallel edges
    template <typename Value, typename Vec3>
    static MI_INLINE Vec3 trilerp_grad(const Value (&f)[8], const Vec3 &p) {
        Value dx[4] = { f[1] - f[0], f[3] - f[2], f[5] - f[4], f[7] - f[6] },
              dy[4] = { f[2] - f[0], f[3] - f[1], f[6] - f[4], f[7] - f[5] },
              dz[4] = { f[4] - f[0], f[5] - f[1], f[6] - f[2], f[7] - f[3] };
        return Vec3(bilerp(dx, p.y(), p.z()),
                    bilerp(dy, p.x(), p.z()),
                    bilerp(dz, p.x(), p.y()));
    }

    /* Given the voxel position, returns tight bounding box around the
     * surface.
     *
     *  Tight Bounding Boxes for Voxels and Bricks in a Signed Distance Field
     *  Ray Tracer. HANSSON-SÖDERLUND, H., AND AKENINE-MÖLLER, T. 2023.
     */
    std::tuple<Mask, InputBoundingBox3f>
    compute_tight_bbox(const ScalarAffineTransform4f& to_world,
                       UInt32 x, UInt32 y, UInt32 z) {
        Vector3f voxel_size(1.f / (m_res.x() - 1.f), 1.f / (m_res.y() - 1.f),
                            1.f / (m_res.z() - 1.f));
        // Corner 'i' has the offset (i & 1, (i >> 1) & 1, i >> 2)
        auto corner = [](uint32_t i) {
            return Point3f((float) (i & 1), (float) ((i >> 1) & 1),
                           (float) (i >> 2));
        };

        InputFloat f[8];
        Mask any_nonneg = false, any_nonpos = false;
        for (uint32_t i = 0; i < 8; ++i) {
            UInt32 index = (x + (i & 1)) + (y + ((i >> 1) & 1)) * m_res.x() +
                           (z + (i >> 2)) * m_res.x() * m_res.y();
            f[i] = gather_grid<InputFloat>(index);
            any_nonneg |= f[i] >= 0;
            any_nonpos |= f[i] <= 0;
        }

        Mask occupied = any_nonneg && any_nonpos;

        // Empty box, expanded by the corners that lie on the surface and by
        // the points where the surface crosses an edge
        InputBoundingBox3f bbox(InputPoint3f(1.f), InputPoint3f(0.f));
        if constexpr (!dr::is_jit_v<Float>)
            if (!occupied)
                return { false, bbox };

        for (uint32_t i = 0; i < 8; ++i) {
            Mask on_surface = f[i] == 0;
            bbox.min = dr::select(on_surface, dr::minimum(bbox.min, corner(i)), bbox.min);
            bbox.max = dr::select(on_surface, dr::maximum(bbox.max, corner(i)), bbox.max);
        }

        for (uint32_t i = 0; i < 8; ++i) {
            for (uint32_t axis = 0; axis < 3; ++axis) {
                if (i & (1u << axis))
                    continue;
                uint32_t j = i | (1u << axis);

                Mask crossing = f[i] * f[j] <= 0 && f[i] != f[j];
                if constexpr (!dr::is_jit_v<Float>)
                    if (!crossing)
                        continue;

                Point3f p = corner(i);
                p[axis] = f[i] / (f[i] - f[j]);
                bbox.min = dr::select(crossing, dr::minimum(bbox.min, p), bbox.min);
                bbox.max = dr::select(crossing, dr::maximum(bbox.max, p), bbox.max);
            }
        }

        Vector3f offset = Vector3f(Float(x), Float(y), Float(z));
        bbox.min = to_world * ((bbox.min + offset) * voxel_size);
        bbox.max = to_world * ((bbox.max + offset) * voxel_size);

        // Pad the box. Rounding here and in the ray-box tests of the
        // acceleration structure could otherwise exclude hits on its boundary.
        Vector3f pad = dr::abs(to_world * voxel_size) * 1e-3f +
                       dr::maximum(dr::abs(bbox.min), dr::abs(bbox.max)) *
                           (8.f * dr::Epsilon<float>);
        bbox.min -= pad;
        bbox.max += pad;

        return { occupied, bbox };
    };

    /* Only computes AABBs for voxel that contain a surface in it.
     * Returns a pointer to the array of AABBs, a pointer to an array with the
     * sample-grid index of each voxel's lower corner, and the count of voxels
     * with surface in them.
     *
     * Depending on the variant used, the pointer returned is either host or
     * device visible
     */
    std::tuple<void *, uint32_t *, uint32_t> build_bboxes() {
        uint32_t shape_v[3] = { m_res.x(), m_res.y(), m_res.z() };
        ScalarAffineTransform4f to_world = m_to_world.scalar();

        dr::eval(m_grid.array(), m_grid_half.array());

        void *aabbs_ptr = nullptr;
        uint32_t *voxel_indices_ptr = nullptr;

        uint32_t count = 0;

        if constexpr (dr::is_jit_v<Float>) {
            auto [z, y, x] = dr::meshgrid(dr::arange<UInt32>(shape_v[2] - 1),
                                          dr::arange<UInt32>(shape_v[1] - 1),
                                          dr::arange<UInt32>(shape_v[0] - 1), false);

            auto [occupied, bbox] = compute_tight_bbox(to_world, x, y, z);

            UInt32 grid_index = (z * shape_v[1] + y) * shape_v[0] + x;

            UInt32 counter = UInt32(0);
            UInt32 slot = dr::scatter_inc(counter, UInt32(0), occupied);
            dr::eval(slot);
            count = counter[0];

            // The caller reports the error for grids without any surface
            if (count == 0)
                return { nullptr, nullptr, 0 };

            // Only the occupied voxels are stored
            m_jit_voxel_indices = dr::zeros<UInt32>(count);

            uint32_t stride = 3; // BBox's Point3f corner stride (floats per corner)
            if constexpr (dr::is_llvm_v<Float>)
                stride = sizeof(InputScalarBoundingBox3f) / sizeof(float) / 2u; // Typically 4-wide

            m_jit_bboxes = dr::zeros<InputFloat>(2 * stride * count);
            dr::scatter(m_jit_bboxes, bbox.min.x(), stride * (2 * slot + 0) + 0, occupied, ReduceMode::NoConflicts);
            dr::scatter(m_jit_bboxes, bbox.min.y(), stride * (2 * slot + 0) + 1, occupied, ReduceMode::NoConflicts);
            dr::scatter(m_jit_bboxes, bbox.min.z(), stride * (2 * slot + 0) + 2, occupied, ReduceMode::NoConflicts);
            dr::scatter(m_jit_bboxes, bbox.max.x(), stride * (2 * slot + 1) + 0, occupied, ReduceMode::NoConflicts);
            dr::scatter(m_jit_bboxes, bbox.max.y(), stride * (2 * slot + 1) + 1, occupied, ReduceMode::NoConflicts);
            dr::scatter(m_jit_bboxes, bbox.max.z(), stride * (2 * slot + 1) + 2, occupied, ReduceMode::NoConflicts);
            dr::scatter(m_jit_voxel_indices, grid_index, slot, occupied, ReduceMode::NoConflicts);
            dr::eval(m_jit_voxel_indices, m_jit_bboxes);

            aabbs_ptr = (void *) m_jit_bboxes.data();
            voxel_indices_ptr = (uint32_t*) m_jit_voxel_indices.data();
        } else {
            uint32_t max_voxel_count =
                (shape_v[0] - 1) * (shape_v[1] - 1) * (shape_v[2] - 1);
            aabbs_ptr = (ScalarBoundingBox3f*) jit_malloc(
                JitBackend::None, sizeof(ScalarBoundingBox3f) * max_voxel_count);
            voxel_indices_ptr = (uint32_t *) jit_malloc(
                JitBackend::None, sizeof(uint32_t) * max_voxel_count);

            for (uint32_t z = 0; z < shape_v[2] - 1; ++z) {
                for (uint32_t y = 0; y < shape_v[1] - 1; ++y) {
                    for (uint32_t x = 0; x < shape_v[0] - 1; ++x) {
                        auto [occupied, bbox] =
                            compute_tight_bbox(to_world, x, y, z);

                        if (!occupied)
                            continue;

                        voxel_indices_ptr[count] =
                            (z * shape_v[1] + y) * shape_v[0] + x;
                        ScalarBoundingBox3f *ptr = (ScalarBoundingBox3f *) aabbs_ptr;
                        ptr[count] = ScalarBoundingBox3f(bbox);
                        count++;
                    }
                }
            }

            // The constructor's error for empty grids skips the destructor,
            // which would otherwise release these buffers
            if (count == 0) {
                jit_free(aabbs_ptr);
                jit_free(voxel_indices_ptr);
                aabbs_ptr = nullptr;
                voxel_indices_ptr = nullptr;
            }
        }

        return { aabbs_ptr, voxel_indices_ptr, count };
    }

    /// Load grid samples, converting them to the requested type
    template <typename Value, typename Index>
    MI_INLINE Value gather_grid(const Index &index,
                                const dr::mask_t<Value> &active = true) const {
        if (m_half)
            return Value(dr::gather<dr::float16_array_t<Value>>(
                m_grid_half.array(), index, active));
        else
            return Value(dr::gather<dr::float32_array_t<Value>>(
                m_grid.array(), index, active));
    }

    /// Coordinates of the voxel whose lower corner has the given sample index
    Vector3u voxel_coords(const UInt32 &grid_index) const {
        UInt32 yz = grid_index / m_res.x();
        return Vector3u(grid_index % m_res.x(), yz % m_res.y(), yz / m_res.y());
    }

    /// Corner values of the voxel whose lower corner has the given sample
    /// index, indexed as x + 2y + 4z
    void gather_corners(const UInt32 &grid_index, Mask active,
                        Float (&f)[8]) const {
        for (uint32_t i = 0; i < 8; ++i) {
            uint32_t offset = (i & 1u) + m_res.x() * (((i >> 1) & 1u) + m_res.y() * (i >> 2));
            f[i] = gather_grid<Float>(grid_index + offset, active);
        }
    }

    /// Value and gradient of the trilinear interpolant within the voxel
    /// whose lower corner has the given sample index
    std::pair<Float, Vector3f> sdf_eval(const Point3f &p,
                                        const UInt32 &grid_index,
                                        Mask active) const {
        Vector3f resolution(m_res.x() - 1.f, m_res.y() - 1.f, m_res.z() - 1.f);
        Vector3f w = dr::clip(p * resolution - Vector3f(voxel_coords(grid_index)),
                              0.f, 1.f);

        Float f[8];
        gather_corners(grid_index, active, f);

        return { trilerp(f, w), trilerp_grad(f, w) * resolution };
    }

    enum NormalMethod {
        Analytic,
        Smooth,
    };

    /// SDF samples, packed [Z, Y, X, 1], in single or half precision
    InputTensorXf m_grid;
    TensorXf16 m_grid_half;
    bool m_half = false;
    /// Number of samples along the X, Y, and Z axes
    ScalarVector3u m_res;

    // Non-empty bounding boxes and sample-grid indices of their lower corners
    InputFloat m_jit_bboxes;
    UInt32 m_jit_voxel_indices;

    // Pointers to non-empty bounding boxes and corresponding indices
    // (These are just the data pointer to the JIT variables aboves.
    // In scalar variants, these are allocated  using `jit_malloc`)
    void *m_bboxes_ptr = nullptr;
    uint32_t *m_voxel_indices_ptr = nullptr;

    uint32_t m_filled_voxel_count = 0;
    NormalMethod m_normal_method;

    MI_TRAVERSE_CB(Base, m_grid, m_grid_half,
                   m_jit_bboxes, m_jit_voxel_indices)
};

MI_EXPORT_PLUGIN(SDFGrid)
NAMESPACE_END(mitsuba)
