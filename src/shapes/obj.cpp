#include <mitsuba/render/mesh.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/mmap.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/timer.h>

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
 * - to_world
   - |transform|
   - Specifies an optional linear object-to-world transformation.
     (Default: none, i.e. object space = world space)

This plugin implements a simple loader for Wavefront OBJ files. It handles
meshes containing triangles and quadrilaterals, and it also imports vertex normals
and texture coordinates.

Loading an ordinary OBJ file is as simple as writing:

.. code-block:: xml

    <shape type="obj">
        <string name="filename" value="my_shape.obj"/>
    </shape>

.. note:: Importing geometry via OBJ files should only be used as an absolutely last resort. Due to
          inherent limitations of this format, the files tend to be unreasonably large, and parsing
          them requires significant amounts of memory and processing power. What's worse is that the
          internally stored data is often truncated, causing a loss of precision.
          If possible, use the \pluginref{ply} or \pluginref{serialized} plugins instead.

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
    MTS_IMPORT_BASE(Mesh, m_vertices, m_faces, m_normal_offset, m_vertex_size, m_face_size,
                    m_texcoord_offset, m_color_offset, m_name, m_bbox, m_to_world, m_vertex_count,
                    m_face_count, m_vertex_struct, m_face_struct, m_disable_vertex_normals,
                    recompute_vertex_normals, is_emitter, emitter, sensor, is_sensor,
                    has_vertex_normals, vertex)
    MTS_IMPORT_TYPES()

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using typename Base::VertexHolder;
    using typename Base::FaceHolder;
    using typename Base::InputFloat;
    using typename Base::InputPoint3f ;
    using typename Base::InputVector2f;
    using typename Base::InputVector3f;
    using typename Base::InputNormal3f;

    InputFloat strtof(const char *nptr, char **endptr) {
            return std::strtof(nptr, endptr);
    }

    OBJMesh(const Properties &props) : Base(props) {
        /* Causes all texture coordinates to be vertically flipped.
           Enabled by default, for consistence with the Mitsuba 1 behavior. */
        bool flip_tex_coords = props.bool_("flip_tex_coords", true);

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_name = file_path.filename().string();


        auto fail = [&](const char *descr, auto... args) {
            Throw(("Error while loading OBJ file \"%s\": " + std::string(descr))
                      .c_str(), m_name, args...);
        };

        Log(Debug, "Loading mesh from \"%s\" ..", m_name);
        if (!fs::exists(file_path))
            fail("file not found");

        ref<MemoryMappedFile> mmap = new MemoryMappedFile(file_path);

        using ScalarIndex3 = std::array<ScalarIndex, 3>;

        struct VertexBinding {
            ScalarIndex3 key {{ 0, 0, 0 }};
            ScalarIndex value { 0 };
            VertexBinding *next { nullptr };
        };

        /// Temporary buffers for vertices, normals, and texture coordinates
        std::vector<InputVector3f> vertices;
        std::vector<InputNormal3f> normals;
        std::vector<InputVector2f> texcoords;
        std::vector<ScalarIndex3> triangles;
        std::vector<VertexBinding> vertex_map;

        size_t vertex_guess = mmap->size() / 100;
        vertices.reserve(vertex_guess);
        normals.reserve(vertex_guess);
        texcoords.reserve(vertex_guess);
        triangles.reserve(vertex_guess * 2);
        vertex_map.resize(vertex_guess);

        ScalarIndex vertex_ctr = 0;

        const char *ptr = (const char *) mmap->data();
        const char *eof = ptr + mmap->size();
        char buf[1025];
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
                    p[i] = strtof(cur, (char **) &cur);
                    parse_error |= cur == orig;
                }
                p = m_to_world.transform_affine(p);
                if (unlikely(!all(enoki::isfinite(p))))
                    fail("mesh contains invalid vertex position data");
                m_bbox.expand(p);
                vertices.push_back(p);
            } else if (cur[0] == 'v' && cur[1] == 'n' && (cur[2] == ' ' || cur[2] == '\t')) {
                // Vertex normal
                InputNormal3f n;
                cur += 3;
                for (size_t i = 0; i < 3; ++i) {
                    const char *orig = cur;
                    n[i] = strtof(cur, (char **) &cur);
                    parse_error |= cur == orig;
                }
                n = normalize(m_to_world.transform_affine(n));
                if (unlikely(!all(enoki::isfinite(n))))
                    fail("mesh contains invalid vertex normal data");
                normals.push_back(n);
            } else if (cur[0] == 'v' && cur[1] == 't' && (cur[2] == ' ' || cur[2] == '\t')) {
                // Texture coordinate
                InputVector2f uv;
                cur += 3;
                for (size_t i = 0; i < 2; ++i) {
                    const char *orig = cur;
                    uv[i] = strtof(cur, (char **) &cur);
                    parse_error |= cur == orig;
                }
                if (flip_tex_coords)
                    uv.y() = 1.f - uv.y();

                texcoords.push_back(uv);
            } else if (cur[0] == 'f' && (cur[1] == ' ' || cur[1] == '\t')) {
                // Face specification
                cur += 2;
                size_t vertex_index = 0;
                size_t type_index = 0;
                ScalarIndex3 key {{ (ScalarIndex) 0, (ScalarIndex) 0, (ScalarIndex) 0 }};
                ScalarIndex3 tri;

                while (true) {
                    const char *next2;
                    ScalarIndex value = (ScalarIndex) strtoul(cur, (char **) &next2, 10);
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
                        size_t map_index = key[0] - 1;

                        if (unlikely(map_index >= vertices.size()))
                            fail("reference to invalid vertex %i!", key[0]);
                        if (unlikely(vertex_map.size() < vertices.size()))
                            vertex_map.resize(vertices.size());

                        // Hash table lookup
                        VertexBinding *entry = &vertex_map[map_index];
                        while (entry->key != key && entry->next != nullptr)
                            entry = entry->next;

                        ScalarIndex id;
                        if (entry->key == key) {
                            // Hit
                            id = entry->value;
                        } else {
                            // Miss
                            if (entry->key != ScalarIndex3{{0, 0, 0}}) {
                                entry->next = new VertexBinding();
                                entry = entry->next;
                            }
                            entry->key = key;
                            id = entry->value = vertex_ctr++;
                        }

                        if (vertex_index < 3) {
                            tri[vertex_index] = id;
                        } else {
                            tri[1] = tri[2];
                            tri[2] = id;
                        }
                        vertex_index++;

                        if (vertex_index >= 3)
                            triangles.push_back(tri);
                    }

                    cur = next2;
                }
            }

            if (unlikely(parse_error))
                fail("could not parse line \"%s\"", buf);
            ptr = next + 1;
        }

        m_vertex_count = vertex_ctr;
        m_face_count = (ScalarSize) triangles.size();
        m_vertex_struct = new Struct();
        for (auto name : { "x", "y", "z" })
            m_vertex_struct->append(name, struct_type_v<InputFloat>);

        if (!m_disable_vertex_normals) {
            for (auto name : { "nx", "ny", "nz" })
                m_vertex_struct->append(name, struct_type_v<InputFloat>);
            m_normal_offset = (ScalarIndex) m_vertex_struct->offset("nx");
        }

        if (!texcoords.empty()) {
            for (auto name : { "u", "v" })
                m_vertex_struct->append(name, struct_type_v<InputFloat>);
            m_texcoord_offset = (ScalarIndex) m_vertex_struct->offset("u");
        }

        m_face_struct = new Struct();
        for (size_t i = 0; i < 3; ++i)
            m_face_struct->append(tfm::format("i%i", i), struct_type_v<ScalarIndex>);

        m_vertex_size = (ScalarSize) m_vertex_struct->size();
        m_face_size   = (ScalarSize) m_face_struct->size();
        m_vertices    = VertexHolder(new uint8_t[(m_vertex_count + 1) * m_vertex_size]);
        m_faces       = FaceHolder(new uint8_t[(m_face_count + 1) * m_face_size]);
        memcpy(m_faces.get(), triangles.data(), m_face_count * m_face_size);

        for (const auto& v_ : vertex_map) {
            const VertexBinding *v = &v_;

            while (v && v->key != ScalarIndex3{{0, 0, 0}}) {
                uint8_t *vertex_ptr = vertex(v->value);
                auto key = v->key;

                store_unaligned(vertex_ptr, vertices[key[0] - 1]);

                if (key[1]) {
                    size_t map_index = key[1] - 1;
                    if (unlikely(map_index >= texcoords.size()))
                        fail("reference to invalid texture coordinate %i!", key[1]);
                    store_unaligned(vertex_ptr + m_texcoord_offset,
                                    texcoords[map_index]);
                }

                if (has_vertex_normals() && key[2]) {
                    size_t map_index = key[2] - 1;
                    if (unlikely(map_index >= normals.size()))
                        fail("reference to invalid normal %i!", key[2]);
                    store_unaligned(vertex_ptr + m_normal_offset, normals[key[2] - 1]);
                }

                v = v->next;
            }
        }

        Log(Debug, "\"%s\": read %i faces, %i vertices (%s in %s)",
            m_name, m_face_count, m_vertex_count,
            util::mem_string(m_face_count * m_face_struct->size() +
                             m_vertex_count * m_vertex_struct->size()),
            util::time_string(timer.value())
        );

        if (!m_disable_vertex_normals && normals.empty())
            recompute_vertex_normals();

        if (is_emitter())
            emitter()->set_shape(this);
        if (is_sensor())
            sensor()->set_shape(this);
    }

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS_VARIANT(OBJMesh, Shape)
MTS_EXPORT_PLUGIN(OBJMesh, "OBJ Mesh")
NAMESPACE_END(mitsuba)
