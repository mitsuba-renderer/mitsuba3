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
    MI_IMPORT_BASE(Mesh, m_to_world, m_is_instance,
                   m_discontinuity_types, m_shape_type, m_flip_normals, initialize,
                   m_vertex_count, m_face_count, m_faces, m_vertex_positions,
                   m_vertex_normals, m_vertex_texcoords, get_children_string)
    using typename Base::FloatStorage;
    MI_IMPORT_TYPES()

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using typename Base::InputPoint3f;
    using typename Base::InputFloat;
    using typename Base::InputVector2f;
    using typename Base::InputNormal3f;

    // Initialize vertices
    inline static const ScalarIndex s_faces[6] {
        1, 2, 0,
        1, 3, 2
    };

    Rectangle(const Properties &props) : Base(props) {
        m_vertex_count = 4;
        m_face_count = 2;
        m_shape_type = ShapeType::Rectangle;

        initialize();
    }

    void initialize() override {
        // Compute shading frame
        Normal3f n     = dr::normalize(m_to_world.value() * Normal3f(0.f, 0.f, 1.f)),
                 dp_du = m_to_world.value() * Vector3f(2.f, 0.f, 0.f),
                 dp_dv = m_to_world.value() * Vector3f(0.f, 2.f, 0.f);

        m_frame = Frame3f(dp_du, dp_dv, n);
        m_inv_surface_area = dr::rcp(surface_area());
        dr::make_opaque(m_frame, m_inv_surface_area);

        m_faces = dr::load<DynamicBuffer<UInt32>>(s_faces, 6);

        if constexpr (dr::is_diff_v<Float>) {
            // Differentiable case: launch kernels to generate coordinates
            if (dr::grad_enabled(m_to_world.value())) {
                UInt32 index = dr::arange<UInt32>(4);
                Float xf = Float(index & 1),
                      yf = Float((index & 2) >> 1);

                Point3f p =
                    m_to_world.value() * Point3f(dr::fmadd(xf, 2.f, -1.f),
                                                 dr::fmadd(yf, 2.f, -1.f), 0.f);

                using Point3fi = dr::replace_scalar_t<Point3f, InputFloat>;
                using Vector2fi = dr::replace_scalar_t<Vector2f, InputFloat>;
                using Normal3fi = dr::replace_scalar_t<Normal3f, InputFloat>;

                m_vertex_positions = dr::empty<FloatStorage>(4*3);
                m_vertex_texcoords = dr::empty<FloatStorage>(4*2);
                m_vertex_normals = dr::empty<FloatStorage>(4*3);

                scatter(m_vertex_positions, Point3fi(p), index, true, ReduceMode::Permute);
                scatter(m_vertex_texcoords, Vector2fi(xf, yf), index, true, ReduceMode::Permute);
                scatter(m_vertex_normals, Normal3fi(n), index, true, ReduceMode::Permute);
                Base::initialize();
                return;
            }
        }

        // Non-differentiable/scalar case: compute coordinates on CPU, then upload
        InputFloat vertex_positions[4*3], vertex_normals[4*3], vertex_texcoords[4*2];
        for (uint32_t index = 0; index < 4; ++index) {
            ScalarFloat xf = ScalarFloat(index & 1),
                        yf = ScalarFloat((index & 2) >> 1);

            ScalarPoint3f p = m_to_world.scalar() *
                              ScalarPoint3f(dr::fmadd(xf, 2.f, -1.f),
                                            dr::fmadd(yf, 2.f, -1.f), 0.f);
            ScalarPoint3f ns =
                normalize(m_to_world.scalar() * ScalarNormal3f(0.f, 0.f, 1.f));

            dr::store(vertex_positions + index * 3, Point<InputFloat, 3>(p));
            dr::store(vertex_normals   + index * 3, Normal<InputFloat, 3>(ns));
            dr::store(vertex_texcoords + index * 2, Vector<InputFloat, 2>(xf, yf));
        }

        m_vertex_positions = dr::load<FloatStorage>(vertex_positions, 4*3);
        m_vertex_normals   = dr::load<FloatStorage>(vertex_normals, 4*3);
        m_vertex_texcoords = dr::load<FloatStorage>(vertex_texcoords, 4*2);
        Base::initialize();
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        PositionSample3f ps = dr::zeros<PositionSample3f>();
        ps.p = m_to_world.value() *
            Point3f(dr::fmadd(sample.x(), 2.f, -1.f),
                    dr::fmadd(sample.y(), 2.f, -1.f), 0.f);
        ps.n    = m_frame.n;
        ps.pdf  = m_inv_surface_area;
        ps.uv   = sample;
        ps.time = time;
        ps.delta = false;

        if (m_flip_normals)
            ps.n = -ps.n;

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
            // Ensure previous ray-tracing operation are fully evaluated before
            // modifying the scalar values of the fields in this class
            if constexpr (dr::is_jit_v<Float>)
                dr::sync_thread();

            m_to_world = m_to_world.value().update();
            initialize();
        }
        Base::parameters_changed(keys);
    }

    SurfaceInteraction3f eval_parameterization(const Point2f &uv, uint32_t, Mask active) const override {
        SurfaceInteraction3f si{};
        si.p = m_to_world.value() *
            Point3f(dr::fmadd(uv.x(), 2.f, - 1.f),
                    dr::fmadd(uv.y(), 2.f, - 1.f), 0.f);
        si.sh_frame  = m_frame;
        si.n         = m_frame.n;
        si.dp_du     = m_frame.s;
        si.dp_dv     = m_frame.t;
        si.uv        = uv;
        si.dn_du = si.dn_dv = dr::zeros<Vector3f>();
        si.shape    = this;
        si.instance = nullptr;
        si.t        = dr::select(active, 0, dr::Infinity<Float>);

        /// Zero-initialize remaining fields
        si.time        = 0.f;
        si.wavelengths = Wavelength(0.f);
        si.dn_du = si.dn_dv = si.wi = Vector3f(0);
        si.duv_dx = si.duv_dy = 0;
        si.prim_index = 0;

        return si;
    }

    bool parameters_grad_enabled() const override {
        return dr::grad_enabled(m_frame) ||
               dr::grad_enabled(m_to_world.value());
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
                                   ScalarIndex /*prim_index*/,
                                   dr::mask_t<FloatP> active) const {
        // Note: the outputs from this function will be post-processed into a
        // SurfaceInteraction3f by `Mesh::compute_surface_interaction()`.

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

        // Which of the two triangles did we hit?
        const auto local_xy = local.x() + local.y();
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

        // We don't technically need to mask the inactive lanes, but we do it
        // nevertheless to match the behavior of `Scene::ray_intersect()`.
        // Return: pi.t, pi.prim_uv, pi.shape_index, pi.prim_index
        return { dr::select(active, t, dr::Infinity<FloatP>),
                 prim_uv & active, ((uint32_t) -1), dr::select(active, prim_index, 0) };
    }

    template <typename FloatP, typename Ray3fP>
    dr::mask_t<FloatP> ray_test_impl(const Ray3fP &ray_,
                                     ScalarIndex /*prim_index*/,
                                     dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);

        AffineTransform<Point<FloatP, 4>> to_object;
        if constexpr (!dr::is_jit_v<FloatP>)
            to_object = m_to_world.scalar().inverse();
        else
            to_object = m_to_world.value().inverse();

        Ray3fP ray     = to_object * ray_;
        FloatP t       = -ray.o.z() / ray.d.z();
        Point<FloatP, 3> local = ray(t);

        // Is intersection within ray segment and rectangle?
        return active && t >= 0.f
                      && t <= ray.maxt
                      && dr::abs(local.x()) <= 1.f
                      && dr::abs(local.y()) <= 1.f;
    }

    MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    //! @}
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

    MI_TRAVERSE_CB(Base, m_frame, m_inv_surface_area)
};

MI_EXPORT_PLUGIN(Rectangle)
NAMESPACE_END(mitsuba)
