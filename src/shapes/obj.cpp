#include <mitsuba/render/mesh.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/mmap.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/profiler.h>

#include <nanothread/nanothread.h>

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

template <typename Float, typename Spectrum>
class OBJMesh final : public Mesh<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Mesh, m_name, m_to_world, m_vertex_count, m_face_count,
                    m_face_normals, build_from_corners)
    MI_IMPORT_TYPES()

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using typename Base::InputFloat;
    using typename Base::InputPoint3f;
    using typename Base::InputVector2f;
    using typename Base::InputNormal3f;
    using typename Base::CornerAttribute;

    OBJMesh(const Properties &props) : Base(props) {
        /* Causes all texture coordinates to be vertically flipped.
           Enabled by default, for consistency with the Mitsuba 1 behavior. */
        bool flip_tex_coords = props.get<bool>("flip_tex_coords", true);

        auto fr = file_resolver();
        fs::path file_path = fr->resolve(props.get<std::string_view>("filename"));
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
        constexpr ScalarIndex Missing = (ScalarIndex) -1;

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

        const char *eof = ptr + file_size;

        Timer timer;

        /* OBJ indices are absolute, so the file can be cut into chunks at
           newline boundaries that are parsed independently into value pools
           and per-corner index triplets, then concatenated in file order. */
        size_t chunk_count =
            std::max<size_t>(1, std::min<size_t>(file_size / (1024 * 1024),
                                                 4 * util::core_count()));

        struct Chunk {
            /// Value pools filled by 'v', 'vn', and 'vt' lines (flat layout)
            std::vector<InputFloat> vertices, normals, texcoords;

            /// Pool indices of the triangle corners (0-based, or Missing)
            std::vector<ScalarIndex> corner_v, corner_vt, corner_vn;

            /// Error message, or empty if the chunk parsed successfully
            std::string error;
        };

        std::vector<Chunk> chunks(chunk_count);
        std::vector<const char *> bounds(chunk_count + 1);
        bounds[0] = ptr;
        bounds[chunk_count] = eof;
        for (size_t i = 1; i < chunk_count; ++i) {
            const char *p = ptr + i * (file_size / chunk_count);
            if (p < bounds[i - 1])
                p = bounds[i - 1];
            const char *nl = p < eof
                ? (const char *) memchr(p, '\n', eof - p) : nullptr;
            bounds[i] = nl ? nl + 1 : eof;
        }

        // Skip space and tab characters, staying within the line
        auto skip_ws = [](const char *&p, const char *end) {
            while (p != end && (*p == ' ' || *p == '\t'))
                ++p;
        };

        // Bounded float parser (leaves 'p' unchanged on failure)
        auto parse_float = [skip_ws](const char *&p, const char *end,
                                     InputFloat &out, bool &error) {
            skip_ws(p, end);
            const char *orig = p;
            if (p != end)
                out = string::parse_float<InputFloat>(p, end, (char **) &p);
            error |= p == orig;
        };

        auto to_world = m_to_world.scalar();
        bool face_normals = m_face_normals;

        auto parse_chunk = [&](const char *ptr, const char *end, Chunk &out) {
            size_t guess = (end - ptr) / 100;
            out.vertices.reserve(guess * 3);
            out.normals.reserve(guess * 3);
            out.texcoords.reserve(guess * 2);
            out.corner_v.reserve(guess * 6);
            out.corner_vt.reserve(guess * 6);
            out.corner_vn.reserve(guess * 6);

            while (ptr < end) {
                // The current line spans ptr..eol-1; parse it in place
                const char *eol = (const char *) memchr(ptr, '\n', end - ptr);
                if (!eol)
                    eol = end;

                const char *cur = ptr;
                skip_ws(cur, eol);
                size_t len = eol - cur;

                bool parse_error = false;
                if (len >= 2 && cur[0] == 'v' && (cur[1] == ' ' || cur[1] == '\t')) {
                    // Vertex position
                    InputPoint3f p;
                    cur += 2;
                    for (size_t i = 0; i < 3; ++i)
                        parse_float(cur, eol, p[i], parse_error);
                    p = to_world * p;
                    if (unlikely(!all(dr::isfinite(p)))) {
                        out.error = "mesh contains invalid vertex position data";
                        return;
                    }
                    size_t off = out.vertices.size();
                    out.vertices.resize(off + 3);
                    dr::store(out.vertices.data() + off, p);
                } else if (len >= 3 && cur[0] == 'v' && cur[1] == 'n' && (cur[2] == ' ' || cur[2] == '\t')) {
                    if (!face_normals) {
                        // Vertex normal
                        InputNormal3f n;
                        cur += 3;
                        for (size_t i = 0; i < 3; ++i)
                            parse_float(cur, eol, n[i], parse_error);
                        n = dr::normalize(to_world * n);
                        if (unlikely(!all(dr::isfinite(n)))) {
                            out.error = "mesh contains invalid vertex normal data";
                            return;
                        }
                        size_t off = out.normals.size();
                        out.normals.resize(off + 3);
                        dr::store(out.normals.data() + off, n);
                    }
                } else if (len >= 3 && cur[0] == 'v' && cur[1] == 't' && (cur[2] == ' ' || cur[2] == '\t')) {
                    // Texture coordinate
                    InputVector2f uv;
                    cur += 3;
                    for (size_t i = 0; i < 2; ++i)
                        parse_float(cur, eol, uv[i], parse_error);
                    if (flip_tex_coords)
                        uv.y() = 1.f - uv.y();

                    size_t off = out.texcoords.size();
                    out.texcoords.resize(off + 2);
                    dr::store(out.texcoords.data() + off, uv);
                } else if (len >= 2 && cur[0] == 'f' && (cur[1] == ' ' || cur[1] == '\t')) {
                    // Face specification
                    cur += 2;
                    size_t vertex_index = 0;
                    ScalarIndex3 first, prev, corner;

                    while (true) {
                        skip_ws(cur, eol);
                        if (cur == eol || *cur == '\r')
                            break;

                        // One corner: an index triplet v, v/vt, v//vn, or v/vt/vn
                        ScalarIndex3 key {{ 0, 0, 0 }};
                        size_t type_index = 0;
                        bool corner_ok = false, has_vertex = false;

                        while (true) {
                            if (unlikely(cur != eol && *cur == '-')) {
                                // Negative (relative) indices are unsupported
                                parse_error = true;
                                break;
                            }

                            ScalarIndex value = 0;
                            bool has_digits = false;
                            while (cur != eol && (unsigned char) (*cur - '0') < 10) {
                                value = value * 10 + (ScalarIndex) (*cur - '0');
                                ++cur;
                                has_digits = true;
                            }

                            if (has_digits) {
                                if (type_index < 3) {
                                    key[type_index] = value;
                                } else {
                                    parse_error = true;
                                    break;
                                }
                                if (type_index == 0)
                                    has_vertex = true;
                            }

                            if (cur != eol && *cur == '/') {
                                do {
                                    type_index++;
                                    cur++;
                                } while (cur != eol && *cur == '/');
                                continue;
                            }

                            corner_ok = has_vertex &&
                                (cur == eol || *cur == ' ' || *cur == '\t' || *cur == '\r');
                            break;
                        }

                        if (!corner_ok)
                            break;

                        corner = ScalarIndex3 {{
                            key[0] - 1,
                            key[1] ? key[1] - 1 : Missing,
                            key[2] ? key[2] - 1 : Missing
                        }};

                        if (vertex_index == 0)
                            first = corner;

                        // Triangulate the polygon as a fan around the first corner
                        if (vertex_index >= 2) {
                            for (const ScalarIndex3 &c : { first, prev, corner }) {
                                out.corner_v.push_back(c[0]);
                                out.corner_vt.push_back(c[1]);
                                out.corner_vn.push_back(c[2]);
                            }
                        }

                        prev = corner;
                        vertex_index++;
                    }
                }

                if (unlikely(parse_error)) {
                    out.error = "could not parse line \"" +
                                std::string(ptr, eol) + '"';
                    return;
                }
                ptr = eol + 1;
            }
        };

        dr::parallel_for(
            dr::blocked_range<size_t>(0, chunk_count, 1),
            [&](const dr::blocked_range<size_t> &range) {
                for (size_t i = range.begin(); i != range.end(); ++i)
                    parse_chunk(bounds[i], bounds[i + 1], chunks[i]);
            });

        // Report the error of the earliest failed chunk, if any
        for (const Chunk &c : chunks)
            if (unlikely(!c.error.empty()))
                fail("%s", c.error);

        // Concatenate the per-chunk results in file order
        std::vector<size_t> off_v(chunk_count + 1, 0), off_n(chunk_count + 1, 0),
                            off_t(chunk_count + 1, 0), off_c(chunk_count + 1, 0);
        for (size_t i = 0; i < chunk_count; ++i) {
            off_v[i + 1] = off_v[i] + chunks[i].vertices.size();
            off_n[i + 1] = off_n[i] + chunks[i].normals.size();
            off_t[i + 1] = off_t[i] + chunks[i].texcoords.size();
            off_c[i + 1] = off_c[i] + chunks[i].corner_v.size();
        }

        std::vector<InputFloat> vertices(off_v[chunk_count]),
                                normals(off_n[chunk_count]),
                                texcoords(off_t[chunk_count]);
        std::vector<ScalarIndex> corner_v(off_c[chunk_count]),
                                 corner_vt(off_c[chunk_count]),
                                 corner_vn(off_c[chunk_count]);

        dr::parallel_for(
            dr::blocked_range<size_t>(0, chunk_count, 1),
            [&](const dr::blocked_range<size_t> &range) {
                auto append = [](auto &dst, size_t offset, const auto &src) {
                    if (!src.empty())
                        memcpy(dst.data() + offset, src.data(),
                               src.size() * sizeof(src[0]));
                };
                for (size_t i = range.begin(); i != range.end(); ++i) {
                    Chunk &c = chunks[i];
                    append(vertices,  off_v[i], c.vertices);
                    append(normals,   off_n[i], c.normals);
                    append(texcoords, off_t[i], c.texcoords);
                    append(corner_v,  off_c[i], c.corner_v);
                    append(corner_vt, off_c[i], c.corner_vt);
                    append(corner_vn, off_c[i], c.corner_vn);
                    c = Chunk();
                }
            });

        /* Index validation happens once the pool sizes are known, which
           also permits forward references within the file */
        size_t vertex_count   = vertices.size() / 3,
               texcoord_count = texcoords.size() / 2,
               normal_count   = normals.size() / 3;
        for (size_t i = 0; i < corner_v.size(); ++i) {
            if (unlikely(corner_v[i] >= vertex_count))
                fail("reference to invalid vertex %i!", corner_v[i] + 1);
            if (unlikely(corner_vt[i] != Missing && corner_vt[i] >= texcoord_count))
                fail("reference to invalid texture coordinate %i!", corner_vt[i] + 1);
            if (unlikely(!face_normals && corner_vn[i] != Missing &&
                         corner_vn[i] >= normal_count))
                fail("reference to invalid normal %i!", corner_vn[i] + 1);
        }

        CornerAttribute attrs[2];
        size_t attr_count = 0;
        if (!texcoords.empty())
            attrs[attr_count++] = { "vertex_texcoords", 2, nullptr,
                                    texcoords.data(), corner_vt.data() };
        if (!normals.empty())
            attrs[attr_count++] = { "vertex_normals", 3, nullptr,
                                    normals.data(), corner_vn.data() };

        // Weld corners that agree in all attributes into shared vertices
        build_from_corners(vertices.size() / 3, corner_v.size(),
                           vertices.data(), corner_v.data(), attrs,
                           attr_count);

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
    }

    MI_DECLARE_CLASS(OBJMesh)

    MI_TRAVERSE_CB(Base)
};

MI_EXPORT_PLUGIN(OBJMesh)
NAMESPACE_END(mitsuba)
