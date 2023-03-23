#include <mitsuba/core/fwd.h>
#include <mitsuba/render/mesh.h>
#include <array>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-cube:

Cube (:monosp:`cube`)
-------------------------

.. pluginparameters::
 :extra-rows: 2

 * - flip_normals
   - |bool|
   - Is the cube inverted, i.e. should the normal vectors be flipped? (Default:|false|, i.e.
     the normals point outside)

 * - to_world
   - |transform|
   - Specifies an optional linear object-to-world transformation.
     (Default: none (i.e. object space = world space))

 * - vertex_count
   - |int|
   - Total number of vertices
   - |exposed|

 * - face_count
   - |int|
   - Total number of faces
   - |exposed|

 * - faces
   - :paramtype:`uint32[]`
   - Face indices buffer (flatten)
   - |exposed|

 * - vertex_positions
   - :paramtype:`float[]`
   - Vertex positions buffer (flatten) pre-multiplied by the object-to-world transformation.
   - |exposed|, |differentiable|, |discontinuous|

 * - vertex_normals
   - :paramtype:`float[]`
   - Vertex normals buffer (flatten)  pre-multiplied by the object-to-world transformation.
   - |exposed|, |differentiable|, |discontinuous|

 * - vertex_texcoords
   - :paramtype:`float[]`
   - Vertex texcoords buffer (flatten)
   - |exposed|, |differentiable|

 * - (Mesh attribute)
   - :paramtype:`float[]`
   - Mesh attribute buffer (flatten)
   - |exposed|, |differentiable|

This shape plugin describes a cube intersection primitive, based on the triangle
mesh class.  By default, it creates a cube between the world-space positions
(−1, −1, −1) and (1, 1, 1). However, an arbitrary linear transformation may be
specified to translate, rotate, scale or skew it as desired. The parameterization
of this shape maps every face onto the rectangle :math:`[0, 1]^2` in uv space.

.. tabs::
    .. code-tab:: xml
        :name: cube

        <shape type="cube">
            <transform name="to_world">
                <scale x="2" y="10" z="1"/>
            </transform>
        </shape>

    .. code-tab:: python

        'type': 'cube',
        'to_world': mi.ScalarTransform4f([2, 10, 1])
*/

MI_VARIANT class Cube final : public Mesh<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Mesh, m_name, m_bbox, m_to_world, m_vertex_count,
                    m_face_count, m_vertex_positions, m_vertex_normals,
                    m_vertex_texcoords, m_faces, m_mesh_attributes,
                    m_face_normals, has_vertex_normals,
                    has_vertex_texcoords, recompute_vertex_normals,
                    initialize)
    MI_IMPORT_TYPES()

    using typename Base::FloatStorage;
    using typename Base::InputFloat;
    using typename Base::InputNormal3f;
    using typename Base::InputPoint3f;
    using typename Base::InputVector2f;
    using typename Base::InputVector3f;
    using typename Base::ScalarIndex;
    using typename Base::ScalarSize;
    using ScalarIndex3 = std::array<ScalarIndex, 3>;

public:
    Cube(const Properties &props) : Base(props) {
        m_face_count   = 12;
        m_vertex_count = 24;
        m_name         = "cube";

        std::vector<InputVector3f> vertices = {
            {  1, -1, -1 }, {  1, -1,  1 }, { -1, -1,  1 }, { -1, -1, -1 },
            {  1,  1, -1 }, { -1,  1, -1 }, { -1,  1,  1 }, {  1,  1,  1 },
            {  1, -1, -1 }, {  1,  1, -1 }, {  1,  1,  1 }, {  1, -1,  1 },
            {  1, -1,  1 }, {  1,  1,  1 }, { -1,  1,  1 }, { -1, -1,  1 },
            { -1, -1,  1 }, { -1,  1,  1 }, { -1,  1, -1 }, { -1, -1, -1 },
            {  1,  1, -1 }, {  1, -1, -1 }, { -1, -1, -1 }, { -1,  1, -1 }
        };
        std::vector<InputNormal3f> normals = {
            { 0, -1,  0 }, {  0, -1,  0 }, {  0, -1,  0 }, {  0, -1,  0 }, {  0, 1, 0 },
            { 0,  1,  0 }, {  0,  1,  0 }, {  0,  1,  0 }, {  1,  0,  0 }, {  1, 0, 0 },
            { 1,  0,  0 }, {  1,  0,  0 }, {  0,  0,  1 }, {  0,  0,  1 }, {  0, 0, 1 },
            { 0,  0,  1 }, { -1,  0,  0 }, { -1,  0,  0 }, { -1,  0,  0 }, { -1, 0, 0 },
            { 0,  0, -1 }, {  0,  0, -1 }, {  0,  0, -1 }, {  0,  0, -1 }
        };
        std::vector<InputVector2f> texcoords = {
            { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 },
            { 1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 },
            { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 },
            { 1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 }
        };
        std::vector<ScalarIndex3> triangles = {
            {  0,  1,  2 }, {  3,  0,  2 }, {  4,  5,  6 }, {  7,  4,  6 },
            {  8,  9, 10 }, { 11,  8, 10 }, { 12, 13, 14 }, { 15, 12, 14 },
            { 16, 17, 18 }, { 19, 16, 18 }, { 20, 21, 22 }, { 23, 20, 22 }
        };

        std::unique_ptr<float[]> vertex_positions(new float[m_vertex_count * 3]);
        std::unique_ptr<float[]> vertex_normals(new float[m_vertex_count * 3]);
        std::unique_ptr<float[]> vertex_texcoords(new float[m_vertex_count * 2]);

        for (uint8_t i = 0; i < m_vertex_count; ++i) {
                InputFloat *position_ptr = vertex_positions.get() + i * 3;
                InputFloat *normal_ptr   = vertex_normals.get() + i * 3;
                InputFloat *texcoord_ptr = vertex_texcoords.get() + i * 2;

                InputPoint3f p  = vertices[i];
                InputNormal3f n = normals[i];
                p               = m_to_world.scalar().transform_affine(p);
                n               = dr::normalize(m_to_world.scalar().transform_affine(n));

                dr::store(position_ptr, p);
                dr::store(normal_ptr, n);
                dr::store(texcoord_ptr, texcoords[i]);
                m_bbox.expand(p);
        }

        m_faces = dr::load<DynamicBuffer<UInt32>>(triangles.data(), m_face_count * 3);
        m_vertex_positions = dr::load<FloatStorage>(vertex_positions.get(), m_vertex_count * 3);
        m_vertex_normals   = dr::load<FloatStorage>(vertex_normals.get(), m_vertex_count * 3);
        m_vertex_texcoords = dr::load<FloatStorage>(vertex_texcoords.get(), m_vertex_count * 2);

        initialize();
    }

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(Cube, Mesh)
MI_EXPORT_PLUGIN(Cube, "Cube intersection primitive");
NAMESPACE_END(mitsuba)
