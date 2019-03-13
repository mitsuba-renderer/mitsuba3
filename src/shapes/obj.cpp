#include <mitsuba/render/mesh.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/mmap.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/timer.h>

NAMESPACE_BEGIN(mitsuba)

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

class OBJMesh final : public Mesh {
public:
    Float strtof(const char *nptr, char **endptr) {
        #if defined(SINGLE_PRECISION)
            return std::strtof(nptr, endptr);
        #else
            return std::strtod(nptr, endptr);
        #endif
    }

    OBJMesh(const Properties &props) : Mesh(props) {
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

        Log(EDebug, "Loading mesh from \"%s\" ..", m_name);
        if (!fs::exists(file_path))
            fail("file not found");

        ref<MemoryMappedFile> mmap = new MemoryMappedFile(file_path);

        using Index3 = std::array<Index, 3>;

        struct VertexBinding {
            Index3 key {{ 0, 0, 0 }};
            Index value { 0 };
            VertexBinding *next { nullptr };
        };

        /// Temporary buffers for vertices, normals, and texture coordinates
        std::vector<Vector3f> vertices;
        std::vector<Normal3f> normals;
        std::vector<Vector2f> texcoords;
        std::vector<Index3> triangles;
        std::vector<VertexBinding> vertex_map;

        size_t vertex_guess = mmap->size() / 100;
        vertices.reserve(vertex_guess);
        normals.reserve(vertex_guess);
        texcoords.reserve(vertex_guess);
        triangles.reserve(vertex_guess * 2);
        vertex_map.resize(vertex_guess);

        Index vertex_ctr = 0;

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
                Point3f p;
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
                Normal3f n;
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
                Vector2f uv;
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
                Index3 key {{ (Index) 0, (Index) 0, (Index) 0 }};
                Index3 tri;

                while (true) {
                    const char *next2;
                    Index value = (Index) strtoul(cur, (char **) &next2, 10);
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

                        Index id;
                        if (entry->key == key) {
                            // Hit
                            id = entry->value;
                        } else {
                            // Miss
                            if (entry->key != Index3{{0, 0, 0}}) {
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
        m_face_count = (Size) triangles.size();
        m_vertex_struct = new Struct();
        for (auto name : { "x", "y", "z" })
            m_vertex_struct->append(name, Struct::EFloat);

        if (!m_disable_vertex_normals) {
            for (auto name : { "nx", "ny", "nz" })
                m_vertex_struct->append(name, Struct::EFloat);
            m_normal_offset = (Index) m_vertex_struct->offset("nx");
        }

        if (!texcoords.empty()) {
            for (auto name : { "u", "v" })
                m_vertex_struct->append(name, Struct::EFloat);
            m_texcoord_offset = (Index) m_vertex_struct->offset("u");
        }

        m_face_struct = new Struct();
        for (size_t i = 0; i < 3; ++i)
            m_face_struct->append(tfm::format("i%i", i),
                                  struct_traits<Index>::value);

        m_vertex_size = (Size) m_vertex_struct->size();
        m_face_size   = (Size) m_face_struct->size();
        m_vertices    = VertexHolder(new uint8_t[(m_vertex_count + 1) * m_vertex_size]);
        m_faces       = FaceHolder(new uint8_t[(m_face_count + 1) * m_face_size]);
        memcpy(m_faces.get(), triangles.data(), m_face_count * m_face_size);

        for (const auto& v_ : vertex_map) {
            const VertexBinding *v = &v_;

            while (v && v->key != Index3{{0, 0, 0}}) {
                uint8_t *vertex_ptr = vertex(v->value);
                auto key = v->key;

                store(vertex_ptr, vertices[key[0] - 1]);

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

        Log(EDebug, "\"%s\": read %i faces, %i vertices (%s in %s)",
            m_name, m_face_count, m_vertex_count,
            util::mem_string(m_face_count * m_face_struct->size() +
                             m_vertex_count * m_vertex_struct->size()),
            util::time_string(timer.value())
        );

        if (!m_disable_vertex_normals && normals.empty())
            recompute_vertex_normals();

        if (is_emitter())
            emitter()->set_shape(this);
    }


    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS(OBJMesh, Mesh)
MTS_EXPORT_PLUGIN(OBJMesh, "OBJ Mesh")

NAMESPACE_END(mitsuba)
