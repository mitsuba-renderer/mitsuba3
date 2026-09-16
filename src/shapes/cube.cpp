#include <mitsuba/core/fwd.h>
#include <mitsuba/render/mesh.h>
#include <cstring>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-cube:

Cube (:monosp:`cube`)
-------------------------

.. pluginparameters::

 * - flip_normals
   - |bool|
   - Is the cube inverted, i.e. should the normal vectors be flipped? (Default:|false|, i.e.
     the normals point outside)

 * - to_world
   - |transform|
   - Specifies an optional linear object-to-world transformation.
     (Default: none (i.e. object space = world space))

In addition, this plugin exposes the standard mesh state parameters
documented in :ref:`sec-shape-mesh-parameters`.

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
        'to_world': mi.ScalarAffineTransform4f([2, 10, 1])
*/

MI_VARIANT class Cube final : public Mesh<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Mesh, m_filename, m_to_world, m_flip_normals, from_packed)
    MI_IMPORT_TYPES()

    using typename Base::InputNormal3f;
    using typename Base::InputPoint3f;
    using typename Base::InputVector2f;

public:
    Cube(const Properties &props) : Base(props) {
        constexpr size_t vertex_count = 24, corner_count = 8, face_count = 12;
        m_filename = "cube";

        // The four vertices of cube face ``s`` are ``4s .. 4s+3``: they
        // share the face normal and repeat one UV pattern, and the two
        // triangles are ``{4s, 4s+1, 4s+2}`` and ``{4s+3, 4s, 4s+2}``
        const InputNormal3f side_normals[6] = {
            { 0, -1, 0 }, { 0, 1, 0 }, { 1, 0, 0 },
            { 0, 0, 1 }, { -1, 0, 0 }, { 0, 0, -1 }
        };
        const InputVector2f side_uv[4] = {
            { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 }
        };

        // Vertex -> cube corner map: the 24 vertices (four per face)
        // reference 8 distinct corners, which connects the faces in the
        // geometric topology while the authored normals keep the edges
        // sharp. Corner ``c`` sits at (+-1, +-1, +-1), with bit ``k`` of
        // ``c`` selecting the sign of axis ``k``.
        static const uint32_t position_index[vertex_count] = {
            1, 5, 4, 0,  3, 2, 6, 7,  1, 3, 7, 5,
            5, 7, 6, 4,  4, 6, 2, 0,  3, 1, 0, 2
        };

        InputPoint3f corner_positions[corner_count];
        for (uint32_t c = 0; c < corner_count; ++c)
            corner_positions[c] =
                InputPoint3f(c & 1 ? 1.f : -1.f, c & 2 ? 1.f : -1.f,
                             c & 4 ? 1.f : -1.f);

        // The normals are per vertex (an identity map), while the
        // positions reference the 8 distinct corners
        PackedMesh pm(dr::backend_v<Float>, vertex_count, face_count,
                      make_layout(true, true), corner_count);
        pm.set_transform(m_to_world.scalar(), m_flip_normals);
        m_flip_normals = false;
        m_to_world = ScalarAffineTransform4f();
        memcpy(pm.position_index.data(), position_index,
               sizeof(position_index));

        for (uint32_t s = 0; s < 6; ++s) {
            InputNormal3f n = side_normals[s];
            uint32_t v = 4 * s;
            for (uint32_t k = 0; k < 4; ++k)
                pm.set_vertex(v + k, corner_positions[position_index[v + k]],
                              n, side_uv[k]);
            pm.set_face(2 * s,     { v,     v + 1, v + 2 });
            pm.set_face(2 * s + 1, { v + 3, v,     v + 2 });
        }

        from_packed(std::move(pm));
    }

    MI_DECLARE_CLASS(Cube)

    MI_TRAVERSE_CB(Base)
};

MI_EXPORT_PLUGIN(Cube)
NAMESPACE_END(mitsuba)
