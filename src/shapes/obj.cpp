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

        /// Value pools filled by 'v', 'vn', and 'vt' lines (flat layout)
        std::vector<InputFloat> vertices;
        std::vector<InputFloat> normals;
        std::vector<InputFloat> texcoords;

        /// Pool indices of each triangle corner (0-based, Missing if absent)
        std::vector<ScalarIndex> corner_v, corner_vt, corner_vn;

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

        vertices.reserve(vertex_guess * 3);
        normals.reserve(vertex_guess * 3);
        texcoords.reserve(vertex_guess * 2);
        corner_v.reserve(vertex_guess * 6);
        corner_vt.reserve(vertex_guess * 6);
        corner_vn.reserve(vertex_guess * 6);

        Timer timer;

        // Skip space and tab characters, staying within the line
        auto skip_ws = [](const char *&p, const char *end) {
            while (p != end && (*p == ' ' || *p == '\t'))
                ++p;
        };

        // Bounded float parser (leaves 'p' unchanged on failure)
        auto parse_float = [&](const char *&p, const char *end,
                               InputFloat &out, bool &error) {
            skip_ws(p, end);
            const char *orig = p;
            if (p != end)
                out = string::parse_float<InputFloat>(p, end, (char **) &p);
            error |= p == orig;
        };

        while (ptr < eof) {
            // The current line spans ptr..eol-1; parse it in place
            const char *eol = (const char *) memchr(ptr, '\n', eof - ptr);
            if (!eol)
                eol = eof;

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
                p = m_to_world.scalar() * p;
                if (unlikely(!all(dr::isfinite(p))))
                    fail("mesh contains invalid vertex position data");
                size_t off = vertices.size();
                vertices.resize(off + 3);
                dr::store(vertices.data() + off, p);
            } else if (len >= 3 && cur[0] == 'v' && cur[1] == 'n' && (cur[2] == ' ' || cur[2] == '\t')) {
                if (!m_face_normals) {
                    // Vertex normal
                    InputNormal3f n;
                    cur += 3;
                    for (size_t i = 0; i < 3; ++i)
                        parse_float(cur, eol, n[i], parse_error);
                    n = dr::normalize(m_to_world.scalar() * n);
                    if (unlikely(!all(dr::isfinite(n))))
                        fail("mesh contains invalid vertex normal data");
                    size_t off = normals.size();
                    normals.resize(off + 3);
                    dr::store(normals.data() + off, n);
                }
            } else if (len >= 3 && cur[0] == 'v' && cur[1] == 't' && (cur[2] == ' ' || cur[2] == '\t')) {
                // Texture coordinate
                InputVector2f uv;
                cur += 3;
                for (size_t i = 0; i < 2; ++i)
                    parse_float(cur, eol, uv[i], parse_error);
                if (flip_tex_coords)
                    uv.y() = 1.f - uv.y();

                size_t off = texcoords.size();
                texcoords.resize(off + 2);
                dr::store(texcoords.data() + off, uv);
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

                    if (unlikely(key[0] - 1 >= vertices.size() / 3))
                        fail("reference to invalid vertex %i!", key[0]);

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
                            corner_v.push_back(c[0]);
                            corner_vt.push_back(c[1]);
                            corner_vn.push_back(c[2]);
                        }
                    }

                    prev = corner;
                    vertex_index++;
                }
            }

            if (unlikely(parse_error))
                fail("could not parse line \"%s\"", std::string(ptr, eol));
            ptr = eol + 1;
        }

        // Texture coordinate and normal references may precede their pools
        size_t texcoord_count = texcoords.size() / 2,
               normal_count   = normals.size() / 3;
        for (ScalarIndex i : corner_vt) {
            if (unlikely(i != Missing && i >= texcoord_count))
                fail("reference to invalid texture coordinate %i!", i + 1);
        }
        if (!m_face_normals) {
            for (ScalarIndex i : corner_vn) {
                if (unlikely(i != Missing && i >= normal_count))
                    fail("reference to invalid normal %i!", i + 1);
            }
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
