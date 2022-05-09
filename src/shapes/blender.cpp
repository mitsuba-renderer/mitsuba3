#include <mitsuba/render/mesh.h>
#include <drjit/color.h>
#include <array>

// Blender Mesh format types for the exporter
NAMESPACE_BEGIN(blender)
    /// Smooth shading flag
    static const int ME_SMOOTH = 1 << 0;

    /// Triangle tessellation of the mesh, contains references to 3 MLoop and the "real" face
    struct MLoopTri {
        unsigned int tri[3];
        unsigned int poly;
    };

    struct MLoopUV {
        float uv[2];
        int flag;
    };

    struct MLoopCol {
        unsigned char r, g, b, a;
    };

    struct MLoop {
        /// Vertex index.
        unsigned int v;
        /// Edge index.
        unsigned int e;
    };

    /// Contains info about the face, like material ID and smooth shading flag
    struct MPoly {
        /// Offset into loop array and number of loops in the face
        int loopstart;
        int totloop;
        short mat_nr;
        char flag, _pad;
    };

    // Vertex data structure for Blender 2.xx
    struct MVertBlender2 {
        // Position
        float co[3];
        // Normal
        short no[3];
        char flag, bweight;
    };

    // Vertex data structure for Blender 3.xx
    struct MVertBlender3 {
        // Position
        float co[3];
        char flag, bweight;
        char padding[2];
    };

    // Vertex normal data structure for Blender 3.xx
    struct MVertNormal {
        float no[3];
    };

NAMESPACE_END(blender)

NAMESPACE_BEGIN(mitsuba)

/**

Blender mesh loader
----------------------------------------------------------

This plugins converts a Blender Mesh to mitsuba's mesh layout. It is used in the Blender exporter Add-on.
It expects as input pointers to Blender mesh data structures, see GeometryExporter.save_mesh
in the mitsuba2-blender addon for an example.
 */

template <typename Float, typename Spectrum>
class BlenderMesh final : public Mesh<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Mesh, m_name, m_bbox, m_to_world, m_vertex_count,
                    m_face_count, m_vertex_positions, m_vertex_normals,
                    m_vertex_texcoords, m_faces, m_face_normals,
                    add_attribute, initialize)
    MI_IMPORT_TYPES()

    using typename Base::MeshAttributeType;
    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using typename Base::InputFloat;
    using typename Base::InputPoint3f;
    using typename Base::InputVector2f;
    using typename Base::InputVector3f;
    using typename Base::InputNormal3f;
    using typename Base::FloatStorage;

    using ScalarIndex3 = std::array<ScalarIndex, 3>;

    /**
    * This constructor created a Mesh object from the part of a blender mesh assigned to a certain material.
    * This allows exporting meshes with multiple materials.
    * This method is inspired by LuxCoreRender (https://github.com/LuxCoreRender/LuxCore/blob/master/src/luxcore/pyluxcoreforblender.cpp)
    * \param props Contains counters and pointers to blender's data structures
    */
    BlenderMesh(const Properties &props) : Base(props) {
        auto fail = [&](const char *descr, auto... args) {
            Throw(("Error while loading Blender mesh \"%s\": " + std::string(descr))
                    .c_str(),
                m_name, args...);
        };

        std::vector<std::string> necessary_fields = {"name", "mat_nr", "vert_count", "loop_tri_count", "loops", "loop_tris", "polys", "verts"};
        for (auto &s : necessary_fields) {
            if (!props.has_property(s))
                fail("missing property \"%s\"!", s);
        }

        m_name     = props.string("name");
        short mat_nr = (short) props.get<int>("mat_nr");
        size_t vertex_count = props.get<int>("vert_count");
        size_t loop_tri_count = props.get<int>("loop_tri_count");
        const blender::MLoop *loops =
            reinterpret_cast<const blender::MLoop *>(props.get<int64_t>("loops"));
        const blender::MLoopTri *tri_loops =
            reinterpret_cast<const blender::MLoopTri *>(props.get<int64_t>("loop_tris"));
        const blender::MPoly *polygons =
            reinterpret_cast<const blender::MPoly *>(props.get<int64_t>("polys"));

        // The type of vertex buffer will depend on the version of blender used.
        void *verts_ptr = reinterpret_cast<void *>(props.get<int64_t>("verts"));
        const blender::MVertBlender2 *verts_blender2 = (const blender::MVertBlender2 *) verts_ptr;
        const blender::MVertBlender3 *verts_blender3 = (const blender::MVertBlender3 *) verts_ptr;

        const blender::MVertNormal *normals_blender3 =
            reinterpret_cast<const blender::MVertNormal *>(props.get<int64_t>("normals", 0));

        // Blender 3.xx uses a slightly different memory layout for storing
        // vertices information. E.g. vertex normals are stored in a separate buffer.
        bool blender_3 = normals_blender3 != nullptr;

        bool has_cols = false;
        std::vector<std::pair<std::string, const blender::MLoopCol *>> cols;
        for (std::string &s : props.property_names()){
            if (s.rfind("vertex_", 0) == 0){
                cols.push_back({s, reinterpret_cast<const blender::MLoopCol *>(props.get<int64_t>(s))});
                has_cols = true;
            }
        }

        bool has_uvs = props.has_property("uvs");
        const blender::MLoopUV *uvs = nullptr;
        if (has_uvs)
            uvs = reinterpret_cast<const blender::MLoopUV *>(props.get<int64_t>("uvs"));
        else
            Log(Warn, "Mesh %s has no texture coordinates!", m_name);

        // Determine whether the object is globally smooth or flat shaded and set the flag accordingly
        // Blender meshes can be partially smooth AND flat (e.g. with edge split modifier)
        // In this case, flat face vertices will be duplicated.
        m_face_normals = true;
        for (size_t tri_loop_id = 0; tri_loop_id < loop_tri_count; tri_loop_id++) {
            const blender::MLoopTri &tri_loop = tri_loops[tri_loop_id];
            const blender::MPoly &face        = polygons[tri_loop.poly];
            if (blender::ME_SMOOTH & face.flag) {
                // If at least one face is smooth shaded, we need to disable face normals
                // and duplicate the (potential) flat shaded face vertices.
                m_face_normals = false;
                break;
            }
        }

        // Temporary buffers for vertices, normals, etc.
        std::vector<std::array<InputFloat, 3>> tmp_vertices; // Store as vector for alignment issues
        std::vector<std::array<InputFloat, 3>> tmp_normals; // Same here
        std::vector<InputVector2f> tmp_uvs;
        std::vector<std::vector<InputFloat>> tmp_cols; // And same here
        std::vector<ScalarIndex3> tmp_triangles;

        tmp_vertices.reserve(vertex_count);
        if (!m_face_normals)
            tmp_normals.reserve(vertex_count);
        tmp_triangles.reserve(loop_tri_count);

        if (has_uvs)
            tmp_uvs.reserve(vertex_count);
        if (has_cols) {
            tmp_cols.reserve(cols.size());
            for (size_t p = 0; p < cols.size(); p++) {
                tmp_cols.push_back(std::vector<InputFloat>());
                tmp_cols[p].reserve(3 * vertex_count);
            }
        }

        ScalarIndex vertex_ctr = 0;

        // Hash map key to define a unique vertex
        struct Key {
            InputNormal3f normal;
            bool smooth{ false };
            size_t poly{ 0 }; // store the polygon face for flat shading, since
                              // comparing normals is ambiguous due to numerical
                              // precision
            InputVector2f uv{ 0.0f, 0.0f };
            bool operator==(const Key &other) const {
                return (smooth ? normal == other.normal : poly == other.poly) &&
                       uv == other.uv;
            }
            bool operator!=(const Key &other) const {
                return (smooth ? normal != other.normal : poly != other.poly) ||
                       uv != other.uv;
            }
        };

        // Hash map entry
        struct VertexBinding {
            Key key;
            // Index of the vertex in the vertex array
            ScalarIndex value{ 0 };
            VertexBinding *next{ nullptr };
            bool is_init{ false };
        };

        // Hash Map to avoid adding duplicate vertices
        std::vector<VertexBinding> vertex_map(vertex_count);

        size_t duplicates_ctr = 0;
        for (size_t tri_loop_id = 0; tri_loop_id < loop_tri_count; tri_loop_id++) {
            const blender::MLoopTri &tri_loop = tri_loops[tri_loop_id];
            const blender::MPoly &face        = polygons[tri_loop.poly];

            // We only export the part of the mesh corresponding to the given
            // material id
            if (face.mat_nr != mat_nr)
                continue;

            ScalarIndex3 triangle;

            const float *co_0, *co_1, *co_2;
            if (blender_3) {
                co_0 = verts_blender3[loops[tri_loop.tri[0]].v].co;
                co_1 = verts_blender3[loops[tri_loop.tri[1]].v].co;
                co_2 = verts_blender3[loops[tri_loop.tri[2]].v].co;
            } else {
                co_0 = verts_blender2[loops[tri_loop.tri[0]].v].co;
                co_1 = verts_blender2[loops[tri_loop.tri[1]].v].co;
                co_2 = verts_blender2[loops[tri_loop.tri[2]].v].co;
            }

            dr::Array<InputPoint3f, 3> face_points;
            face_points[0] = InputPoint3f(co_0[0], co_0[1], co_0[2]);
            face_points[1] = InputPoint3f(co_1[0], co_1[1], co_1[2]);
            face_points[2] = InputPoint3f(co_2[0], co_2[1], co_2[2]);

            InputNormal3f normal(0.f);
            if (!(blender::ME_SMOOTH & face.flag) && !m_face_normals) {
                // Flat shading, use per face normals (only if the mesh is not globally flat)
                const InputVector3f e1 = face_points[1] - face_points[0];
                const InputVector3f e2 = face_points[2] - face_points[0];
                normal = m_to_world.scalar().transform_affine(dr::cross(e1, e2));
                if(unlikely(dr::all(dr::eq(normal, 0.f))))
                    continue; // Degenerate triangle, ignore it
                else
                    normal = dr::normalize(normal);
            }

            InputFloat color_factor = dr::rcp(255.f);

            for (int i = 0; i < 3; i++) {
                const size_t loop_index = tri_loop.tri[i];
                const size_t vert_index = loops[loop_index].v;
                if (unlikely((vert_index >= vertex_count)))
                    fail("reference to invalid vertex %i!", vert_index);

                Key vert_key;
                if (blender::ME_SMOOTH & face.flag || m_face_normals) {
                    if (!blender_3) {
                        const short *no = verts_blender2[vert_index].no;
                        // Store per vertex normals if the face is smooth or if the mesh is globally flat
                        normal = m_to_world.scalar().transform_affine(InputNormal3f(no[0], no[1], no[2]));
                    } else {
                        const float *no = normals_blender3[vert_index].no;
                        // Store per vertex normals if the face is smooth or if the mesh is globally flat
                        normal = m_to_world.scalar().transform_affine(InputNormal3f(no[0], no[1], no[2]));
                    }

                    if(unlikely(dr::all(dr::eq(normal, 0.f))))
                        fail("invalid normals!");
                    else
                        normal = dr::normalize(normal);
                    vert_key.smooth = true;
                } else {
                    // vert_key.smooth = false (default), flat shading
                    // Store the referenced polygon (face)
                    vert_key.poly = tri_loop.poly;
                }

                vert_key.normal = normal;

                if (has_uvs) {
                    const blender::MLoopUV &loop_uv = uvs[loop_index];
                    const InputVector2f uv(loop_uv.uv[0], 1.0f - loop_uv.uv[1]);
                    vert_key.uv = uv;
                }

                // Vertex index in the blender mesh is the map index
                VertexBinding *map_entry = &vertex_map[vert_index];
                while (vert_key != map_entry->key && map_entry->next != nullptr)
                    map_entry = map_entry->next;

                if (map_entry->is_init && map_entry->key == vert_key) {
                    triangle[i] = map_entry->value;
                    duplicates_ctr++;
                } else {
                    if (map_entry->is_init) {
                        // Add a new entry
                        map_entry->next = new VertexBinding();
                        map_entry       = map_entry->next;
                    }
                    ScalarSize vert_id = vertex_ctr++;
                    map_entry->key     = vert_key;
                    map_entry->value   = vert_id;
                    map_entry->is_init = true;
                    // Add stuff to the temporary buffers
                    InputPoint3f pt = m_to_world.scalar().transform_affine(face_points[i]);
                    tmp_vertices.push_back({pt.x(), pt.y(), pt.z()});
                    if (!m_face_normals)
                        tmp_normals.push_back({normal.x(), normal.y(), normal.z()});
                    if (has_uvs)
                        tmp_uvs.push_back(vert_key.uv);
                    if (has_cols) {
                        for (size_t p = 0; p < cols.size(); p++) {
                            const blender::MLoopCol &loop_col = cols[p].second[loop_index];
                            // Blender stores vertex colors in sRGB space
                            tmp_cols[p].push_back(dr::srgb_to_linear(loop_col.r * color_factor));
                            tmp_cols[p].push_back(dr::srgb_to_linear(loop_col.g * color_factor));
                            tmp_cols[p].push_back(dr::srgb_to_linear(loop_col.b * color_factor));
                        }
                    }
                    triangle[i] = vert_id;
                }
            }
            tmp_triangles.push_back(triangle);
        }
        Log(Info, "%s: Removed %i duplicates", m_name, duplicates_ctr);

        if (vertex_ctr == 0)
            return;

        m_face_count = (ScalarSize) tmp_triangles.size();
        m_faces = dr::load<DynamicBuffer<UInt32>>(tmp_triangles.data(), m_face_count * 3);

        m_vertex_count = vertex_ctr;
        m_vertex_positions = dr::load<FloatStorage>(tmp_vertices.data(), m_vertex_count * 3);
        if (!m_face_normals)
            m_vertex_normals = dr::load<FloatStorage>(tmp_normals.data(), m_vertex_count * 3);

        if (has_uvs)
            m_vertex_texcoords = dr::load<FloatStorage>(tmp_uvs.data(), m_vertex_count * 2);

        if (has_cols) {
            for (size_t p = 0; p < cols.size(); p++)
                add_attribute(cols[p].first, 3, tmp_cols[p]);
        }

        initialize();
    }

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(BlenderMesh, Mesh)
MI_EXPORT_PLUGIN(BlenderMesh, "Blender Mesh")
NAMESPACE_END(mitsuba)
