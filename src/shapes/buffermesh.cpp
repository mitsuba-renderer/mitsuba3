#include <mitsuba/render/mesh.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/util.h>
#include <drjit/tensor.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-buffermesh:

Simple buffer mesh (:monosp:`buffermesh`)
-------------------------

Simple mesh plugin that loads geometry directly from buffers. This is useful to
hold existing meshes in Python dictionaries.

 */

template <typename Float, typename Spectrum>
class BufferMesh final : public Mesh<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Mesh, m_vertex_count, m_face_count, m_vertex_positions,
                   m_vertex_normals, m_vertex_texcoords, m_faces,
                   m_face_normals, recompute_vertex_normals, initialize)
    MI_IMPORT_TYPES()
    using typename Base::ScalarSize;

    BufferMesh(const Properties &props) : Base(props) {
        auto vertex_positions = props.tensor<TensorXf>("vertex_positions");
        auto vertex_normals   = props.tensor<TensorXf>("vertex_normals");
        auto vertex_texcoords = props.tensor<TensorXf>("vertex_texcoords");
        auto faces            = props.tensor<TensorXf>("faces");

        m_vertex_count = (ScalarSize) vertex_positions->shape(0) / 3;
        m_face_count   = (ScalarSize) faces->shape(0) / 3;

        m_vertex_positions = vertex_positions->array();
        m_vertex_normals = vertex_normals->array();
        m_vertex_texcoords = vertex_texcoords->array();
        m_faces = faces->array();

        initialize();
    }

private:
    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(BufferMesh, Mesh)
MI_EXPORT_PLUGIN(BufferMesh, "Buffer Mesh")
NAMESPACE_END(mitsuba)
