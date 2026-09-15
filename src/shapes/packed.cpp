#include <mitsuba/render/mesh.h>
#include <mitsuba/core/packed.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/profiler.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-packed:

Packed mesh loader (:monosp:`packed`)
-------------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the ``.packed`` container that should be loaded

 * - index
   - |int|
   - A container may hold several meshes. This parameter specifies the
     index of the entry that should be loaded. (Default: 0, i.e. the first
     one)

 * - name
   - |string|
   - Alternatively, this parameter selects an entry by its name.

 * - face_normals
   - |bool|
   - When set to |true|, any existing or computed vertex normals are
     discarded and \emph{face normals} will instead be used during rendering.
     This gives the rendered object a faceted appearance. (Default: |false|)

 * - flip_normals
   - |bool|
   - Is the mesh inverted, i.e. should the normal vectors be flipped? (Default:|false|, i.e.
     the normals point outside)

 * - to_world
   - |transform|
   - Specifies an optional linear object-to-world transformation.
     (Default: none, i.e. object space = world space)

In addition, this plugin exposes the standard mesh state parameters
documented in :ref:`sec-shape-mesh-parameters`.

This plugin loads meshes from ``.packed`` containers, which represent the
most space and time-efficient way of getting geometry information into
Mitsuba 3. A container stores the internal representation of one or more
meshes in a compressed form that matches the memory layout of the renderer
and decompresses straight into the memory that the renderer uses.
:py:meth:`mitsuba.Mesh.write_packed` produces such files, and the :ref:`file format
description <sec-packed-format>` documents their layout. Loading a mesh
from a container is considerably faster than the :ref:`ply <shape-ply>`
plugin and orders of magnitude faster than the :ref:`obj <shape-obj>`
plugin.

.. tabs::
    .. code-tab:: xml
        :name: packed

        <shape type="packed">
            <string name="filename" value="meshes.packed"/>
            <string name="name" value="teapot"/>
            <bsdf type='diffuse'/>
        </shape>

    .. code-tab:: python

        'type': 'packed',
        'filename': 'meshes.packed',
        'name': 'teapot',
        'material': {
            'type': 'diffuse',
        }
 */

template <typename Float, typename Spectrum>
class PackedMeshShape final : public Mesh<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Mesh, m_filename, m_source_path, m_to_world,
                   m_vertex_count, m_face_count, m_face_normals,
                   m_flip_normals, from_packed)
    MI_IMPORT_TYPES()

    PackedMeshShape(const Properties &props) : Base(props) {
        ScopedPhase phase(ProfilerPhase::LoadGeometry);
        Timer timer;

        ref<PackedFile> file = PackedFile::open(m_source_path);

        size_t index;
        if (props.has_property("name")) {
            std::string_view name = props.get<std::string_view>("name");
            index = file->find(name);
            if (index == file->entry_count())
                Throw("Error while loading \"%s\": the container has no "
                      "entry named \"%s\"!", m_filename, name);
        } else {
            int index_prop = props.get<int>("index", 0);
            if (index_prop < 0 || (size_t) index_prop >= file->entry_count())
                Throw("Error while loading \"%s\": entry index %i is out of "
                      "range (the container has %zu entries)!",
                      m_filename, index_prop, file->entry_count());
            index = (size_t) index_prop;
        }

        PackedFile::Entry e = file->entry(index);

        // A mesh stored without a name keeps the "<file>@<index>" label
        m_filename = e.name().empty()
            ? tfm::format("%s@%zu", m_source_path.filename(), index)
            : std::string(e.name());

        Log(Debug, "Loading mesh \"%s\" ..", m_filename);

        char tag[4];
        std::memcpy(tag, e.data(), 4);
        e.skip(4);
        if (std::memcmp(tag, "MESH", 4) != 0)
            Throw("\"%s\": the container entry does not hold a mesh.",
                  m_filename);

        uint32_t version = e.read<uint32_t>();
        if (version != PackedMeshVersion)
            Throw("\"%s\": unsupported mesh entry version %u.", m_filename,
                  version);

        uint32_t flags = e.read<uint32_t>();
        Layout layout = (Layout) (flags & (uint32_t) PackedMeshFlags::LayoutMask);
        bool normals = has_flag(layout, Layout::Normals);

        uint32_t vertex_count   = e.read<uint32_t>(),
                 face_count     = e.read<uint32_t>(),
                 position_count = e.read<uint32_t>(),
                 normal_count   = e.read<uint32_t>();

        if (position_count > vertex_count ||
            normal_count > vertex_count ||
            (has_flag(layout, Layout::Tangents) && !normals))
            Throw("\"%s\": invalid mesh entry header.", m_filename);

        struct Attribute { std::string name; bool coeffs; uint32_t dim; };
        std::vector<Attribute> attrs(e.read<uint32_t>());
        for (Attribute &a : attrs) {
            a.name   = e.read_string();
            a.coeffs = (e.read<uint8_t>() & 1) != 0;
            a.dim    = e.read<uint8_t>();
        }

        PackedMesh pm(dr::backend_v<Float>, vertex_count, face_count,
                      layout, position_count, normal_count);

        // The arrays decompress straight into the staging storage
        e.read_array(pm.vertices.data(),
                     (size_t) vertex_count * MeshVertexStride * sizeof(float));
        e.read_array(pm.faces.data(),
                     (size_t) face_count * MeshFaceStride * sizeof(uint32_t));
        if (position_count)
            e.read_array(pm.position_index.data(),
                         (size_t) vertex_count * sizeof(uint32_t));
        if (normal_count)
            e.read_array(pm.normal_index.data(),
                         (size_t) vertex_count * sizeof(uint32_t));

        for (const Attribute &a : attrs) {
            if (a.coeffs && !is_spectral_v<Spectrum>)
                Log(Warn, "\"%s\": attribute \"%s\" stores spectral "
                    "upsampling coefficients; a non-spectral variant "
                    "cannot reproduce the original colors.",
                    m_filename, a.name);
            float *dst = pm.add_attribute(a.name, a.dim,
                                          /* upsample_srgb */ !a.coeffs);
            size_t rows = string::starts_with(a.name, "face_") ? face_count
                                                                : vertex_count;
            e.read_array(dst, rows * a.dim * sizeof(float));
        }

        // The records arrived in bulk, so they are transformed after the fact
        pm.set_transform(m_to_world.scalar(), m_flip_normals);
        m_flip_normals = false;
        m_to_world = ScalarAffineTransform4f();
        pm.transform_records();

        // The stored FaceNormals flag applies when the scene description
        // leaves the property unset
        if ((flags & (uint32_t) PackedMeshFlags::FaceNormals) &&
            !props.has_property("face_normals"))
            m_face_normals = true;

        from_packed(std::move(pm));

        Log(Debug, "\"%s\": read %i faces, %i vertices (in %s)",
            m_filename, m_face_count, m_vertex_count,
            util::time_string((float) timer.value()));
    }

    MI_DECLARE_CLASS(PackedMeshShape)
};

MI_EXPORT_PLUGIN(PackedMeshShape)
NAMESPACE_END(mitsuba)
