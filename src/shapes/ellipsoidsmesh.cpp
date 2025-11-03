#include <mitsuba/core/fwd.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/render/mesh.h>

#include "ellipsoids.h"

#if defined(MI_ENABLE_EMBREE)
    #include <embree3/rtcore.h>
#endif

NAMESPACE_BEGIN(mitsuba)

/// Forward declaration of template shell mesh data buffers
extern std::vector<dr::Array<float, 3>>    box_vertices, uv_sphere_72_vertices, ico_sphere_20_vertices;
extern std::vector<dr::Array<uint32_t, 3>> box_faces,    uv_sphere_72_faces,    ico_sphere_20_faces;

/**!

.. _shape-EllipsoidsMesh:

Mesh ellipsoids (:monosp:`ellipsoidsmesh`)
------------------------------------------

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
   - If True, use adaptive extent values based on the `opacities` attribute of the volumetric primitives. (Default: False)
   - |readonly|

 * - shell
   - |string| or |mesh|
   - Specifies the shell type. Could be one of :monosp:`box`, :monosp:`ico_sphere`,
     or :monosp:`uv_sphere`, as well as a custom child mesh object. (Default: ``ico_sphere``)

 * - to_world
   - |transform|
   - Specifies an optional linear object-to-world transformation to apply to all ellipsoids.
   - |exposed|, |differentiable|, |discontinuous|

 * - (Nested plugin)
   - |tensor|
   - Specifies arbitrary ellipsoids attribute as a tensor of shape (N, D) with D
     the dimensionality of the attribute. For instance this can be used to define
     an opacity value for each ellipsoids, or a set of spherical harmonic coefficients
     as used in the :monosp:`volprim_rf_basic` integrator.

This shape plugin defines a point cloud of anisotropic ellipsoid primitives
given centers, scales, and quaternions, using a mesh-based representation with
backface culling. This plugin is designed to leverage hardware acceleration for
ray-triangle intersections, providing a performance advantage over analytical
ellipsoid representations.

This shape also exposes an `extent` parameter, it acts as an extra scaling
factor for the ellipsoids' scales. Typically, this is used to define the support
of a kernel function defined within the ellipsoid. For example, the
`scale` parmaters of the ellipsoid will define the variances of a gaussian and
the `extent` will multiple those value to define the effictive radius of the
ellipsoid. When `extent_adaptive_clamping` is enabled, the extent is
additionally multiplied by an opacity-dependent factor::math:`\sqrt{2 * \log(opacity / 0.01)} / 3`

This shape is designed for use with volumetric primitive integrators, as
detailed in :cite:`Condor2024Gaussians`.

.. tabs::
    .. code-tab:: xml
        :name: sphere

        <shape type="ellipsoidsmesh">
            <string name="filename" value="my_primitives.ply"/>
        </shape>

    .. code-tab:: python

        'primitives': {
            'type': 'ellipsoidsmesh',
            'filename': 'my_primitives.ply'
        }
 */

template <typename Float, typename Spectrum>
class EllipsoidsMesh final : public Mesh<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Mesh, initialize, m_vertex_count, m_face_count, m_faces, mark_dirty,
                   m_vertex_positions, m_bbox, recompute_bbox, get_children_string, m_shape_type)
    MI_IMPORT_TYPES(Shape)

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using typename Base::InputFloat;
    using typename Base::InputPoint3f;
    using FloatStorage = DynamicBuffer<dr::replace_scalar_t<Float, InputFloat>>;
    using IndexStorage = DynamicBuffer<dr::replace_scalar_t<Float, ScalarIndex>>;
    using ArrayXf      = dr::DynamicArray<Float>;

#if defined(MI_ENABLE_LLVM) && !defined(MI_ENABLE_EMBREE)
    using Base::m_vertex_positions_ptr;
    using Base::m_faces_ptr;
#endif

    EllipsoidsMesh(const Properties &props) : Base(props) {
        m_shape_type = ShapeType::EllipsoidsMesh;

        Timer timer;

        m_ellipsoids = EllipsoidsData<Float, Spectrum>(props);

        std::string_view shell_type = "default";
        if (props.has_property("shell")) {
            if (props.type("shell") == Properties::Type::String) {
                shell_type = props.get<std::string_view>("shell");
                if (shell_type != "box" && shell_type != "default" && shell_type != "ico_sphere" && shell_type != "uv_sphere")
                    Throw("Shell type '%s' is not supported. Should be one of: [\"default\", \"box\", \"ico_sphere\", \"uv_sphere\"]", shell_type);
            } else {
                shell_type = "mesh";
            }
        }

        if (shell_type == "box") {
            Log(Debug, "Load box shell template (12 triangles)");
            m_shell_vertices = box_vertices;
            m_shell_faces    = box_faces;
        } else if (shell_type == "default" || shell_type == "ico_sphere") {
            Log(Debug, "Load default ICO sphere shell template (20 triangles)");
            m_shell_vertices = ico_sphere_20_vertices;
            m_shell_faces    = ico_sphere_20_faces;
        } else if (shell_type == "uv_sphere") {
            Log(Debug, "Load default UV sphere shell template (72 triangles)");
            m_shell_vertices = uv_sphere_72_vertices;
            m_shell_faces    = uv_sphere_72_faces;
        } else {
            Log(Debug, "Load shell template from nested mesh object.");
            ref<Base> mesh(dynamic_cast<Base *>(props.get<ref<Object>>("shell").get()));

            struct MeshDataRetriever : public TraversalCallback {
                void put_object(std::string_view, Object *, uint32_t) override {}
                void put_value(std::string_view name, void *val, uint32_t, const std::type_info &) override {
                    if (name == "vertex_positions")
                        vertex_positions = *((FloatStorage *) val);
                    else if (name == "faces")
                        faces = *((IndexStorage *) val);
                };
                FloatStorage vertex_positions;
                IndexStorage faces;
            };

            MeshDataRetriever retriever = MeshDataRetriever();
            mesh->traverse((TraversalCallback *) &retriever);

            auto&& vertex_positions = dr::migrate(retriever.vertex_positions, AllocType::Host);
            auto&& faces            = dr::migrate(retriever.faces,            AllocType::Host);

            if constexpr (dr::is_jit_v<Float>)
                dr::sync_thread();

            const InputFloat *vertex_positions_ptr = vertex_positions.data();
            const ScalarIndex *faces_ptr           = faces.data();

            m_shell_vertices.clear();
            for (size_t i = 0; i < dr::width(vertex_positions) / 3; ++i) {
                m_shell_vertices.push_back({
                    vertex_positions_ptr[3 * i + 0],
                    vertex_positions_ptr[3 * i + 1],
                    vertex_positions_ptr[3 * i + 2],
                });
            }

            m_shell_faces.clear();
            for (size_t i = 0; i < dr::width(faces) / 3; ++i) {
                m_shell_faces.push_back({
                    faces_ptr[3 * i + 0],
                    faces_ptr[3 * i + 1],
                    faces_ptr[3 * i + 2],
                });
            }
        }

        // Scale vertex positions of shell to ensure that it full encapsulates the unit sphere
        {
            InputFloat scaling = dr::Largest<InputFloat>;
            for (size_t i = 0; i < m_shell_faces.size(); ++i) {
                InputPoint3f v0 = m_shell_vertices[m_shell_faces[i][0]];
                InputPoint3f v1 = m_shell_vertices[m_shell_faces[i][1]];
                InputPoint3f v2 = m_shell_vertices[m_shell_faces[i][2]];

                // Ensures that all vertices are outside of unit sphere
                scaling = std::min(scaling, dr::norm(v0));
                scaling = std::min(scaling, dr::norm(v1));
                scaling = std::min(scaling, dr::norm(v2));

                // Ensures that face center is outside of unit sphere
                scaling = std::min(scaling, dr::norm((v0 + v1 + v2) / 3.f));
            }

            for (size_t i = 0; i < m_shell_vertices.size(); ++i)
                m_shell_vertices[i] /= scaling;
        }

        Log(Debug, "Template mesh shell contains %zu faces and %zu vertices", m_shell_faces.size(), m_shell_vertices.size());

        recompute_mesh();

        ScalarSize shell_count_bytes = 15 * sizeof(InputFloat) + 24 * sizeof(InputFloat) + 12 * sizeof(ScalarIndex);
        Log(Debug, "Initialize %i mesh ellipsoid shells (%s in %s)",
            m_ellipsoids.count(),
            util::mem_string(m_ellipsoids.count() * shell_count_bytes),
            util::time_string((float) timer.value()));
    }

    void traverse(TraversalCallback *cb) override {
        m_ellipsoids.traverse(cb);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        m_ellipsoids.parameters_changed();

        if (keys.empty() || string::contains(keys, "data")) {
            recompute_mesh();
        }

        // Don't call Mesh::parameters_changed() as it will initialize
        // data-structures that are not needed for this plugin!
    }

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

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     uint32_t recursion_depth,
                                                     Mask active) const override {
        auto si = Base::compute_surface_interaction(ray, pi, ray_flags, recursion_depth, active);

        // Divide by the number of faces per Gaussians
        si.prim_index /= (uint32_t) m_shell_faces.size();

        return si;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "EllipsoidsMesh[" << std::endl
            << "  bbox = " << string::indent(m_bbox) << "," << std::endl
            << "  ellipsoid_count = " << m_ellipsoids.count() << "," << std::endl
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

    MI_DECLARE_CLASS(EllipsoidsMesh)

private:
    void recompute_mesh() {
        if constexpr (dr::is_jit_v<Float>) {
            UInt32 idx = dr::arange<UInt32>(m_ellipsoids.count());

            auto ellipsoid = m_ellipsoids.template get_ellipsoid<Float>(idx);
            auto rot = dr::quat_to_matrix<Matrix3f>(ellipsoid.quat);

            const AffineTransform4f& to_world = AffineTransform4f::translate(ellipsoid.center) *
                                          AffineTransform4f(rot) *
                                          AffineTransform4f::scale(ellipsoid.scale) *
                                          AffineTransform4f::scale(m_ellipsoids.template extents<Float>(idx));

            int nb_vertices = (int) m_shell_vertices.size();
            int nb_faces    = (int) m_shell_faces.size();

            uint32_t vertex_count = (uint32_t) (m_ellipsoids.count() * nb_vertices);
            uint32_t face_count   = (uint32_t) (m_ellipsoids.count() * nb_faces);

            bool initialized = (vertex_count != m_vertex_count) || (face_count != m_face_count);

            m_vertex_count = vertex_count;
            m_face_count   = face_count;

            m_vertex_positions = dr::empty<FloatStorage>(3 * m_vertex_count);
            m_faces            = dr::empty<IndexStorage>(3 * m_face_count);

            for (int i = 0; i < nb_vertices; ++i) {
                Point3f v = to_world * Point3f(m_shell_vertices[i]);
                // Convert to 32-bit precision
                using JitInputPoint3f = Point<dr::replace_scalar_t<Float, InputFloat>, 3>;
                dr::scatter(m_vertex_positions, JitInputPoint3f(v), idx * nb_vertices + i);
            }

            UInt32 offset = idx * uint32_t(nb_vertices);
            for (int i = 0; i < nb_faces; ++i) {
                Vector3u face = m_shell_faces[i];
                dr::scatter(m_faces, face + offset, idx * nb_faces + i);
            }

            // Don't call Mesh::initialize() as it will initialize data-structure
            // that are not needed for this plugin!
#if defined(MI_ENABLE_LLVM) && !defined(MI_ENABLE_EMBREE)
            m_vertex_positions_ptr = m_vertex_positions.data();
            m_faces_ptr = m_faces.data();
#endif
            if (initialized)
                recompute_bbox();
            mark_dirty();
        } else {
            size_t nb_vertices = m_shell_vertices.size();
            size_t nb_faces    = m_shell_faces.size();

            uint32_t vertex_count = (uint32_t) (m_ellipsoids.count() * nb_vertices);
            uint32_t face_count   = (uint32_t) (m_ellipsoids.count() * nb_faces);

            bool initialized = (vertex_count != m_vertex_count) || (face_count != m_face_count);

            m_vertex_count = vertex_count;
            m_face_count   = face_count;

            m_vertex_positions = dr::empty<FloatStorage>(3 * m_vertex_count);
            m_faces            = dr::empty<IndexStorage>(3 * m_face_count);

            for (size_t i = 0; i < m_ellipsoids.count(); ++i) {
                auto ellipsoid = m_ellipsoids.template get_ellipsoid<Float>(i);
                auto rot = dr::quat_to_matrix<Matrix3f>(ellipsoid.quat);

                const ScalarAffineTransform4f &to_world =
                    ScalarAffineTransform4f::translate(ellipsoid.center) *
                    ScalarAffineTransform4f(rot) *
                    ScalarAffineTransform4f::scale(ellipsoid.scale) *
                    ScalarAffineTransform4f::scale(m_ellipsoids.template extents<Float>(i));

                for (size_t j = 0; j < nb_vertices; ++j) {
                    Point3f v = to_world * Point3f(m_shell_vertices[j]);
                    for (size_t k = 0; k < 3; ++k)
                        m_vertex_positions[i * nb_vertices * 3 + j * 3 + k] = float(v[k]);
                }

                UInt32 offset = (uint32_t) i * uint32_t(nb_vertices);
                for (size_t j = 0; j < nb_faces; ++j) {
                    Vector3u face = m_shell_faces[j];
                    for (size_t k = 0; k < 3; ++k)
                        m_faces[i * nb_faces * 3 + j * 3 + k] = face[k] + offset;
                }

                // Don't call Mesh::initialize() as it will initialize data-structure
                // that are not needed for this plugin!
#if defined(MI_ENABLE_LLVM) && !defined(MI_ENABLE_EMBREE)
                m_vertex_positions_ptr = m_vertex_positions.data();
                m_faces_ptr = m_faces.data();
#endif
                if (initialized)
                    recompute_bbox();
                mark_dirty();
            }
        }
    }

#if defined(MI_ENABLE_EMBREE)
    template <size_t N, typename RTCRay_, typename RTCHit_>
    static void embree_filter_backface_culling_packet(
        const RTCFilterFunctionNArguments *args, RTCRay_ *ray, RTCHit_ *hit) {
        using FloatP    = dr::Packet<float, N>;
        using Int32P    = dr::Packet<int, N>;
        using Vector3fP = dr::Array<FloatP, 3>;

        Vector3fP d;
        d.x() = dr::load_aligned<FloatP>(ray->dir_x);
        d.y() = dr::load_aligned<FloatP>(ray->dir_y);
        d.z() = dr::load_aligned<FloatP>(ray->dir_z);

        Vector3fP n;
        n.x() = dr::load_aligned<FloatP>(hit->Ng_x);
        n.y() = dr::load_aligned<FloatP>(hit->Ng_y);
        n.z() = dr::load_aligned<FloatP>(hit->Ng_z);

        Int32P valid = dr::load_aligned<Int32P>(args->valid);
        valid = dr::select(dr::dot(d, n) > 0.0f, 0, valid);
        dr::store_aligned(args->valid, valid);
    }

    static void embree_filter_backface_culling(const RTCFilterFunctionNArguments *args) {
        switch (args->N) {
            case 1:
                {
                    const RTCRay *ray = (RTCRay *)args->ray;
                    RTCHit *hit = (RTCHit *)args->hit;

                    auto d = dr::Array<float, 3>(ray->dir_x, ray->dir_y, ray->dir_z);
                    auto n = dr::Array<float, 3>(hit->Ng_x, hit->Ng_y, hit->Ng_z);

                    // Always ignore back-facing intersections
                    if (dr::dot(d, n) > 0.0f) {
                        *args->valid = 0;
                    }
                }
                break;

            case 4:
                embree_filter_backface_culling_packet<4>(
                    args,
                    (RTCRay4 *) args->ray,
                    (RTCHit4 *) args->hit
                );
                break;

            case 8:
                embree_filter_backface_culling_packet<8>(
                    args,
                    (RTCRay8 *) args->ray,
                    (RTCHit8 *) args->hit
                );
                break;

            case 16:
                embree_filter_backface_culling_packet<16>(
                    args,
                    (RTCRay16 *) args->ray,
                    (RTCHit16 *) args->hit
                );
                break;

            default:
                Throw("embree_filter_backface_culling(): unsupported packet size!");
        }
    }

    RTCGeometry embree_geometry(RTCDevice device) override {
        RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);

        rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3,
                                m_vertex_positions.data(), 0, 3 * sizeof(InputFloat),
                                m_vertex_count);
        rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3,
                                m_faces.data(), 0, 3 * sizeof(ScalarIndex),
                                m_face_count);

        rtcSetGeometryIntersectFilterFunction(geom, embree_filter_backface_culling);
        rtcSetGeometryOccludedFilterFunction(geom,  embree_filter_backface_culling);

        rtcCommitGeometry(geom);
        return geom;
    }
#endif

private:
    /// Object holding the ellipsoid data and attributes
    EllipsoidsData<Float, Spectrum> m_ellipsoids;

    // Template mesh for the shell
    std::vector<dr::Array<float, 3>> m_shell_vertices;
    std::vector<dr::Array<uint32_t, 3>> m_shell_faces;

    // Store constructor properties to expand to other plugin in LLVM mode.
    Properties m_props;
};

MI_EXPORT_PLUGIN(EllipsoidsMesh)

// =============================================================
// Hardcoded mesh shell template data
// =============================================================

std::vector<dr::Array<float, 3>> box_vertices = {
    {  1, -1, -1 }, {  1, -1,  1 }, { -1, -1,  1 }, { -1, -1, -1 },
    {  1,  1, -1 }, { -1,  1, -1 }, { -1,  1,  1 }, {  1,  1,  1 },
    {  1, -1, -1 }, {  1,  1, -1 }, {  1,  1,  1 }, {  1, -1,  1 },
    {  1, -1,  1 }, {  1,  1,  1 }, { -1,  1,  1 }, { -1, -1,  1 },
    { -1, -1,  1 }, { -1,  1,  1 }, { -1,  1, -1 }, { -1, -1, -1 },
    {  1,  1, -1 }, {  1, -1, -1 }, { -1, -1, -1 }, { -1,  1, -1 }
};

std::vector<dr::Array<uint32_t, 3>> box_faces = {
    {  0,  1,  2 }, {  3,  0,  2 }, {  4,  5,  6 }, {  7,  4,  6 },
    {  8,  9, 10 }, { 11,  8, 10 }, { 12, 13, 14 }, { 15, 12, 14 },
    { 16, 17, 18 }, { 19, 16, 18 }, { 20, 21, 22 }, { 23, 20, 22 }
};

std::vector<dr::Array<float, 3>> uv_sphere_72_vertices = {
    { 0.377821f, 0.809017f, -0.450270f },   { 0.611327f, 0.309017f, -0.728552f },   { 0.611327f, -0.309017f, -0.728552f },
    { 0.377821f, -0.809017f, -0.450270f },  { 0.578855f, 0.809017f, -0.102068f },   { 0.936608f, 0.309017f, -0.165149f },
    { 0.936608f, -0.309017f, -0.165149f },  { 0.578855f, -0.809017f, -0.102068f },  { 0.509037f, 0.809017f, 0.293893f },
    { 0.823639f, 0.309017f, 0.475528f },    { 0.823639f, -0.309017f, 0.475528f },   { 0.509037f, -0.809017f, 0.293893f },
    { 0.201034f, 0.809017f, 0.552337f },    { 0.325280f, 0.309017f, 0.893701f },    { 0.325280f, -0.309017f, 0.893701f },
    { 0.201034f, -0.809017f, 0.552337f },   { -0.201034f, 0.809017f, 0.552337f },   { -0.325280f, 0.309017f, 0.893701f },
    { -0.325280f, -0.309017f, 0.893701f },  { -0.201034f, -0.809017f, 0.552337f },  { 0.000000f, 1.000000f, 0.000000f },
    { -0.509037f, 0.809017f, 0.293893f },   { -0.823639f, 0.309017f, 0.475528f },   { -0.823639f, -0.309017f, 0.475528f },
    { -0.509037f, -0.809017f, 0.293893f },  { -0.578855f, 0.809017f, -0.102068f },  { -0.936608f, 0.309017f, -0.165149f },
    { -0.936608f, -0.309017f, -0.165149f }, { -0.578855f, -0.809017f, -0.102068f }, { -0.377821f, 0.809017f, -0.450269f },
    { -0.611327f, 0.309017f, -0.728551f },  { -0.611327f, -0.309017f, -0.728551f }, { -0.377821f, -0.809017f, -0.450269f },
    { -0.000000f, 0.809017f, -0.587785f },  { -0.000000f, 0.309017f, -0.951056f },  { -0.000000f, -0.309017f, -0.951056f },
    { -0.000000f, -0.809017f, -0.587785f }, { 0.000000f, -1.000000f, 0.000000f }
};

std::vector<dr::Array<uint32_t, 3>> uv_sphere_72_faces = {
    { 35, 3, 36 },  { 34, 0, 1 },   { 37, 36, 3 },
    { 34, 2, 35 },  { 33, 20, 0 },  { 37, 3, 7 },
    { 1, 6, 2 },    { 0, 20, 4 },   { 2, 7, 3 },
    { 1, 4, 5 },    { 37, 7, 11 },  { 5, 10, 6 },
    { 4, 20, 8 },   { 6, 11, 7 },   { 4, 9, 5 },
    { 37, 11, 15 }, { 9, 14, 10 },  { 8, 20, 12 },
    { 11, 14, 15 }, { 8, 13, 9 },   { 37, 15, 19 },
    { 13, 18, 14 }, { 12, 20, 16 }, { 14, 19, 15 },
    { 13, 16, 17 }, { 37, 19, 24 }, { 17, 23, 18 },
    { 16, 20, 21 }, { 18, 24, 19 }, { 17, 21, 22 },
    { 37, 24, 28 }, { 22, 27, 23 }, { 21, 20, 25 },
    { 23, 28, 24 }, { 22, 25, 26 }, { 37, 28, 32 },
    { 26, 31, 27 }, { 25, 20, 29 }, { 27, 32, 28 },
    { 26, 29, 30 }, { 37, 32, 36 }, { 30, 35, 31 },
    { 29, 20, 33 }, { 32, 35, 36 }, { 29, 34, 30 },
    { 35, 2, 3 },   { 34, 33, 0 },  { 34, 1, 2 },
    { 1, 5, 6 },    { 2, 6, 7 },    { 1, 0, 4 },
    { 5, 9, 10 },   { 6, 10, 11 },  { 4, 8, 9 },
    { 9, 13, 14 },  { 11, 10, 14 }, { 8, 12, 13 },
    { 13, 17, 18 }, { 14, 18, 19 }, { 13, 12, 16 },
    { 17, 22, 23 }, { 18, 23, 24 }, { 17, 16, 21 },
    { 22, 26, 27 }, { 23, 27, 28 }, { 22, 21, 25 },
    { 26, 30, 31 }, { 27, 31, 32 }, { 26, 25, 29 },
    { 30, 34, 35 }, { 32, 31, 35 }, { 29, 33, 34 },
};

std::vector<dr::Array<float, 3>> ico_sphere_20_vertices = {
    { 0.000000f, -1.000000f, 0.000000f },   { 0.723600f, -0.447215f, 0.525720f },   { -0.276385f, -0.447215f, 0.850640f },
    { -0.894425f, -0.447215f, 0.000000f },  { -0.276385f, -0.447215f, -0.85064f },  { 0.723600f, -0.447215f, -0.525720f },
    { 0.276385f, 0.447215f, 0.850640f },    { -0.723600f, 0.447215f, 0.525720f },   { -0.723600f, 0.447215f, -0.525720f },
    { 0.276385f, 0.447215f, -0.850640f },   { 0.894425f, 0.447215f, 0.000000f },    { 0.000000f, 1.000000f, 0.000000f }
};

std::vector<dr::Array<uint32_t, 3>> ico_sphere_20_faces = {
    { 0, 1, 2 }, { 1, 0, 5 }, { 0, 2, 3 }, { 0, 3, 4 }, { 0, 4, 5 },
    { 1, 5, 10 }, { 2, 1, 6 }, { 3, 2, 7 }, { 4, 3, 8 }, { 5, 4, 9 },
    { 1, 10, 6 }, { 2, 6, 7 }, { 3, 7, 8 }, { 4, 8, 9 }, { 5, 9, 10 },
    { 6, 10, 11 }, { 7, 6, 11 }, { 8, 7, 11 }, { 9, 8, 11 }, { 10, 9, 11 }
};

//
// =============================================================

NAMESPACE_END(mitsuba)
