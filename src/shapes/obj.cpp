#include <mitsuba/render/mesh.h>
#include <mitsuba/render/mesh_utils.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/mmap.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/profiler.h>

#include <array>


NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-obj:

Wavefront OBJ mesh loader (:monosp:`obj`)
-----------------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the OBJ file that should be loaded

 * - face_normals
   - |bool|
   - When set to |true|, any existing or computed vertex normals are
     discarded and *face normals* will instead be used during rendering.
     This gives the rendered object a faceted appearance. (Default: |false|)

 * - flip_tex_coords
   - |bool|
   - Treat the vertical component of the texture as inverted? Most OBJ files use this convention. (Default: |true|)

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

This plugin implements a simple loader for Wavefront OBJ files. It handles
meshes containing triangles and quadrilaterals, and it also imports vertex normals
and texture coordinates.

Loading an ordinary OBJ file is as simple as writing:

.. tabs::
    .. code-tab:: xml
        :name: obj

        <shape type="obj">
            <string name="filename" value="my_shape.obj"/>
        </shape>

    .. code-tab:: python

        'type': 'obj',
        'filename': 'my_shape.obj'

.. note:: Importing geometry via OBJ files should only be used as an absolutely
          last resort. Due to inherent limitations of this format, the files
          tend to be unreasonably large, and parsing them requires significant
          amounts of memory and processing power. What's worse is that the
          internally stored data is often truncated, causing a loss of
          precision. If possible, use the :ref:`ply <shape-ply>` or
          :ref:`serialized <shape-serialized>` plugins instead.

 */

template <bool Negate, size_t N>
void advance(const char **start_, const char *end, const char (&delim)[N]) {
    const char *start = *start_;

    while (true) {
        bool is_delim = false;
        for (size_t i = 0; i < N; ++i)
            if (*start == delim[i])
                is_delim = true;
        if ((is_delim ^ Negate) || start == end)
            break;
        ++start;
    }

    *start_ = start;
}

template <typename Float, typename Spectrum>
class OBJMesh final : public Mesh<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Mesh, m_filename, m_source_path, m_face_count,
                   m_vertex_count, has_face_normals, from_corners)
    MI_IMPORT_TYPES()

    using typename Base::InputFloat;
    using typename Base::InputPoint3f ;
    using typename Base::InputVector2f;
    using typename Base::InputNormal3f;

    OBJMesh(const Properties &props) : Base(props) {
        /* Causes all texture coordinates to be vertically flipped.
           Enabled by default, for consistency with the Mitsuba 1 behavior. */
        bool flip_tex_coords = props.get<bool>("flip_tex_coords", true);

        auto fail = [&](const char *descr, auto... args) {
            Throw(("Error while loading OBJ file \"%s\": " + std::string(descr))
                      .c_str(), m_filename, args...);
        };

        Log(Debug, "Loading mesh from \"%s\" ..", m_filename);

        ScopedPhase phase(ProfilerPhase::LoadGeometry);

        constexpr uint32_t MissingIndex = (uint32_t) -1;

        /* Value pools (flat float arrays) and per-corner indices into them.
           The heavy lifting -- corner deduplication, triangulation, normal
           generation -- happens in from_corners(). */
        std::vector<InputFloat> vertices, normals, texcoords;
        std::vector<uint32_t> corner_vertex, corner_uv, corner_normal;
        std::vector<uint32_t> face_offsets;

 #if !defined(_WIN32)
        ref<MemoryMappedFile> mmap = new MemoryMappedFile(m_source_path);
        size_t file_size           = mmap->size();
        const char *ptr            = (const char *) mmap->data();
#else
        // Memory-mapped IO performs surprisingly poorly on Windows
        ref<FileStream> fs = new FileStream(m_source_path);
        size_t file_size = fs->size();
        std::unique_ptr<char[]> tmp(new char[file_size]);
        fs->read(tmp.get(), file_size);
        const char *ptr = tmp.get();
#endif

        size_t vertex_guess = file_size / 100;
        const char *eof     = ptr + file_size;
        char buf[1025];

        vertices.reserve(vertex_guess * 3);
        normals.reserve(vertex_guess * 3);
        texcoords.reserve(vertex_guess * 2);
        corner_vertex.reserve(vertex_guess * 6);
        corner_uv.reserve(vertex_guess * 6);
        corner_normal.reserve(vertex_guess * 6);
        face_offsets.reserve(vertex_guess * 2);
        face_offsets.push_back(0);

        bool has_uv_indices = false, has_normal_indices = false;

        Timer timer;

        while (ptr < eof) {
            // Determine the offset of the next newline
            const char *next = ptr;
            advance<false>(&next, eof, "\n");

            // Copy buf into a 0-terminated buffer
            size_t size = next - ptr;
            if (size >= sizeof(buf) - 1)
                fail("file contains an excessively long line! (%i characters)", size);
            memcpy(buf, ptr, size);
            buf[size] = '\0';

            // Skip whitespace
            const char *cur = buf, *eol = buf + size;
            advance<true>(&cur, eol, " \t\r");

            bool parse_error = false;
            if (cur[0] == 'v' && (cur[1] == ' ' || cur[1] == '\t')) {
                // Vertex position
                InputPoint3f p;
                cur += 2;
                for (size_t i = 0; i < 3; ++i) {
                    const char *orig = cur;
                    p[i] = string::strtof<InputFloat>(cur, (char **) &cur);
                    parse_error |= cur == orig;
                }
                if (unlikely(!all(dr::isfinite(p))))
                    fail("mesh contains invalid vertex position data");
                for (size_t i = 0; i < 3; ++i)
                    vertices.push_back(p[i]);
            } else if (cur[0] == 'v' && cur[1] == 'n' && (cur[2] == ' ' || cur[2] == '\t')) {
                if (!has_face_normals()) {
                    cur += 3;
                    // Vertex normal
                    InputNormal3f n;
                    for (size_t i = 0; i < 3; ++i) {
                        const char *orig = cur;
                        n[i] = string::strtof<InputFloat>(cur, (char **) &cur);
                        parse_error |= cur == orig;
                    }
                    if (unlikely(!all(dr::isfinite(n))))
                        fail("mesh contains invalid vertex normal data");
                    for (size_t i = 0; i < 3; ++i)
                        normals.push_back(n[i]);
                }
            } else if (cur[0] == 'v' && cur[1] == 't' && (cur[2] == ' ' || cur[2] == '\t')) {
                // Texture coordinate
                InputVector2f uv;
                cur += 3;
                for (size_t i = 0; i < 2; ++i) {
                    const char *orig = cur;
                    uv[i] = string::strtof<InputFloat>(cur, (char **) &cur);
                    parse_error |= cur == orig;
                }
                if (flip_tex_coords)
                    uv.y() = 1.f - uv.y();

                texcoords.push_back(uv.x());
                texcoords.push_back(uv.y());
            } else if (cur[0] == 'f' && (cur[1] == ' ' || cur[1] == '\t')) {
                // Face specification
                cur += 2;
                size_t type_index = 0;
                uint32_t key[3] { 0, 0, 0 };

                while (true) {
                    const char *next2;
                    uint32_t value = (uint32_t) strtoul(cur, (char **) &next2, 10);
                    if (cur == next2)
                        break;

                    if (type_index < 3) {
                        key[type_index] = value;
                    } else {
                        parse_error = true;
                        break;
                    }

                    while (*next2 == '/') {
                        type_index++;
                        next2++;
                    }

                    if (*next2 == ' ' || *next2 == '\t' || *next2 == '\0' || *next2 == '\r') {
                        type_index = 0;

                        if (unlikely(key[0] == 0 || (key[0] - 1) * 3 >= vertices.size()))
                            fail("reference to invalid vertex %i!", key[0]);
                        if (unlikely(key[1] != 0 && (key[1] - 1) * 2 >= texcoords.size()))
                            fail("reference to invalid texture coordinate %i!", key[1]);
                        if (unlikely(key[2] != 0 && !has_face_normals() &&
                                     (key[2] - 1) * 3 >= normals.size()))
                            fail("reference to invalid normal %i!", key[2]);

                        corner_vertex.push_back(key[0] - 1);
                        corner_uv.push_back(key[1] ? key[1] - 1 : MissingIndex);
                        corner_normal.push_back(key[2] ? key[2] - 1 : MissingIndex);
                        has_uv_indices |= key[1] != 0;
                        has_normal_indices |= key[2] != 0;

                        key[1] = key[2] = 0;
                    }

                    cur = next2;
                }

                face_offsets.push_back((uint32_t) corner_vertex.size());
            }

            if (unlikely(parse_error))
                fail("could not parse line \"%s\"", buf);
            ptr = next + 1;
        }

        CornerMesh desc;
        desc.vertex_count = vertices.size() / 3;
        desc.positions = vertices.data();
        desc.corner_count = corner_vertex.size();
        desc.corner_vertex = corner_vertex.data();
        desc.face_count = face_offsets.size() - 1;
        desc.face_offsets = face_offsets.data();
        if (has_uv_indices)
            desc.texcoords = { "texcoords", 2, texcoords.data(),
                               texcoords.size() / 2, corner_uv.data() };
        if (has_normal_indices && !has_face_normals())
            desc.normals = { "normals", 3, normals.data(),
                             normals.size() / 3, corner_normal.data() };

        from_corners(desc);

        Log(Debug, "\"%s\": read %i faces, %i vertices (in %s)",
            m_filename, m_face_count, m_vertex_count,
            util::time_string((float) timer.value()));
    }

    MI_DECLARE_CLASS(OBJMesh)

    MI_TRAVERSE_CB(Base)
};

MI_EXPORT_PLUGIN(OBJMesh)
NAMESPACE_END(mitsuba)
