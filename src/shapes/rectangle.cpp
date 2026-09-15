#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/mesh.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-rectangle:

Rectangle (:monosp:`rectangle`)
-------------------------------------------------

.. pluginparameters::

 * - flip_normals
   - |bool|
   - Is the rectangle inverted, i.e. should the normal vectors be flipped? (Default: |false|)

 * - to_world
   - |transform|
   - Specifies a linear object-to-world transformation. (Default: none (i.e. object space = world space))
   - |exposed|, |differentiable|, |discontinuous|

 * - silhouette_sampling_weight
   - |float|
   - Weight associated with this shape when sampling silhoeuttes in the scene. (Default: 1)
   - |exposed|

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/shape_rectangle.jpg
   :caption: Basic example
.. subfigure:: ../../resources/data/docs/images/render/shape_rectangle_parameterization.jpg
   :caption: A textured rectangle with the default parameterization
.. subfigend::
   :label: fig-rectangle

This shape plugin describes a simple rectangular shape primitive.
It is mainly provided as a convenience for those cases when creating and
loading an external mesh with two triangles is simply too tedious, e.g.
when an area light source or a simple ground plane are needed.
By default, the rectangle covers the XY-range :math:`[-1,1]\times[-1,1]`
and has a surface normal that points into the positive Z-direction.
To change the rectangle scale, rotation, or translation, use the
:monosp:`to_world` parameter.


The following XML snippet showcases a simple example of a textured rectangle:

.. tabs::
    .. code-tab:: xml
        :name: rectangle

        <shape type="rectangle">
            <bsdf type="diffuse">
                <texture name="reflectance" type="checkerboard">
                    <transform name="to_uv">
                        <scale x="5" y="5" />
                    </transform>
                </texture>
            </bsdf>
        </shape>

    .. code-tab:: python

        'type': 'rectangle',
        'material': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'checkerboard',
                'to_uv': mi.ScalarAffineTransform4f().scale([5, 5, 1])
            }
        }

 */

template <typename Float, typename Spectrum>
class Rectangle final : public Mesh<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Mesh, m_to_world,
                   m_discontinuity_types, m_shape_type, m_flip_normals,
                   m_packed_faces, m_packed_vertices, m_vertex_count,
                   m_face_count, m_position_count, m_normal_count, m_layout,
                   m_built, transform, pack, needs_tangents, drop_views,
                   get_children_string)
    MI_IMPORT_TYPES()

    using typename Base::ScalarIndex;
    using typename Base::FloatBuffer;
    using typename Base::IndexBuffer;

    Rectangle(const Properties &props) : Base(props) {
        m_shape_type = ShapeType::Rectangle;

        // This shape rebuilds its records whenever 'to_world' changes, so it
        // holds on to the flip rather than consuming it during construction
        m_flipped = m_flip_normals;
        m_flip_normals = false;

        initialize();
    }

    void initialize() override {
        // The unit rectangle is symmetric in z, so mirroring that axis turns
        // the surface around without moving it
        AffineTransform4f to_world = m_to_world.value();
        if (m_flipped)
            to_world = to_world *
                AffineTransform4f::scale(Vector3f(1.f, 1.f, -1.f));

        // Compute shading frame
        Normal3f n     = dr::normalize(to_world * Normal3f(0.f, 0.f, 1.f)),
                 dp_du = to_world * Vector3f(2.f, 0.f, 0.f),
                 dp_dv = to_world * Vector3f(0.f, 2.f, 0.f);

        m_frame = Frame3f(dp_du, dp_dv, n);
        m_inv_surface_area = dr::rcp(surface_area());
        dr::make_opaque(m_frame, m_inv_surface_area);

        // transform() below reverses the winding of mirrored geometry, which
        // permutes the vertices that the barycentric coordinates refer to.
        // The 'm_flipped' mirror above contributes another sign change.
        m_reversed =
            (dr::det(ScalarMatrix3f(m_to_world.scalar().matrix)) < 0.f) !=
            m_flipped;

        // (Re-)create the unit rectangle records and map them to world
        // space. This runs again on every 'to_world' write; in
        // differentiable variants, transform() keeps the packed state
        // attached to a gradient-enabled 'to_world'.
        static const uint32_t face_records[8] = { 1, 2, 0, 0,  1, 3, 2, 0 };
        static const float vertex_records[32] = {
            // position  | normal  | uv
            -1, -1, 0,     0, 0, 1,  0, 0,
             1, -1, 0,     0, 0, 1,  1, 0,
            -1,  1, 0,     0, 0, 1,  0, 1,
             1,  1, 0,     0, 0, 1,  1, 1,
        };

        m_packed_faces    = dr::load<IndexBuffer>(face_records, 8);
        m_packed_vertices = dr::load<FloatBuffer>(vertex_records, 32);
        m_vertex_count    = m_position_count = m_normal_count = 4;
        m_face_count      = 2;
        m_layout          = make_layout(true, true);
        m_built           = true;

        // Encode tangent frames when the attached material consumes them
        if (needs_tangents())
            pack(/* regenerate_normals */ false);
        transform(to_world);
        // This shape does not expose the mesh fields
        drop_views();
    }

    // =============================================================
    // Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        PositionSample3f ps = dr::zeros<PositionSample3f>();
        Point3f local(dr::fmadd(sample.x(), 2.f, -1.f),
                      dr::fmadd(sample.y(), 2.f, -1.f), 0.f);
        ps.p     = m_to_world.value() * local;
        ps.n     = m_frame.n;
        ps.p_err = m_to_world.value().position_error(local, ps.n);
        ps.pdf   = m_inv_surface_area;
        ps.uv    = sample;
        ps.time  = time;
        ps.delta = false;

        return ps;
    }

    Float pdf_position(const PositionSample3f & /*ps*/, Mask active) const override {
        MI_MASK_ARGUMENT(active);
        return m_inv_surface_area;
    }

    Float surface_area() const override {
        return dr::norm(dr::cross(m_frame.s, m_frame.t));
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox;
        ScalarAffineTransform4f to_world = m_to_world.scalar();

        bbox.expand(to_world * ScalarPoint3f(-1.f, -1.f, 0.f));
        bbox.expand(to_world * ScalarPoint3f(-1.f,  1.f, 0.f));
        bbox.expand(to_world * ScalarPoint3f( 1.f, -1.f, 0.f));
        bbox.expand(to_world * ScalarPoint3f( 1.f,  1.f, 0.f));

        return bbox;
    }

    void traverse(TraversalCallback *cb) override {
        Shape<Float, Spectrum>::traverse(cb); // mesh attributes not exposed
        cb->put("to_world", m_to_world, ParamFlags::Differentiable | ParamFlags::Discontinuous);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "to_world")) {
            // LLVM/Embree reads these parameters from host memory; wait for
            // in-flight kernels before overwriting (GPU uploads are queue-ordered).
            if constexpr (dr::is_llvm_v<Float>)
                dr::sync_thread();

            m_to_world = m_to_world.value().update();
            initialize();
        }
        Base::parameters_changed(keys);
    }

    SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                               uint32_t ray_flags,
                                               Mask active) const override {
        MI_MASK_ARGUMENT(active);

        bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape);
        AffineTransform4f to_world = detach_shape ? dr::detach(m_to_world.value())
                                                  : m_to_world.value();
        Frame3f frame = detach_shape ? dr::detach(m_frame) : m_frame;

        Point3f local(dr::fmadd(uv.x(), 2.f, - 1.f),
                      dr::fmadd(uv.y(), 2.f, - 1.f), 0.f);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.t     = dr::select(active, 0.f, dr::Infinity<Float>);
        si.p     = to_world * local;
        si.n     = frame.n;
        si.p_err = to_world.position_error(local, si.n);
        si.uv    = uv;
        si.dp_du = frame.s;
        si.dp_dv = frame.t;
        si.sh_frame.n = si.n;
        si.sh_frame.s = si.dp_du;
        si.initialize_sh_frame();
        si.shape = this;
        dr::masked(si.shape, !active) = nullptr;

        return si;
    }

    bool parameters_grad_enabled() const override {
        return dr::grad_enabled(m_frame) ||
               dr::grad_enabled(m_to_world.value());
    }

    // =============================================================

    // =============================================================
    // Silhouette sampling routines and other utilities
    // =============================================================

    SilhouetteSample3f sample_silhouette(const Point3f &sample,
                                         uint32_t flags,
                                         Mask active) const override {
        MI_MASK_ARGUMENT(active);

        if (!has_flag(flags, DiscontinuityFlags::PerimeterType))
            return dr::zeros<SilhouetteSample3f>();

        SilhouetteSample3f ss = dr::zeros<SilhouetteSample3f>();
        const AffineTransform4f &to_world = m_to_world.value();

        // Sample a point on one of the edges
        Mask range = false;
        Vector2f edge_dir = dr::zeros<Vector2f>();

        // Use sample.x() to determine a point on the rectangle edges:
        // clockwise rotation starting at bottom left corner
        range = (sample.x() < 0.25f);
        dr::masked(edge_dir, range) = Vector2f(0.f, 1.f);
        dr::masked(ss.uv, range) =
            dr::fmadd(edge_dir * 4.f, sample.x() - 0.00f, Point2f(0.f, 0.f));

        range = (0.25f <= sample.x() && sample.x() < 0.50f);
        dr::masked(edge_dir, range) = Vector2f(1.f, 0.f);
        dr::masked(ss.uv, range) =
            dr::fmadd(edge_dir * 4.f, sample.x() - 0.25f, Point2f(0.f, 1.f));

        range = (0.50f <= sample.x() && sample.x() < 0.75f);
        dr::masked(edge_dir, range) = Vector2f(0.f, -1.f);
        dr::masked(ss.uv, range) =
            dr::fmadd(edge_dir * 4.f, sample.x() - 0.50f, Point2f(1.f, 1.f));

        range = (0.75f <= sample.x());
        dr::masked(edge_dir, range) = Vector2f(-1.f, 0.f);
        dr::masked(ss.uv, range) =
            dr::fmadd(edge_dir * 4.f, sample.x() - 0.75f, Point2f(1.f, 0.f));

        // Object space spans [-1,1]x[-1,1], UV coordinates span [0,1]x[0,1]
        Vector3f local(dr::fmsub(ss.uv.x(), 2.f, 1.f),
                       dr::fmsub(ss.uv.y(), 2.f, 1.f),
                       0.f);
        ss.p = to_world * Point3f(local);

        // Sample a tangential direction at the point
        ss.d = warp::square_to_uniform_sphere(Point2f(dr::tail<2>(sample)));

        // Fill other fields
        ss.discontinuity_type = (uint32_t) DiscontinuityFlags::PerimeterType;
        ss.flags = flags;

        Vector3f world_edge_dir = to_world *
            Vector3f(edge_dir.x(), edge_dir.y(), 0.f);
        ss.silhouette_d = dr::normalize(world_edge_dir);
        Normal3f frame_n = dr::normalize(dr::cross(ss.d, ss.silhouette_d));

        // Normal direction `ss.n` must point outwards
        Vector3f inward_dir = -local;
        inward_dir = to_world * inward_dir;
        frame_n[dr::dot(inward_dir, frame_n) > 0.f] *= -1.f;
        ss.n = frame_n;

        ss.pdf = dr::rcp(dr::norm(world_edge_dir) * 2.f) * 0.25f;
        ss.pdf *= warp::square_to_uniform_sphere_pdf(ss.d);
        ss.foreshortening = dr::norm(dr::cross(ss.d, ss.silhouette_d));
        ss.shape = this;

        return ss;
    }

    Point3f invert_silhouette_sample(const SilhouetteSample3f &ss,
                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        Mask range = false;
        Float sample_x = dr::zeros<Float>();
        Bool done = false;

        // Clockwise rotation starting at bottom left corner
        range = (ss.uv.x() == 0.f);
        dr::masked(sample_x, range && !done) = ss.uv.y() * 0.25f + 0.00f;
        done |= range;

        range = (ss.uv.y() == 1.f);
        dr::masked(sample_x, range && !done) = ss.uv.x() * 0.25f + 0.25f;
        done |= range;

        range = (ss.uv.x() == 1.f);
        dr::masked(sample_x, range && !done) = (1.f - ss.uv.y()) * 0.25f + 0.50f;
        done |= range;

        range = (ss.uv.y() == 0.f);
        dr::masked(sample_x, range && !done) = (1.f - ss.uv.x()) * 0.25f + 0.75f;

        Point2f sample_yz = warp::uniform_sphere_to_square(ss.d);

        Point3f sample = dr::zeros<Point3f>(dr::width(ss));
        sample.x() = sample_x;
        sample.y() = sample_yz.x();
        sample.z() = sample_yz.y();

        return sample;
    }

    Point3f differential_motion(const SurfaceInteraction3f &si,
                                Mask active) const override {
        MI_MASK_ARGUMENT(active);

        if constexpr (!dr::is_diff_v<Float>) {
            return si.p;
        } else {
            Point2f uv = dr::detach(si.uv);

            Point3f local(dr::fmadd(uv.x(), 2.f, -1.f),
                          dr::fmadd(uv.y(), 2.f, -1.f), 0.f);
            Point3f p_diff = m_to_world.value() * local;

            return dr::replace_grad(si.p, p_diff);
        }
    }

    SilhouetteSample3f primitive_silhouette_projection(
        const Point3f &viewpoint, const SurfaceInteraction3f &si,
        uint32_t flags, Float sample, Mask active) const override {
        MI_MASK_ARGUMENT(active);

        if (!has_flag(flags, DiscontinuityFlags::PerimeterType))
            return dr::zeros<SilhouetteSample3f>();

        SilhouetteSample3f ss = dr::zeros<SilhouetteSample3f>();
        const AffineTransform4f &to_world = m_to_world.value();

        // Project to nearest edge
        Mask top_right_triangle = si.uv.y() > 1 - si.uv.x();
        Mask bottom_left_triangle = !top_right_triangle;
        Mask top_left_triangle = si.uv.y() > si.uv.x();
        Mask bottom_right_triangle = !top_left_triangle;

        Mask bottom_edge = bottom_left_triangle && bottom_right_triangle;
        Mask left_edge = bottom_left_triangle && top_left_triangle;
        Mask top_edge = top_right_triangle && top_left_triangle;
        Mask right_edge = active && !top_edge && !bottom_edge && !left_edge;

        // Uniformly pick a point on the edge
        Point3f local = dr::zeros<Point3f>();
        dr::masked(local, bottom_edge) = Point3f(dr::fmsub(sample, 2.f, 1), -1.f, 0.f);
        dr::masked(local, top_edge) =    Point3f(dr::fmsub(sample, 2.f, 1),  1.f, 0.f);
        dr::masked(local, left_edge) =   Point3f(-1.f, dr::fmsub(sample, 2.f, 1), 0.f);
        dr::masked(local, right_edge) =  Point3f( 1.f, dr::fmsub(sample, 2.f, 1), 0.f);

        // Explicitly write UVs with 0s and 1s to match `invert_silhouette_sample`
        dr::masked(ss.uv, bottom_edge) = Point2f(sample, 0.f);
        dr::masked(ss.uv, top_edge) =    Point2f(sample, 1.f);
        dr::masked(ss.uv, left_edge) =   Point2f(0.f, sample);
        dr::masked(ss.uv, right_edge) =  Point2f(1.f, sample);

        Point2f edge_dir = dr::zeros<Point2f>();
        dr::masked(edge_dir, bottom_edge) = Point2f(1.f, 0.f);
        dr::masked(edge_dir, top_edge) =    Point2f(1.f, 0.f);
        dr::masked(edge_dir, left_edge) =   Point2f(0.f, 1.f);
        dr::masked(edge_dir, right_edge) =  Point2f(0.f, 1.f);

        ss.p = to_world * Point3f(local);
        ss.d            = dr::normalize(ss.p - viewpoint);
        ss.silhouette_d = dr::normalize(to_world *
            Vector3f(edge_dir.x(), edge_dir.y(), 0.f));

        Vector3f frame_t = dr::normalize(viewpoint - ss.p);
        Normal3f frame_n = dr::normalize(dr::cross(frame_t, ss.silhouette_d));
        Vector3f inward_dir = to_world * Vector3f(-local);
        frame_n[dr::dot(inward_dir, frame_n) > 0.f] *= -1.f;
        ss.n = frame_n;

        ss.discontinuity_type = (uint32_t) DiscontinuityFlags::PerimeterType;
        ss.flags = flags;
        ss.shape = this;

        return ss;
    }

    std::tuple<DynamicBuffer<UInt32>, DynamicBuffer<Float>>
    precompute_silhouette(const ScalarPoint3f & /*viewpoint*/) const override {
        DynamicBuffer<UInt32> indices((uint32_t)DiscontinuityFlags::PerimeterType);
        DynamicBuffer<Float> weights(1.f);

        return {indices, weights};
    }

    SilhouetteSample3f
    sample_precomputed_silhouette(const Point3f &viewpoint,
                                  UInt32 /*sample1*/,
                                  Float sample, Mask active) const override {
        MI_MASK_ARGUMENT(active);

        // Pick one of the four edges and rescale the sample to parameterize the
        // position along it
        UInt32 index = dr::minimum(UInt32(sample * 4.f), 3u);
        Float sample_reuse = dr::fmsub(sample, 4.f, Float(index));

        // The projection locates the edge from a UV coordinate on it. The order
        // is left, top, right, bottom, the last two mirroring the first two.
        Mask along_v = (index & 1u) == 0u;
        Point2f uv = dr::select(along_v, Point2f(0.f, 0.5f), Point2f(0.5f, 1.f));

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.uv = dr::select(index >= 2u, 1.f - uv, uv);

        uint32_t flags = (uint32_t) DiscontinuityFlags::PerimeterType;
        SilhouetteSample3f ss = primitive_silhouette_projection(
            viewpoint, si, flags, sample_reuse, active);

        // Each edge is chosen with probability 1/4 and sampled uniformly along
        // its length, which is twice the norm of a transformed basis vector
        const AffineTransform4f &to_world = m_to_world.value();
        Float len = dr::select(along_v,
                               dr::norm(to_world * Vector3f(0.f, 1.f, 0.f)),
                               dr::norm(to_world * Vector3f(1.f, 0.f, 0.f)));
        ss.pdf = 0.125f * dr::rcp(len);

        return ss;
    }

    // =============================================================


    // =============================================================
    // Ray tracing routines
    // =============================================================

    template <typename FloatP, typename Ray3fP>
    std::tuple<dr::mask_t<FloatP>, FloatP, Point<FloatP, 3>>
    intersect_impl(const Ray3fP &ray_, dr::mask_t<FloatP> active) const {
        AffineTransform<Point<FloatP, 4>> to_object;
        if constexpr (!dr::is_jit_v<FloatP>)
            to_object = m_to_world.scalar().inverse();
        else
            to_object = m_to_world.value().inverse();

        Ray3fP ray = to_object * ray_;
        FloatP t   = -ray.o.z() / ray.d.z();
        Point<FloatP, 3> local = ray(t);

        // Is intersection within ray segment and rectangle?
        active = active && t >= 0.f
                        && t <= ray.maxt
                        && dr::abs(local.x()) <= 1.f
                        && dr::abs(local.y()) <= 1.f;

        return { active, t, local };
    }

    template <typename FloatP, typename Ray3fP>
    std::tuple<dr::mask_t<FloatP>, FloatP, Point<FloatP, 2>,
               dr::uint32_array_t<FloatP>, dr::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray,
                                   dr::uint32_array_t<FloatP> /*prim_index*/,
                                   dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);

        // Note: the outputs from this function will be post-processed into a
        // SurfaceInteraction3f by `Mesh::compute_surface_interaction()`.
        auto [valid, t, local] = intersect_impl<FloatP>(ray, active);

        // Which of the two triangles did we hit?
        FloatP local_xy = local.x() + local.y();
        dr::uint32_array_t<FloatP> prim_index = dr::select(local_xy <= 0.f, 0, 1);

        // Compute barycentric coordinates inside of the hit triangle (w.r.t. vertices 1 and 2).
        // The final intersection position will be recomputed as:
        //     si.p = p0 * (1 - b1 - b2) + p1 * b1 + p2 * b2;
        // Expression of the barycentric coordinates:
        //     b1 = ((local - p0) x (p2 - p0)) / ((p1 - p0) x (p2 - p0))
        //     b2 = ((local - p0) x (p0 - p1)) / ((p1 - p0) x (p2 - p0))
        // where `x` denotes the cross product.
        // Given the hardcoded vertices for this rectangle, it simplifies to:
        //     Triangle 0:
        //         b1 = (local.y + 1) / 2
        //         b2 = -(local.x + local.y) / 2
        //     Triangle 1:
        //         b1 = (local.x + local.y) / 2
        //         b2 = (1 - local.x) / 2
        Point<FloatP, 2> prim_uv = 0.5f * dr::select(
            prim_index == 0,
            Point<FloatP, 2>(local.y() + 1.f, -local_xy),
            Point<FloatP, 2>(local_xy, 1.f - local.x())
        );

        if (m_reversed)
            prim_uv.y() = 1.f - prim_uv.x() - prim_uv.y();

        // We don't technically need to mask the inactive lanes, but we do it
        // nevertheless to match the behavior of `Scene::ray_intersect()`.
        // Return: pi.valid, pi.t, pi.prim_uv, pi.shape_index, pi.prim_index
        return { valid, dr::select(valid, t, dr::Infinity<FloatP>),
                 prim_uv & valid, ((uint32_t) -1), dr::select(valid, prim_index, 0) };
    }

    template <typename FloatP, typename Ray3fP>
    dr::mask_t<FloatP> ray_test_impl(const Ray3fP &ray,
                                     dr::uint32_array_t<FloatP> /*prim_index*/,
                                     dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);
        return std::get<0>(intersect_impl<FloatP>(ray, active));
    }

    MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    // =============================================================

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Rectangle[" << std::endl
            << "  to_world = " << string::indent(m_to_world, 13) << "," << std::endl
            << "  frame = " << string::indent(m_frame) << "," << std::endl
            << "  surface_area = " << surface_area() << "," << std::endl
            << "  " << string::indent(get_children_string()) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(Rectangle)
private:
    Frame3f m_frame;
    Float m_inv_surface_area;

    /// Should the rectangle be turned inside out?
    bool m_flipped;

    /// Is the winding of the two triangles reversed?
    bool m_reversed;

    MI_TRAVERSE_CB(Base, m_frame, m_inv_surface_area)
};

MI_EXPORT_PLUGIN(Rectangle)
NAMESPACE_END(mitsuba)
