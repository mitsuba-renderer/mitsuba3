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
#include <mitsuba/render/volumegrid.h>

#include <drjit/tensor.h>
#include <drjit/texture.h>

#if defined(MI_ENABLE_EMBREE)
#include <embree3/rtcore.h>
#endif

#if defined(MI_ENABLE_CUDA)
#include "optix/sdfgrid.cuh"
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
   - Tensor array containing the grid data. This parameter can only be specified
     when building this plugin at runtime from Python or C++ and cannot be
     specified in the XML scene description.
   - |exposed|, |differentiable|, |discontinuous|

 * - watertight
   - |bool|
   - Is the associated surface watertight, i.e. does the surface contain no holes? (Default: |true|)
   - |exposed|

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
    MI_IMPORT_BASE(Shape, m_to_world, m_to_object, m_is_instance, initialize,
                   mark_dirty, get_children_string, parameters_grad_enabled)
    MI_IMPORT_TYPES()

    // Grid texture is always stored in single precision
    using InputFloat     = dr::replace_scalar_t<Float, float>;
    using InputTexture3f = dr::Texture<InputFloat, 3>;
    using InputPoint3f   = Point<InputFloat, 3>;
    using InputTensorXf  = dr::Tensor<DynamicBuffer<InputFloat>>;

    using typename Base::ScalarIndex;
    using typename Base::ScalarSize;

    SDFGrid(const Properties &props) : Base(props) {
#if !defined(MI_ENABLE_EMBREE)
        if constexpr (!dr::is_jit_v<Float>)
            Throw("The SDF grid is only available with Embree in scalar "
                  "variants!");
#endif

        std::string normals_mode_str = props.string("normals", "smooth");
        if (normals_mode_str == "analytic")
            m_normal_method = Analytic;
        else if (normals_mode_str == "smooth")
            m_normal_method = Smooth;
        else
            Throw("Invalid normals mode \"%s\", must be one of: \"analytic\", "
                  "or \"smooth\"!",
                  normals_mode_str);

        m_watertight = props.get<bool>("watertight", true);

        if (props.has_property("filename")) {
            FileResolver *fs   = Thread::thread()->file_resolver();
            fs::path file_path = fs->resolve(props.string("filename"));
            if (!fs::exists(file_path))
                Log(Error, "\"%s\": file does not exist!", file_path);
            VolumeGrid<float, Color<float, 3>> vol_grid(file_path);
            ScalarVector3i res = vol_grid.size();
            size_t shape[4]    = { (size_t) res.z(), (size_t) res.y(),
                                   (size_t) res.x(), 1 };
            if (vol_grid.channel_count() != 1)
                Throw(
                    "SDF grid data source \"%s\" has %lu channels, expected 1.",
                    file_path, vol_grid.channel_count());

            m_grid_texture = InputTexture3f(
                InputTensorXf(vol_grid.data(), 4, shape), true, true,
                dr::FilterMode::Linear, dr::WrapMode::Clamp);
        } else if (props.has_property("grid")) {
            TensorXf* tensor = props.tensor<TensorXf>("grid");
            if (tensor->ndim() != 4)
                Throw(
                    "SDF grid tensor has dimension %lu, expected 4",
                    tensor->ndim());
            if (tensor->shape(3) != 1)
                Throw(
                    "SDF grid shape at index 3 is %lu, expected 1",
                    tensor->shape(3));
            m_grid_texture = InputTexture3f(*tensor, true, true,
                dr::FilterMode::Linear, dr::WrapMode::Clamp);
        } else {
            InputFloat default_data[8] = { -1.f, -1.f, -1.f, -1.f,
                                           -1.f, -1.f, -1.f, -1.f };
            size_t default_shape[4]    = { 2, 2, 2, 1 };
            m_grid_texture = InputTexture3f(
                InputTensorXf(default_data, 4, default_shape), true, true,
                dr::FilterMode::Linear, dr::WrapMode::Clamp);
        }
        update();
        initialize();
    }

    ~SDFGrid() {
        jit_free(m_host_bboxes);
        jit_free(m_host_voxel_indices);
        jit_free(m_device_bboxes);
        jit_free(m_device_voxel_indices);
    }

    void update() {
        auto [S, Q, T] =
            dr::transform_decompose(m_to_world.scalar().matrix, 25);
        if (dr::abs(Q[0]) > 1e-6f || dr::abs(Q[1]) > 1e-6f ||
            dr::abs(Q[2]) > 1e-6f || dr::abs(Q[3] - 1) > 1e-6f)
            Log(Warn, "'to_world' transform shouldn't perform any rotations, "
                      "use instancing (`shapegroup` and `instance` plugins) "
                      "instead!");

        m_to_object = m_to_world.value().inverse();

        auto shape = m_grid_texture.tensor().shape();
        Vector3f voxel_size(0.f);
        for (size_t i = 0; i < 3; ++i) {
            m_inv_shape[i] = 1.f / shape[i];
            voxel_size[i] = 1.f / (shape[i] - 1);
        }
        m_voxel_size = voxel_size;
        dr::make_opaque(m_inv_shape, m_voxel_size);

        if constexpr (!dr::is_cuda_v<Float>) {
            dr::eval(m_grid_texture.value()); // Make sure the SDF data is evaluated
            m_host_grid_data = m_grid_texture.tensor().data();
        }

        jit_free(m_host_bboxes);
        jit_free(m_host_voxel_indices);
        jit_free(m_device_bboxes);
        jit_free(m_device_voxel_indices);
        std::tie(m_host_bboxes,
                 m_host_voxel_indices,
                 m_device_bboxes,
                 m_device_voxel_indices,
                 m_filled_voxel_count) = build_bboxes();
        if (m_filled_voxel_count == 0)
            Throw("SDFGrid should at least have one non-empty voxel!");

        mark_dirty();
    }

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
        callback->put_parameter("to_world",     *m_to_world.ptr(),          ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_parameter("grid",         m_grid_texture.tensor(),    ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_parameter("watertight",   m_watertight,               +ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "to_world") ||
            string::contains(keys, "grid") || string::contains(keys, "watertight")) {
            // Ensure previous ray-tracing operation are fully evaluated before
            // modifying the scalar values of the fields in this class
            if constexpr (dr::is_jit_v<Float>)
                dr::sync_thread();

            // Update the scalar value of the matrix
            m_to_world = m_to_world.value();
            m_grid_texture.set_tensor(m_grid_texture.tensor());

            update();
        }

        Base::parameters_changed();
    }

    ScalarSize primitive_count() const override { return m_filled_voxel_count; }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox;
        ScalarTransform4f to_world = m_to_world.scalar();

        bbox.expand(to_world.transform_affine(ScalarPoint3f(0.f, 0.f, 0.f)));
        bbox.expand(to_world.transform_affine(ScalarPoint3f(1.f, 0.f, 0.f)));
        bbox.expand(to_world.transform_affine(ScalarPoint3f(0.f, 1.f, 0.f)));
        bbox.expand(to_world.transform_affine(ScalarPoint3f(1.f, 1.f, 0.f)));
        bbox.expand(to_world.transform_affine(ScalarPoint3f(0.f, 0.f, 1.f)));
        bbox.expand(to_world.transform_affine(ScalarPoint3f(1.f, 0.f, 1.f)));
        bbox.expand(to_world.transform_affine(ScalarPoint3f(0.f, 1.f, 1.f)));
        bbox.expand(to_world.transform_affine(ScalarPoint3f(1.f, 1.f, 1.f)));

        return bbox;
    }

    ScalarBoundingBox3f bbox(ScalarIndex index) const override {
        if constexpr (dr::is_cuda_v<Float>)
            NotImplementedError("bbox(ScalarIndex index)");

        return reinterpret_cast<ScalarBoundingBox3f*>(m_host_bboxes)[index];
    }

    Float surface_area() const override {
        return 0;
    }

    // =============================================================
    //! @{ \name Sampling routines
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

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    template <typename FloatP, typename Ray3fP>
    std::tuple<FloatP, Point<FloatP, 2>, dr::uint32_array_t<FloatP>,
               dr::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray_,
                                   ScalarIndex prim_index,
                                   dr::mask_t<FloatP> active) const {
        auto [hit, t, uv, shape_index, p] =
            ray_intersect_preliminary_common_impl<FloatP>(ray_, prim_index,
                                                          active);
        return { t, uv, shape_index, p };
    }

    template <typename FloatP, typename Ray3fP>
    dr::mask_t<FloatP> ray_test_impl(const Ray3fP &ray_,
                                     ScalarIndex prim_index,
                                     dr::mask_t<FloatP> active) const {
        auto [hit, t, uv, shape_index, p] =
            ray_intersect_preliminary_common_impl<FloatP>(ray_, prim_index, active);
        return hit;
    }

    MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    SurfaceInteraction3f
    compute_surface_interaction(const Ray3f &ray,
                                const PreliminaryIntersection3f &pi,
                                uint32_t ray_flags, uint32_t recursion_depth,
                                Mask active) const override {
        MI_MASK_ARGUMENT(active);
        constexpr bool IsDiff = dr::is_diff_v<Float>;

        // Early exit when tracing isn't necessary
        if (!m_is_instance && recursion_depth > 0)
            return dr::zeros<SurfaceInteraction3f>();

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

        bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape);
        bool follow_shape = has_flag(ray_flags, RayFlags::FollowShape);

        Transform4f to_world  = m_to_world.value();
        Transform4f to_object = m_to_object.value();

        dr::suspend_grad<Float> scope(detach_shape, to_world, to_object,
                                      m_grid_texture.value());

        if constexpr (IsDiff) {
            if (follow_shape) {
                /* FollowShape glues the interaction point with the shape.
                   Therefore, to also account for a possible differential motion
                   of the shape, we first compute a detached intersection point
                   in local space and transform it back in world space to get a
                   point rigidly attached to the shape's motion, including
                   translation, scaling and rotation. */
                Point3f local_p =
                    dr::detach(to_object.transform_affine(ray(pi.t)));
                Vector3f local_grad = dr::detach(sdf_grad(local_p));
                Normal3f local_n    = dr::normalize(local_grad);
                Ray3f local_ray = dr::detach(to_object.transform_affine(ray));

                /* Note: Only when applying a motion to the entire shape is the
                 * interaction point truly "glued" to the shape. For a single
                 * voxel, the motion of the surface is ambiguous and therefore
                 * the interaction point is not "glued" to the shape. */

                // Capture gradients of `m_grid_texture`
                InputFloat sdf_value;
                m_grid_texture.eval(rescale_point(local_p), &sdf_value);
                Point3f local_motion =
                    sdf_value * (-local_n) / dr::dot(local_n, local_grad);
                local_p = dr::replace_grad(local_p, local_motion);

                // Capture gradients of `m_to_world`
                si.p = to_world.transform_affine(local_p);
                si.t = dr::sqrt(dr::squared_norm(si.p - ray.o) /
                                dr::squared_norm(ray.d));
            } else {
                /* To ensure that the differential interaction point stays along
                   the traced ray, we first recompute the intersection distance
                   in a differentiable way (w.r.t. to the grid parameters) and
                   then compute the corresponding point along the ray. (Instead
                   of computing an intersection with the SDF, we compute an
                   intersection with the tangent plane.) */
                Point3f local_p =
                    dr::detach(to_object.transform_affine(ray(pi.t)));
                Ray3f local_ray = dr::detach(to_object.transform_affine(ray));

                /// Differentiable tangent plane normal
                // Capture gradients of `m_grid_texture`
                Normal3f local_n = dr::normalize(sdf_grad(local_p));
                // Capture gradients of `m_to_world`
                Normal3f n = to_world.transform_affine(local_n);

                /// Differentiable tangent plane point
                // Capture gradients of `m_grid_texture`
                InputFloat sdf_value;
                m_grid_texture.eval(rescale_point(local_p), &sdf_value);

                Float t_diff =
                    sdf_value / dr::dot(dr::detach(local_n), -local_ray.d);
                t_diff = dr::replace_grad(pi.t, t_diff);
                // Capture gradients of `m_to_world`
                Point3f p = to_world.transform_affine(local_ray(t_diff));

                si.t = dr::dot(p - ray.o, n) / dr::dot(n, ray.d);
                si.p = ray(si.t);
            }
        } else {
            si.t = pi.t;
            si.p = ray(si.t);
        }

        si.t = dr::select(active, si.t, dr::Infinity<Float>);

        Vector3f grad = sdf_grad(m_to_object.value().transform_affine(si.p));

        si.n =
            dr::normalize(m_to_world.value().transform_affine(Normal3f(grad)));

        if (likely(has_flag(ray_flags, RayFlags::ShadingFrame))) {
            switch (m_normal_method) {
                case Analytic:
                    si.sh_frame.n = si.n;
                    break;
                case Smooth:
                    si.sh_frame.n =
                        smooth(m_to_object.value().transform_affine(si.p));
                    break;
                default:
                    Throw("Unknown normal computation.");
            }
        }

        si.uv    = Point2f(0.f, 0.f);
        si.dp_du = Vector3f(0.f);
        si.dp_dv = Vector3f(0.f);
        si.dn_du = si.dn_dv = dr::zeros<Vector3f>();

        si.shape    = this;
        si.instance = nullptr;

        if (unlikely(has_flag(ray_flags, RayFlags::BoundaryTest))) {
            Float dp = dr::dot(si.sh_frame.n, -ray.d);
            // Add non-linearity by squaring the returned value
            si.boundary_test = dr::sqr(dp);
        }

        return si;
    }

    Normal3f smooth_sh(const Point3f &p, const Float *u_ptr, const Float *v_ptr,
                       const Float *w_ptr) const {
        /**
           Herman Hansson-Söderlund, Alex Evans, and Tomas Akenine-Möller, Ray
           Tracing of Signed Distance Function Grids, Journal of Computer
           Graphics Techniques (JCGT), vol. 11, no. 3, 94-113, 2022
        */
        auto shape = m_grid_texture.tensor().shape();
        Vector3f resolution = Vector3f(shape[2] - 1, shape[1] - 1, shape[0] - 1);
        Point3f scaled_p = p * resolution;

        Point3i v000 = Point3i(round(scaled_p)) + Vector3i(-1, -1, -1);
        Point3i v100 = v000 + Vector3i(1, 0, 0);
        Point3i v010 = v000 + Vector3i(0, 1, 0);
        Point3i v110 = v000 + Vector3i(1, 1, 0);
        Point3i v001 = v000 + Vector3i(0, 0, 1);
        Point3i v101 = v000 + Vector3i(1, 0, 1);
        Point3i v011 = v000 + Vector3i(0, 1, 1);
        Point3i v111 = v000 + Vector3i(1, 1, 1);

        // Detect voxels that are outside of the grid, their normals will not
        // be used in the interpolation
        Bool s000 = !dr::any(v000 < 0);
        Bool s100 = !dr::any(v100 < 0);
        Bool s010 = !dr::any(v010 < 0);
        Bool s110 = !dr::any(v110 < 0);
        Bool s001 = !dr::any(v001 < 0);
        Bool s101 = !dr::any(v101 < 0);
        Bool s011 = !dr::any(v011 < 0);
        Bool s111 = !dr::any(v111 < 0);

        Vector3f n000 =
            dr::select(s000, dr::normalize(voxel_grad(p, v000)), Vector3f(0.f));
        Vector3f n100 =
            dr::select(s100, dr::normalize(voxel_grad(p, v100)), Vector3f(0.f));
        Vector3f n010 =
            dr::select(s010, dr::normalize(voxel_grad(p, v010)), Vector3f(0.f));
        Vector3f n110 =
            dr::select(s110, dr::normalize(voxel_grad(p, v110)), Vector3f(0.f));
        Vector3f n001 =
            dr::select(s001, dr::normalize(voxel_grad(p, v001)), Vector3f(0.f));
        Vector3f n101 =
            dr::select(s101, dr::normalize(voxel_grad(p, v101)), Vector3f(0.f));
        Vector3f n011 =
            dr::select(s011, dr::normalize(voxel_grad(p, v011)), Vector3f(0.f));
        Vector3f n111 =
            dr::select(s111, dr::normalize(voxel_grad(p, v111)), Vector3f(0.f));

        Vector3f diff = scaled_p - Vector3f(v111) + Vector3f(0.5);
        Float &u      = diff[0];
        Float &v      = diff[1];
        Float &w      = diff[2];
        if (u_ptr)
            u = *u_ptr;
        if (v_ptr)
            v = *v_ptr;
        if (w_ptr)
            w = *w_ptr;

        // Disable weighting on invalid axis
        Bool invalid_x_0 = !s000 && !s010 && !s001 && !s011;
        Bool invalid_x_1 = !s100 && !s110 && !s101 && !s111;
        Bool invalid_y_0 = !s000 && !s100 && !s001 && !s101;
        Bool invalid_y_1 = !s010 && !s110 && !s011 && !s111;
        Bool invalid_z_0 = !s000 && !s100 && !s010 && !s110;
        Bool invalid_z_1 = !s001 && !s101 && !s011 && !s111;

        u = dr::select(invalid_x_0, 1, u);
        u = dr::select(invalid_x_1, 0, u);
        v = dr::select(invalid_y_0, 1, v);
        v = dr::select(invalid_y_1, 0, v);
        w = dr::select(invalid_z_0, 1, w);
        w = dr::select(invalid_z_1, 0, w);

        Normal3f n = (1 - w) * ((1 - v) * ((1 - u) * n000 + u * n100) +
                                v * ((1 - u) * n010 + u * n110)) +
                           w * ((1 - v) * ((1 - u) * n001 + u * n101) +
                                v * ((1 - u) * n011 + u * n111));

        return n;
    };

    Normal3f smooth(const Point3f &p) const {
        Normal3f n = smooth_sh(p, nullptr, nullptr, nullptr);
        return dr::normalize(m_to_world.value().transform_affine(Normal3f(n)));
    }

    bool parameters_grad_enabled() const override {
        return dr::grad_enabled(m_to_world);
    }

#if defined(MI_ENABLE_CUDA)
    using Base::m_optix_data_ptr;

    void optix_prepare_geometry() override {
        if constexpr (dr::is_cuda_v<Float>) {
            if (!m_optix_data_ptr)
                m_optix_data_ptr =
                    jit_malloc(AllocType::Device, sizeof(OptixSDFGridData));

            size_t resolution[3] = { m_grid_texture.tensor().shape()[2],
                                     m_grid_texture.tensor().shape()[1],
                                     m_grid_texture.tensor().shape()[0] };

            dr::eval(m_grid_texture.value()); // Make sure the SDF data is evaluated
            OptixSDFGridData data = { (size_t *) m_device_voxel_indices,
                                      resolution[0],
                                      resolution[1],
                                      resolution[2],
                                      m_voxel_size.scalar()[0],
                                      m_voxel_size.scalar()[1],
                                      m_voxel_size.scalar()[2],
                                      m_grid_texture.tensor().array().data(),
                                      m_to_object.scalar(),
                                      m_watertight };
            jit_memcpy(JitBackend::CUDA, m_optix_data_ptr, &data,
                       sizeof(OptixSDFGridData));
        }
    }

    void optix_build_input(OptixBuildInput &build_input) const override {
        build_input.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
        build_input.customPrimitiveArray.aabbBuffers   = &m_device_bboxes;
        build_input.customPrimitiveArray.numPrimitives = m_filled_voxel_count;
        build_input.customPrimitiveArray.strideInBytes = 6 * sizeof(float);
        build_input.customPrimitiveArray.flags         = optix_geometry_flags;
        build_input.customPrimitiveArray.numSbtRecords = 1;
    }
#endif

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SDFgrid[" << std::endl
            << "  to_world = " << string::indent(m_to_world, 13) << ","
            << std::endl
            << "  " << string::indent(get_children_string()) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    /// Shared implementation for ray_intersect_preliminary_impl and
    /// ray_test_impl
    template <typename FloatP, typename Ray3fP>
    std::tuple<dr::mask_t<FloatP>, FloatP, Point<FloatP, 2>,
               dr::uint32_array_t<FloatP>, dr::uint32_array_t<FloatP>>
        MI_INLINE ray_intersect_preliminary_common_impl(
            const Ray3fP &ray_, ScalarIndex prim_index, dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);

        // The current implementation doesn't support JIT types so don't try to
        // use this in for instance compute_surface_interaction
        if constexpr (dr::is_jit_v<FloatP>)
            NotImplementedError("ray_intersect_preliminary_common_impl");

        Transform<Point<FloatP, 4>> to_object = m_to_object.scalar();
        Ray3fP ray = to_object.transform_affine(ray_);

        auto shape = m_grid_texture.tensor().shape();

        uint32_t voxel_index     = m_host_voxel_indices[prim_index];
        ScalarVector3u voxel_pos = to_voxel_position(voxel_index);

        // Find voxel AABB in object space
        ScalarBoundingBox3f bbox_local;
        {
            ScalarPoint3f bbox_min =
                ScalarPoint3f(voxel_pos.x(), voxel_pos.y(), voxel_pos.z());
            ScalarPoint3f bbox_max = bbox_min + ScalarPoint3f(1.f, 1.f, 1.f);
            bbox_min *= m_voxel_size.scalar();
            bbox_max *= m_voxel_size.scalar();
            bbox_local.expand(bbox_min);
            bbox_local.expand(bbox_max);
        }

        // To determine voxel intersection, we need both near and far AABB
        // intersections
        auto [bbox_hit, t_beg, t_end] = bbox_local.ray_intersect(ray);

        // Convert ray to voxel-space [0, 1] x [0, 1] x [0, 1]
        {
            ScalarMatrix4f m{};
            m[0][0] = (float) (shape[2] - 1);
            m[0][1] = 0.f;
            m[0][2] = 0.f;
            m[0][3] = 0.f;

            m[1][0] = 0.f;
            m[1][1] = (float) (shape[1] - 1);
            m[1][2] = 0.f;
            m[1][3] = 0.f;

            m[2][0] = 0.f;
            m[2][1] = 0.f;
            m[2][2] = (float) (shape[0] - 1);
            m[2][3] = 0.f;

            m[3][0] = -1.f * (float) voxel_pos.x();
            m[3][1] = -1.f * (float) voxel_pos.y();
            m[3][2] = -1.f * (float) voxel_pos.z();
            m[3][3] = 1.f;

            auto to_voxel = ScalarTransform4f(m);

            ray = to_voxel.transform_affine(ray);
        }

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
            ScalarVector3u v000 = voxel_pos;
            ScalarVector3u v100 = v000 + ScalarVector3u(1, 0, 0);
            ScalarVector3u v010 = v000 + ScalarVector3u(0, 1, 0);
            ScalarVector3u v110 = v000 + ScalarVector3u(1, 1, 0);
            ScalarVector3u v001 = v000 + ScalarVector3u(0, 0, 1);
            ScalarVector3u v101 = v000 + ScalarVector3u(1, 0, 1);
            ScalarVector3u v011 = v000 + ScalarVector3u(0, 1, 1);
            ScalarVector3u v111 = v000 + ScalarVector3u(1, 1, 1);

            float s000 = m_host_grid_data[to_voxel_index(v000)];
            float s100 = m_host_grid_data[to_voxel_index(v100)];
            float s010 = m_host_grid_data[to_voxel_index(v010)];
            float s110 = m_host_grid_data[to_voxel_index(v110)];
            float s001 = m_host_grid_data[to_voxel_index(v001)];
            float s101 = m_host_grid_data[to_voxel_index(v101)];
            float s011 = m_host_grid_data[to_voxel_index(v011)];
            float s111 = m_host_grid_data[to_voxel_index(v111)];

            FloatP o_x = ray.o.x();
            FloatP o_y = ray.o.y();
            FloatP o_z = ray.o.z();
            FloatP d_x = ray.d.x();
            FloatP d_y = ray.d.y();
            FloatP d_z = ray.d.z();

            FloatP a  = s101 - s001;
            FloatP k0 = s000;
            FloatP k1 = s100 - s000;
            FloatP k2 = s010 - s000;
            FloatP k3 = s110 - s010 - k1;
            FloatP k4 = k0 - s001;
            FloatP k5 = k1 - a;
            FloatP k6 = k2 - (s011 - s001);
            FloatP k7 = k3 - (s111 - s011 - a);
            FloatP m0 = o_x * o_y;
            FloatP m1 = d_x * d_y;
            FloatP m2 = dr::fmadd(o_x, d_y, o_y * d_x);
            FloatP m3 = dr::fmadd(k5, o_z, -k1);
            FloatP m4 = dr::fmadd(k6, o_z, -k2);
            FloatP m5 = dr::fmadd(k7, o_z, -k3);

            c0 = dr::fmadd(k4, o_z, -k0) +
                 dr::fmadd(o_x, m3, dr::fmadd(o_y, m4, m0 * m5));
            c1 = dr::fmadd(d_x, m3, d_y * m4) + m2 * m5 +
                 d_z * (k4 + dr::fmadd(k5, o_x, dr::fmadd(k6, o_y, k7 * m0)));
            c2 = dr::fmadd(
                m1, m5,
                d_z * (dr::fmadd(k5, d_x, dr::fmadd(k6, d_y, k7 * m2))));
            c3 = k7 * m1 * d_z;
        }

        auto [hit, t] = sdf_solve_cubic(t_beg, t_end, c3, c2, c1, c0);

        if (m_watertight) {
            FloatP eval_sdf             = -(c3 * t_beg * t_beg * t_beg +
                                c2 * t_beg * t_beg + c1 * t_beg + c0);
            dr::masked(t, eval_sdf < 0) = t_beg;
            hit                         = hit || eval_sdf < 0;
        }

        active = active && bbox_hit && hit && t >= 0.f && t <= ray.maxt;

        return { active, dr::select(active, t, dr::Infinity<FloatP>),
                 Point<FloatP, 2>(0, 0), ((uint32_t) -1), prim_index };
    }

    /* \brief Solve cubic polynomial that gives solution to voxel intersection
     *
     *  M ARMITT, G., K LEER , A., WALD , I., AND F RIEDRICH , H.
     *  2004. Fast and accurate ray-voxel intersection techniques for
     *  iso-surface ray tracing.
     */
    template <typename FloatP>
    MI_INLINE std::tuple<dr::mask_t<FloatP>, FloatP>
    sdf_solve_cubic(FloatP t_beg, FloatP t_end, FloatP c3, FloatP c2, FloatP c1,
                    FloatP c0) const {

        using MaskP = dr::mask_t<FloatP>;

        auto [has_derivative_roots, root_0, root_1] =
            math::solve_quadratic(c3 * 3, c2 * 2, c1);

        auto eval_sdf_t = [&](FloatP t_) -> FloatP {
            return -(c3 * t_ * t_ * t_ + c2 * t_ * t_ + c1 * t_ + c0);
        };

        auto numerical_solve = [&](FloatP t_near, FloatP t_far, FloatP f_near,
                                   FloatP f_far) -> FloatP {
            static constexpr uint32_t num_solve_max_iter = 50;
            static constexpr float num_solve_epsilon     = 0.004f;

            FloatP t   = 0;
            FloatP f_t = 0;

            uint32_t i = 0;
            MaskP done = false;
            while (!dr::all(done)) {
                t   = t_near + (t_far - t_near) * (-f_near / (f_far - f_near));
                f_t = eval_sdf_t(t);
                FloatP condition = f_t * f_near;
                t_far = dr::select(condition <= 0, t, t_far);
                f_far = dr::select(condition <= 0, f_t, f_far);

                t_near = dr::select(condition > 0, t, t_near);
                f_near = dr::select(condition > 0, f_t, f_near);
                done   = (dr::abs(f_t) < num_solve_epsilon) ||
                       (num_solve_max_iter < ++i);
            }

            return t;
        };

        FloatP t_near = t_beg;
        FloatP t_far  = t_end;

        FloatP f_root_0 = eval_sdf_t(root_0);
        FloatP f_root_1 = eval_sdf_t(root_1);

        MaskP root_0_valid = t_near <= root_0 && root_0 <= t_far;

        dr::masked(t_far, has_derivative_roots && root_0_valid &&
                              eval_sdf_t(t_beg) * f_root_0 <= 0) = root_0;
        dr::masked(t_near, has_derivative_roots && root_0_valid &&
                               eval_sdf_t(t_beg) * f_root_0 > 0) = root_0;

        MaskP root_1_valid = t_near <= root_1 && root_1 <= t_far;

        dr::masked(t_far, has_derivative_roots && root_1_valid &&
                              eval_sdf_t(t_near) * f_root_1 <= 0) = root_1;
        dr::masked(t_near, has_derivative_roots && root_1_valid &&
                               eval_sdf_t(t_near) * f_root_1 > 0) = root_1;

        FloatP f_near = eval_sdf_t(t_near);
        FloatP f_far  = eval_sdf_t(t_far);

        MaskP active = f_near * f_far <= 0;

        FloatP t =
            dr::select(active, numerical_solve(t_near, t_far, f_near, f_far),
                       dr::Infinity<Float>);

        return { active, t };
    }

    /* \brief Given an index of the flat SDFGrid data (voxel corners), return
     * the associated voxel position
     */
    MI_INLINE ScalarVector3u to_voxel_position(uint32_t index) const {
        auto shape = m_grid_texture.tensor().shape();
        // Data is packed [Z, Y, X, C]
        uint32_t shape_v[3] = { (uint32_t) shape[2], (uint32_t) shape[1],
                                (uint32_t) shape[0] };

        uint32_t resolution_x = shape_v[2] - 1;
        uint32_t resolution_y = shape_v[1] - 1;

        uint32_t x = index % resolution_x;
        uint32_t y = ((index - x) / resolution_y) % resolution_y;
        uint32_t z =
            (index - x - y * resolution_x) / (resolution_x * resolution_y);

        return { x, y, z };
    }

    /* \brief Given a voxel position, returns the corresponding voxel index
     * relative to the flat array of SDFGrid data. In particular, the returned
     * index maps to the bottom-left corner of the associated voxel
     */
    MI_INLINE ScalarIndex to_voxel_index(const ScalarVector3u &v) const {
        auto shape = m_grid_texture.tensor().shape();
        // Data is packed [Z, Y, X, C]
        uint32_t shape_v[3] = { (uint32_t) shape[2], (uint32_t) shape[1],
                                (uint32_t) shape[0] };

        return v.z() * shape_v[1] * shape_v[0] + v.y() * shape_v[0] + v.x();
    }

    /* \brief Offsets and rescales an point in [0, 1] x [0, 1] x [0, 1] to
     * its corresponding point in the texture. This is usually necessary because
     * dr::Texture objects assume that the value of a pixel is positionned in
     * the middle of the pixel. For a 3D grid, this means that values are not
     * at the corners, but in the middle of the voxels.
     */
    MI_INLINE InputPoint3f rescale_point(const Point3f &p) const {
        Point3f rescaled(
            p[0] * (1 - m_inv_shape[0]) +  (m_inv_shape[0] / 2.f),
            p[1] * (1 - m_inv_shape[1]) +  (m_inv_shape[1] / 2.f),
            p[2] * (1 - m_inv_shape[2]) +  (m_inv_shape[2] / 2.f)
        );

        return InputPoint3f(InputFloat(rescaled.x()), InputFloat(rescaled.y()),
                            InputFloat(rescaled.z()));
    }

    /* \brief Only computes AABBs for voxel that contain a surface in it.
     * Returns a pointer to the array of AABBs, a pointer to an array of voxel
     * indices of the former AABBs and the count of voxels with surface in them.
     *
     * Depending on the variant used, the pointer returned is either host or
     * device visible
     */
    std::tuple<void *, size_t *, void *, size_t *, size_t> build_bboxes() {
        auto shape = m_grid_texture.tensor().shape();
        size_t shape_v[3]  = { shape[2], shape[1], shape[0] };
        float voxel_size[3] = { m_voxel_size.scalar()[0],
                                m_voxel_size.scalar()[1],
                                m_voxel_size.scalar()[2] };
        size_t max_voxel_count =
            (shape[0] - 1) * (shape[1] - 1) * (shape[2] - 1);
        ScalarTransform4f to_world = m_to_world.scalar();

        float *grid = nullptr;

        dr::eval(m_grid_texture.value()); // Make sure the SDF data is evaluated
        if constexpr (dr::is_cuda_v<Float>) {
            grid = (float *) jit_malloc_migrate(
                m_grid_texture.tensor().array().data(), AllocType::Host, false);
            jit_sync_thread();
        } else {
            grid = m_grid_texture.tensor().array().data();
        }

#if defined(MI_ENABLE_CUDA)
        using BoundingBoxType =
            typename std::conditional<dr::is_cuda_v<Float>,
                                      optix::BoundingBox3f,
                                      ScalarBoundingBox3f>::type;
#else
        using BoundingBoxType = ScalarBoundingBox3f;
#endif

        size_t count = 0;
        BoundingBoxType *host_aabbs = (BoundingBoxType *) jit_malloc(
            AllocType::Host, sizeof(BoundingBoxType) * max_voxel_count);
        size_t *host_voxel_indices = (size_t *) jit_malloc(
            AllocType::Host, sizeof(size_t) * max_voxel_count);
        for (size_t z = 0; z < shape[0] - 1; ++z) {
            for (size_t y = 0; y < shape[1] - 1; ++y) {
                for (size_t x = 0; x < shape[2] - 1; ++x) {
                    auto value_index = [&](size_t x_off, size_t y_off, size_t z_off) {
                        return (x + x_off) +
                               (y + y_off) * shape_v[0] +
                               (z + z_off) * shape_v[0] * shape_v[1];
                    };

                    size_t v000 = value_index(0, 0, 0);
                    size_t v100 = value_index(1, 0, 0);
                    size_t v010 = value_index(0, 1, 0);
                    size_t v110 = value_index(1, 1, 0);
                    size_t v001 = value_index(0, 0, 1);
                    size_t v101 = value_index(1, 0, 1);
                    size_t v011 = value_index(0, 1, 1);
                    size_t v111 = value_index(1, 1, 1);

                    float f000 = grid[v000];
                    float f100 = grid[v100];
                    float f010 = grid[v010];
                    float f110 = grid[v110];
                    float f001 = grid[v001];
                    float f101 = grid[v101];
                    float f011 = grid[v011];
                    float f111 = grid[v111];

                    // No surface within voxel
                    if (f000 > 0 && f100 > 0 && f010 > 0 && f110 > 0 &&
                        f001 > 0 && f101 > 0 && f011 > 0 && f111 > 0)
                        continue;


                    ScalarBoundingBox3f bbox{};
                    auto expand_bbox = [&](size_t x, size_t y, size_t z) {
                        bbox.expand(to_world.transform_affine(
                            ScalarPoint3f(
                                x * voxel_size[2],
                                y * voxel_size[1],
                                z * voxel_size[0])));
                    };

                    expand_bbox(x + 0, y + 0, z + 0);
                    expand_bbox(x + 1, y + 0, z + 0);
                    expand_bbox(x + 0, y + 1, z + 0);
                    expand_bbox(x + 1, y + 1, z + 0);
                    expand_bbox(x + 0, y + 0, z + 1);
                    expand_bbox(x + 1, y + 0, z + 1);
                    expand_bbox(x + 0, y + 1, z + 1);
                    expand_bbox(x + 1, y + 1, z + 1);

                    size_t voxel_idx = x +
                                       y * (shape_v[0] - 1) +
                                       z * (shape_v[0] - 1) * (shape_v[1] - 1);

                    host_voxel_indices[count] = voxel_idx;
                    host_aabbs[count] = BoundingBoxType(bbox);

                    count++;
                }
            }
        }

        void *device_aabbs = nullptr;
        size_t* device_voxel_indices = nullptr;

        if constexpr (dr::is_cuda_v<Float>) {
            device_aabbs =
                jit_malloc(AllocType::Device, sizeof(BoundingBoxType) * count);
            jit_memcpy_async(JitBackend::CUDA, device_aabbs, host_aabbs,
                       sizeof(BoundingBoxType) * count);

            device_voxel_indices = (size_t *) jit_malloc(AllocType::Device,
                                                      sizeof(size_t) * count);
            jit_memcpy_async(JitBackend::CUDA, device_voxel_indices, host_voxel_indices,
                       sizeof(size_t) * count);

            jit_free(grid);
        }

        return { host_aabbs, host_voxel_indices, device_aabbs, device_voxel_indices, count };
    }

    /// Computes the SDF gradient for a given point and its containing voxel
    Vector3f voxel_grad(const Point3f &p, const Point3i &voxel_index) const {
        InputFloat f[6];
        Point3f query;

        Point3f voxel_size = m_voxel_size.value();
        Point3f p000 = Point3f(voxel_index) * voxel_size;

        query = rescale_point(Point3f(p000[0] + voxel_size[0], p[1], p[2]));
        m_grid_texture.eval(query, &f[0]);
        query = rescale_point(Point3f(p000[0], p[1], p[2]));
        m_grid_texture.eval(query, &f[1]);

        query = rescale_point(Point3f(p[0], p000[1] + voxel_size[1], p[2]));
        m_grid_texture.eval(query, &f[2]);
        query = rescale_point(Point3f(p[0], p000[1], p[2]));
        m_grid_texture.eval(query, &f[3]);

        query = rescale_point(Point3f(p[0], p[1], p000[2] + voxel_size[2]));
        m_grid_texture.eval(query, &f[4]);
        query = rescale_point(Point3f(p[0], p[1], p000[2] ));
        m_grid_texture.eval(query, &f[5]);

        Float dx = f[0] - f[1]; // f(1, y, z) - f(0, y, z)
        Float dy = f[2] - f[3]; // f(x, 1, z) - f(x, 0, z)
        Float dz = f[4] - f[5]; // f(x, y, 1) - f(x, y, 0)

        return Vector3f(dx, dy, dz);
    }

    Vector3f sdf_grad(const Point3f &p) const {
        auto shape = m_grid_texture.tensor().shape();
        Vector3f resolution = Vector3f(shape[2] - 1, shape[1] - 1, shape[0] - 1);
        Point3i min_voxel_index(p * resolution);

        return voxel_grad(p, min_voxel_index);
    }

#if defined(MI_ENABLE_CUDA)
    static constexpr uint32_t optix_geometry_flags[1] = {
        OPTIX_GEOMETRY_FLAG_NONE
    };
#endif

    enum NormalMethod {
        Analytic,
        Smooth,
    };

    /// SDF data
    InputTexture3f m_grid_texture;
    /// Inverse resolution (1 / tensor_shape)
    Vector3f m_inv_shape;
    /// Local voxel sizes (1 / (tensor_shape - 1))
    field<Vector<InputFloat, 3>> m_voxel_size;

    // Weak pointer to underlying grid texture data. Only used for llvm/scalar
    // variants. We store this because during raytracing, we don't want to call
    // Texture3f::tensor().data() which internally calls jit_var_ptr and is
    // guarded by a global state lock
    float *m_host_grid_data = nullptr;

    // Host-visible non-empty bounding boxes and corresponding indices
    // Also used when targeting CUDA variants, as we upload asynchronously
    // to GPU
    void *m_host_bboxes = nullptr;
    size_t *m_host_voxel_indices = nullptr;

    // Device-visible bounding boxes
    void *m_device_bboxes = nullptr;
    size_t *m_device_voxel_indices = nullptr;

    bool m_watertight;
    size_t m_filled_voxel_count = 0;
    NormalMethod m_normal_method;
};

MI_IMPLEMENT_CLASS_VARIANT(SDFGrid, Shape)
MI_EXPORT_PLUGIN(SDFGrid, "SDFGrid intersection primitive");
NAMESPACE_END(mitsuba)
