#include <mitsuba/render/mesh.h>
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
 :extra-rows: 5

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
    MI_IMPORT_BASE(Mesh, m_name, m_bbox, m_to_world, m_vertex_count,
                    m_face_count, m_vertex_positions, m_vertex_normals,
                    m_vertex_texcoords, m_faces, m_face_normals,
                    recompute_vertex_normals, has_vertex_normals, initialize)
    MI_IMPORT_TYPES()

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using typename Base::InputFloat;
    using typename Base::InputPoint3f ;
    using typename Base::InputVector2f;
    using typename Base::InputVector3f;
    using typename Base::InputNormal3f;
    using typename Base::FloatStorage;

    OBJMesh(const Properties &props) : Base(props) {
        /* Causes all texture coordinates to be vertically flipped.
           Enabled by default, for consistency with the Mitsuba 1 behavior. */
        bool flip_tex_coords = props.get<bool>("flip_tex_coords", true);

        auto fr = Thread::thread()->file_resolver();
        fs::path file_path = fr->resolve(props.string("filename"));
        m_name = file_path.filename().string();


        auto fail = [&](const char *descr, auto... args) {
            Throw(("Error while loading OBJ file \"%s\": " + std::string(descr))
                      .c_str(), m_name, args...);
        };

        Log(Debug, "Loading mesh from \"%s\" ..", m_name);
        if (!fs::exists(file_path))
            fail("file not found");

        ScopedPhase phase(ProfilerPhase::LoadGeometry);

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

 #if !defined(_WIN32)
        ref<MemoryMappedFile> mmap = new MemoryMappedFile(file_path);
        size_t file_size           = mmap->size();
        const char *ptr            = (const char *) mmap->data();
#else
        // Memory-mapped IO performs surprisingly poorly on Windows
        ref<FileStream> fs = new FileStream(file_path);
        size_t file_size = fs->size();
        std::unique_ptr<char[]> tmp(new char[file_size]);
        fs->read(tmp.get(), file_size);
        const char *ptr = tmp.get();
#endif

        size_t vertex_guess = file_size / 100;
        const char *eof     = ptr + file_size;
        char buf[1025];

        vertices.reserve(vertex_guess);
        normals.reserve(vertex_guess);
        texcoords.reserve(vertex_guess);
        triangles.reserve(vertex_guess * 2);
        vertex_map.resize(vertex_guess);

        ScalarIndex vertex_ctr = 0;

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
                p = m_to_world.scalar().transform_affine(p);
                if (unlikely(!all(dr::isfinite(p))))
                    fail("mesh contains invalid vertex position data");
                m_bbox.expand(p);
                vertices.push_back(p);
            } else if (cur[0] == 'v' && cur[1] == 'n' && (cur[2] == ' ' || cur[2] == '\t')) {
                if (!m_face_normals) {
                    cur += 3;
                    // Vertex normal
                    InputNormal3f n;
                    for (size_t i = 0; i < 3; ++i) {
                        const char *orig = cur;
                        n[i] = string::strtof<InputFloat>(cur, (char **) &cur);
                        parse_error |= cur == orig;
                    }
                    n = dr::normalize(m_to_world.scalar().transform_affine(n));
                    if (unlikely(!all(dr::isfinite(n))))
                        fail("mesh contains invalid vertex normal data");
                    normals.push_back(n);
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

        std::unique_ptr<float[]> vertex_positions(new float[m_vertex_count * 3]);
        std::unique_ptr<float[]> vertex_normals(new float[m_vertex_count * 3]);
        std::unique_ptr<float[]> vertex_texcoords(new float[m_vertex_count * 2]);

        for (const auto& v_ : vertex_map) {
            const VertexBinding *v = &v_;

            while (v && v->key != ScalarIndex3{{0, 0, 0}}) {
                InputFloat* position_ptr = vertex_positions.get() + v->value * 3;
                InputFloat* normal_ptr   = vertex_normals.get() + v->value * 3;
                InputFloat* texcoord_ptr = vertex_texcoords.get() + v->value * 2;
                auto key = v->key;

                dr::store(position_ptr, vertices[key[0] - 1]);

                if (key[1]) {
                    size_t map_index = key[1] - 1;
                    if (unlikely(map_index >= texcoords.size()))
                        fail("reference to invalid texture coordinate %i!", key[1]);
                    dr::store(texcoord_ptr, texcoords[map_index]);
                }

                if (!m_face_normals && key[2]) {
                    size_t map_index = key[2] - 1;
                    if (unlikely(map_index >= normals.size()))
                        fail("reference to invalid normal %i!", key[2]);
                    dr::store(normal_ptr, normals[key[2] - 1]);
                }

                v = v->next;
            }
        }

        m_faces = dr::load<DynamicBuffer<UInt32>>(triangles.data(), m_face_count * 3);
        m_vertex_positions = dr::load<FloatStorage>(vertex_positions.get(), m_vertex_count * 3);
        if (!m_face_normals)
            m_vertex_normals   = dr::load<FloatStorage>(vertex_normals.get(), m_vertex_count * 3);
        if (!texcoords.empty())
            m_vertex_texcoords = dr::load<FloatStorage>(vertex_texcoords.get(), m_vertex_count * 2);

        size_t vertex_data_bytes = 3 * sizeof(InputFloat);
        if (!m_face_normals)
            vertex_data_bytes += 3 * sizeof(InputFloat);
        if (!texcoords.empty())
            vertex_data_bytes += 2 * sizeof(InputFloat);

        Log(Debug, "\"%s\": read %i faces, %i vertices (%s in %s)",
            m_name, m_face_count, m_vertex_count,
            util::mem_string(m_face_count * 3 * sizeof(ScalarIndex) +
                             m_vertex_count * vertex_data_bytes),
            util::time_string((float) timer.value())
        );

        if (!m_face_normals && normals.empty()) {
            Timer timer2;
            recompute_vertex_normals();
            Log(Debug, "\"%s\": computed vertex normals (took %s)", m_name,
                util::time_string((float) timer2.value()));
        }

        initialize();
    }

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(OBJMesh, Mesh)
MI_EXPORT_PLUGIN(OBJMesh, "OBJ Mesh")
NAMESPACE_END(mitsuba)
