#include <mitsuba/core/fstream.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/records.h>
#include "blender_types.h"
#include <mutex>

#if defined(MTS_ENABLE_EMBREE)
    #include <embree3/rtcore.h>
#endif

#if defined(MTS_ENABLE_OPTIX)
    #include <optix.h>
    #include <optix_stubs.h>
    #include <optix_function_table_definition.h>
#endif

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT Mesh<Float, Spectrum>::Mesh(const Properties &props) : Base(props) {
    /* When set to ``true``, Mitsuba will use per-face instead of per-vertex
       normals when rendering the object, which will give it a faceted
       appearance. Default: ``false`` */
    if (props.bool_("face_normals", false))
        m_disable_vertex_normals = true;
    m_to_world = props.transform("to_world", ScalarTransform4f());
    m_mesh = true;
}

MTS_VARIANT Mesh<Float, Spectrum>::Mesh(const std::string &name, Struct *vertex_struct,
                                        ScalarSize vertex_count, Struct *face_struct,
                                        ScalarSize face_count)
    : m_name(name), m_vertex_count(vertex_count), m_face_count(face_count),
      m_vertex_struct(vertex_struct), m_face_struct(face_struct) {
    /* Helper lambda function to determine compatibility (offset/type) of a 'Struct' field */
    auto check_field = [](const Struct *s, size_t idx,
                          const std::string &suffix_exp,
                          Struct::Type type_exp) {
        if (idx >= s->field_count())
            Throw("Mesh::Mesh(): Incompatible data structure %s", s->to_string());
        auto field = s->operator[](idx);
        std::string suffix = field.name;
        auto it = suffix.rfind(".");
        if (it != std::string::npos)
            suffix = suffix.substr(it + 1);
        if (suffix != suffix_exp || field.type != type_exp)
            Throw("Mesh::Mesh(): Incompatible data structure %s", s->to_string());
    };

    check_field(vertex_struct, 0, "x",  struct_type_v<InputFloat>);
    check_field(vertex_struct, 1, "y",  struct_type_v<InputFloat>);
    check_field(vertex_struct, 2, "z",  struct_type_v<InputFloat>);

    check_field(face_struct,   0, "i0", struct_type_v<ScalarIndex>);
    check_field(face_struct,   1, "i1", struct_type_v<ScalarIndex>);
    check_field(face_struct,   2, "i2", struct_type_v<ScalarIndex>);

    if (vertex_struct->has_field("nx") &&
        vertex_struct->has_field("ny") &&
        vertex_struct->has_field("nz")) {
        check_field(vertex_struct, 3, "nx", struct_type_v<InputFloat>);
        check_field(vertex_struct, 4, "ny", struct_type_v<InputFloat>);
        check_field(vertex_struct, 5, "nz", struct_type_v<InputFloat>);
        m_normal_offset = (ScalarIndex) vertex_struct->field("nx").offset;
    }

    if (vertex_struct->has_field("u") && vertex_struct->has_field("v")) {
        if (m_normal_offset == 0) {
            check_field(vertex_struct, 3, "u", struct_type_v<InputFloat>);
            check_field(vertex_struct, 4, "v", struct_type_v<InputFloat>);
        } else {
            check_field(vertex_struct, 6, "u", struct_type_v<InputFloat>);
            check_field(vertex_struct, 7, "v", struct_type_v<InputFloat>);
        }
        m_texcoord_offset = (ScalarIndex) vertex_struct->field("u").offset;
    }

    m_vertex_size = (ScalarSize) m_vertex_struct->size();
    m_face_size   = (ScalarSize) m_face_struct->size();

    m_vertices = VertexHolder(new uint8_t[(vertex_count + 1) * m_vertex_size]);
    m_faces    = VertexHolder(new uint8_t[(face_count + 1) * m_face_size]);

    m_mesh = true;
    set_children();
}

/**
 * This constructor created a Mesh object from the part of a blender mesh assigned to a certain material.
 * This allows exporting meshes with multiple materials.
 * This method is inspired by LuxCoreRender (https://github.com/LuxCoreRender/LuxCore/blob/master/src/luxcore/pyluxcoreforblender.cpp)
 * \param name The mesh's unique name.
 * \param loop_tri_count The number of LoopTris in the mesh.
 * \param loop_tri_ptr A pointer to the list of LoopTris, a structure describing a face corner.
 * \param vertex_count The number of vertices in the mesh.
 * \param vertex_ptr A pointer to the list of vertices in blender's format.
 * \param poly_ptr A pointer to the list of faces in blender's format.
 * \param uv_ptr A pointer to the list of texture coordinates.
 * \param col_ptr A pointer to the list of vertex colors.
 * \param mat_nr The index of a material applied to the mesh. Identifies the faces we export for this mesh.
 * \param to_world The mesh transform matrix.
 */
MTS_VARIANT Mesh<Float, Spectrum>::Mesh(
    const std::string &name, uintptr_t loop_tri_count, uintptr_t loop_tri_ptr,
    uintptr_t loop_ptr, uintptr_t vertex_count, uintptr_t vertex_ptr,
    uintptr_t poly_ptr, uintptr_t uv_ptr, uintptr_t col_ptr, short mat_nr,
    const ScalarMatrix4f &to_world) {

    auto fail = [&](const char *descr, auto... args) {
        Throw(("Error while loading Blender mesh \"%s\": " + std::string(descr))
                  .c_str(),
              m_name, args...);
    };

    m_name     = name;
    m_to_world = to_world;

    const blender::MLoop *loops =
        reinterpret_cast<const blender::MLoop *>(loop_ptr);
    const blender::MLoopTri *tri_loops =
        reinterpret_cast<const blender::MLoopTri *>(loop_tri_ptr);
    const blender::MPoly *polygons =
        reinterpret_cast<const blender::MPoly *>(poly_ptr);
    const blender::MVert *verts =
        reinterpret_cast<const blender::MVert *>(vertex_ptr);
    const blender::MLoopUV *uvs =
        reinterpret_cast<const blender::MLoopUV *>(uv_ptr);
    const blender::MLoopCol *cols =
        reinterpret_cast<const blender::MLoopCol *>(col_ptr);

    bool has_uvs = (uvs != nullptr);
    if (!has_uvs)
        Log(Warn, "Mesh %s has no texture coordinates!", m_name);
    bool has_cols = (cols != nullptr);
    if (!has_cols)
        Log(Warn, "Mesh %s has no vertex colors", m_name);

    using ScalarIndex3 = std::array<ScalarIndex, 3>;
    // Temporary buffers for vertices, normals, etc.
    std::vector<InputPoint3f> tmp_vertices;
    std::vector<InputNormal3f> tmp_normals;
    std::vector<InputVector2f> tmp_uvs;
    std::vector<InputVector3f> tmp_cols;
    std::vector<ScalarIndex3> tmp_triangles;

    ScalarIndex vertex_ctr = 0;

    struct Key { // hash map key to define a unique vertex
        InputNormal3f normal;
        bool smooth{ false };
        size_t poly{ 0 }; // store the polygon face for flat shading, since
                          // comparing normals is ambiguous due to numerical
                          // precision
        InputVector2f uv{ 0.0f, 0.0f };
        InputVector3f col{ 0.0f, 0.0f, 0.0f };
        bool operator==(const Key &other) const {
            return (smooth ? normal == other.normal : poly == other.poly) &&
                   uv == other.uv && col == other.col;
        }
        bool operator!=(const Key &other) const {
            return (smooth ? normal != other.normal : poly != other.poly) ||
                   uv != other.uv || col != other.col;
        }
    };
    struct VertexBinding { // Hash map entry
        Key key;
        ScalarIndex value{ 0 }; // index of the vertex in the vertex array
        VertexBinding *next{ nullptr };
        bool is_init{ false };
    };
    std::vector<VertexBinding> vertex_map; // Hash Map to avoid adding duplicate
                                           // vertices
    vertex_map.resize(vertex_count);

    size_t duplicates_ctr = 0;
    for (size_t tri_loop_id = 0; tri_loop_id < loop_tri_count; tri_loop_id++) {
        const blender::MLoopTri &tri_loop = tri_loops[tri_loop_id];
        const blender::MPoly &face        = polygons[tri_loop.poly];

        // We only export the part of the mesh corresponding to the given
        // material id
        if (face.mat_nr != mat_nr)
            continue;

        ScalarIndex3 triangle;

        const blender::MVert &v0 = verts[loops[tri_loop.tri[0]].v];
        const blender::MVert &v1 = verts[loops[tri_loop.tri[1]].v];
        const blender::MVert &v2 = verts[loops[tri_loop.tri[2]].v];

        Array<InputPoint3f, 3> face_points;
        face_points[0] = InputPoint3f(v0.co[0], v0.co[1], v0.co[2]);
        face_points[1] = InputPoint3f(v1.co[0], v1.co[1], v1.co[2]);
        face_points[2] = InputPoint3f(v2.co[0], v2.co[1], v2.co[2]);

        InputNormal3f normal;
        if (!(blender::ME_SMOOTH & face.flag)) {
            // flat shading, use per face normals
            const InputVector3f e1 = face_points[1] - face_points[0];
            const InputVector3f e2 = face_points[2] - face_points[0];
            normal = normalize(m_to_world.transform_affine(cross(e1, e2)));
        }
        for (int i = 0; i < 3; i++) {
            const size_t loop_index = tri_loop.tri[i];
            const size_t vert_index = loops[loop_index].v;
            if (unlikely((vert_index >= vertex_count)))
                fail("reference to invalid vertex %i!", vert_index);

            const blender::MVert &vert = verts[vert_index];
            Key vert_key;
            if (blender::ME_SMOOTH & face.flag) {
                // smooth shading, store per vertex normals
                normal = InputNormal3f(vert.no[0], vert.no[1], vert.no[2]);
                normal = normalize(m_to_world.transform_affine(normal));
                vert_key.smooth = true;
            } else {
                // vert_key.smooth = false (default), flat shading
                vert_key.poly =
                    tri_loop.poly; // store the referenced polygon (face)
            }
            vert_key.normal = normal;
            if (has_uvs) {
                const blender::MLoopUV &loop_uv = uvs[loop_index];
                const InputVector2f uv(loop_uv.uv[0], loop_uv.uv[1]);
                vert_key.uv = uv;
            }
            if (has_cols) {
                const blender::MLoopCol &loop_col = cols[loop_index];
                const InputVector3f color(loop_col.r / 255.0f,
                                          loop_col.g / 255.0f,
                                          loop_col.b / 255.0f);
                vert_key.col = color;
            }
            // the vertex index in the blender mesh is the map index
            VertexBinding *map_entry = &vertex_map[vert_index];
            while (vert_key != map_entry->key && map_entry->next != nullptr)
                map_entry = map_entry->next;

            if (map_entry->is_init && map_entry->key == vert_key) {
                triangle[i] = map_entry->value;
                duplicates_ctr++;
            } else {
                if (map_entry->is_init) {
                    // add a new entry
                    map_entry->next = new VertexBinding();
                    map_entry       = map_entry->next;
                }
                ScalarSize vert_id = vertex_ctr++;
                map_entry->key     = vert_key;
                map_entry->value   = vert_id;
                map_entry->is_init = true;
                // add stuff to the temporary buffers
                tmp_vertices.push_back(
                    m_to_world.transform_affine(face_points[i]));
                tmp_normals.push_back(normal);
                if (has_uvs)
                    tmp_uvs.push_back(vert_key.uv);
                if (has_cols)
                    tmp_cols.push_back(vert_key.col);
                triangle[i] = vert_id;
            }
        }
        tmp_triangles.push_back(triangle);
    }
    Log(Warn, "Removed %i duplicates", duplicates_ctr);
    if (vertex_ctr == 0)
        return;
    m_vertex_count = vertex_ctr;
    m_face_count   = (ScalarSize) tmp_triangles.size();
    // create the vertex buffer and initialize its fields
    m_vertex_struct = new Struct();
    for (auto name : { "x", "y", "z" })
        m_vertex_struct->append(name, struct_type_v<InputFloat>);
    m_disable_vertex_normals = false;
    for (auto name : { "nx", "ny", "nz" })
        m_vertex_struct->append(name, struct_type_v<InputFloat>);
    m_normal_offset = (ScalarIndex) m_vertex_struct->offset("nx");
    if (has_uvs) {
        for (auto name : { "u", "v" })
            m_vertex_struct->append(name, struct_type_v<InputFloat>);
        m_texcoord_offset = (ScalarIndex) m_vertex_struct->offset("u");
    }
    if (has_cols) {
        for (auto name : { "r", "g", "b" })
            m_vertex_struct->append(name, struct_type_v<InputFloat>);
        m_color_offset = (ScalarIndex) m_vertex_struct->offset("r");
    }

    // create the face buffer and initialize its fields
    m_face_struct = new Struct();
    for (size_t i = 0; i < 3; ++i)
        m_face_struct->append(tfm::format("i%i", i),
                              struct_type_v<ScalarIndex>);

    m_vertex_size = (ScalarSize) m_vertex_struct->size();
    m_face_size   = (ScalarSize) m_face_struct->size();
    m_vertices =
        VertexHolder(new uint8_t[(m_vertex_count + 1) * m_vertex_size]);
    m_faces = FaceHolder(new uint8_t[(m_face_count + 1) * m_face_size]);
    memcpy(m_faces.get(), tmp_triangles.data(), m_face_count * m_face_size);

    for (ScalarIndex id = 0; id < vertex_ctr; id++) {
        uint8_t *vertex_ptr = vertex(id);

        store_unaligned(vertex_ptr, tmp_vertices[id]);
        m_bbox.expand(tmp_vertices[id]);
        store_unaligned(vertex_ptr + m_normal_offset, tmp_normals[id]);
        if (has_uvs)
            store_unaligned(vertex_ptr + m_texcoord_offset, tmp_uvs[id]);
        if (has_cols)
            store_unaligned(vertex_ptr + m_color_offset, tmp_cols[id]);
    }
    set_children();
}

MTS_VARIANT Mesh<Float, Spectrum>::~Mesh() { }

MTS_VARIANT typename Mesh<Float, Spectrum>::ScalarBoundingBox3f
Mesh<Float, Spectrum>::bbox() const {
    return m_bbox;
}

MTS_VARIANT typename Mesh<Float, Spectrum>::ScalarBoundingBox3f
Mesh<Float, Spectrum>::bbox(ScalarIndex index) const {
    Assert(index <= m_face_count);

    auto idx = (const ScalarIndex *) face(index);
    Assert(idx[0] < m_vertex_count &&
           idx[1] < m_vertex_count &&
           idx[2] < m_vertex_count);

    ScalarPoint3f v0 = vertex_position(idx[0]),
                  v1 = vertex_position(idx[1]),
                  v2 = vertex_position(idx[2]);

    return typename Mesh<Float, Spectrum>::ScalarBoundingBox3f(min(min(v0, v1), v2),
                                                               max(max(v0, v1), v2));
}

std::string type_name(const Struct::Type type) {
    switch (type) {
        case Struct::Type::Int8:    return "char";
        case Struct::Type::UInt8:   return "uchar";
        case Struct::Type::Int16:   return "short";
        case Struct::Type::UInt16:  return "ushort";
        case Struct::Type::Int32:   return "int";
        case Struct::Type::UInt32:  return "uint";
        case Struct::Type::Int64:   return "long";
        case Struct::Type::UInt64:  return "ulong";
        case Struct::Type::Float16: return "half";
        case Struct::Type::Float32: return "float";
        case Struct::Type::Float64: return "double";
        default: Throw("internal error");
    }
}

MTS_VARIANT void Mesh<Float, Spectrum>::write_ply(Stream *stream) const {

    std::string stream_name = "<stream>";
    auto fs = dynamic_cast<FileStream *>(stream);
    if (fs)
        stream_name = fs->path().filename().string();

    Log(Info, "Writing mesh to \"%s\" ..", stream_name);

    Timer timer;
    stream->write_line("ply");
    if (Struct::host_byte_order() == Struct::ByteOrder::BigEndian)
        stream->write_line("format binary_big_endian 1.0");
    else
        stream->write_line("format binary_little_endian 1.0");

    if (m_vertex_struct->field_count() > 0) {
        stream->write_line(tfm::format("element vertex %i", m_vertex_count));
        for (auto const &f : *m_vertex_struct)
            stream->write_line(
                tfm::format("property %s %s", type_name(f.type), f.name));
    }

    if (m_face_struct->field_count() > 0) {
        stream->write_line(tfm::format("element face %i", m_face_count));
        stream->write_line(tfm::format("property list uchar %s vertex_indices",
            type_name((*m_face_struct)[0].type)));
    }

    stream->write_line("end_header");

    if (m_vertex_struct->field_count() > 0) {
        stream->write(
            m_vertices.get(),
            m_vertex_struct->size() * m_vertex_count
        );
    }

    if (m_face_struct->field_count() > 0) {
        ref<Struct> face_struct_out = new Struct(true);

        face_struct_out->append("__size", Struct::Type::UInt8, +Struct::Flags::Default,3.0);
        for (auto f: *m_face_struct)
            face_struct_out->append(f.name, f.type);

        ref<StructConverter> conv =
            new StructConverter(m_face_struct, face_struct_out);

        FaceHolder temp(new uint8_t[face_struct_out->size() * m_face_count]);

        if (!conv->convert(m_face_count, m_faces.get(), temp.get()))
            Throw("PLYMesh::write(): internal error during conversion");

        stream->write(
            temp.get(),
            face_struct_out->size() * m_face_count
        );
    }

    Log(Info, "\"%s\": wrote %i faces, %i vertices (%s in %s)",
        m_name, m_face_count, m_vertex_count,
        util::mem_string(m_face_count * m_face_struct->size() +
                         m_vertex_count * m_vertex_struct->size()),
        util::time_string(timer.value())
    );
}

MTS_VARIANT void Mesh<Float, Spectrum>::recompute_vertex_normals() {
    if (!has_vertex_normals())
        Throw("Storing new normals in a Mesh that didn't have normals at "
              "construction time is not implemented yet.");

    std::vector<InputNormal3f> normals(m_vertex_count, zero<InputNormal3f>());
    size_t invalid_counter = 0;
    Timer timer;

    /* Weighting scheme based on "Computing Vertex Normals from Polygonal Facets"
       by Grit Thuermer and Charles A. Wuethrich, JGT 1998, Vol 3 */
    for (ScalarSize i = 0; i < m_face_count; ++i) {
        const ScalarIndex *idx = (const ScalarIndex *) face(i);
        Assert(idx[0] < m_vertex_count && idx[1] < m_vertex_count && idx[2] < m_vertex_count);
        InputPoint3f v[3]{ vertex_position(idx[0]),
                           vertex_position(idx[1]),
                           vertex_position(idx[2]) };

        InputVector3f side_0 = v[1] - v[0],
                      side_1 = v[2] - v[0];
        InputNormal3f n = cross(side_0, side_1);
        InputFloat length_sqr = squared_norm(n);
        if (likely(length_sqr > 0)) {
            n *= rsqrt(length_sqr);

            // Use Enoki to compute the face angles at the same time
            auto side1 = transpose(Array<Packet<InputFloat, 3>, 3>{ side_0, v[2] - v[1], v[0] - v[2] });
            auto side2 = transpose(Array<Packet<InputFloat, 3>, 3>{ side_1, v[0] - v[1], v[1] - v[2] });
            InputVector3f face_angles = unit_angle(normalize(side1), normalize(side2));

            for (size_t j = 0; j < 3; ++j)
                normals[idx[j]] += n * face_angles[j];
        }
    }

    for (ScalarSize i = 0; i < m_vertex_count; i++) {
        InputNormal3f n = normals[i];
        InputFloat length = norm(n);
        if (likely(length != 0.f)) {
            n /= length;
        } else {
            n = InputNormal3f(1, 0, 0); // Choose some bogus value
            invalid_counter++;
        }

        store(vertex(i) + m_normal_offset, n);
    }

    if (invalid_counter == 0)
        Log(Debug, "\"%s\": computed vertex normals (took %s)", m_name,
            util::time_string(timer.value()));
    else
        Log(Warn, "\"%s\": computed vertex normals (took %s, %i invalid vertices!)",
            m_name, util::time_string(timer.value()), invalid_counter);
}

MTS_VARIANT void Mesh<Float, Spectrum>::recompute_bbox() {
    m_bbox.reset();
    for (ScalarSize i = 0; i < m_vertex_count; ++i)
        m_bbox.expand(vertex_position(i));
}

MTS_VARIANT void Mesh<Float, Spectrum>::area_distr_build() {
    if (m_face_count == 0)
        Throw("Cannot create sampling table for an empty mesh: %s", to_string());

    std::lock_guard<tbb::spin_mutex> lock(m_mutex);
    std::unique_ptr<ScalarFloat[]> table(new ScalarFloat[m_face_count]);
    for (ScalarIndex i = 0; i < m_face_count; i++)
        table[i] = face_area(i);

    m_area_distr = DiscreteDistribution<Float>(
        table.get(),
        m_face_count
    );
}

MTS_VARIANT typename Mesh<Float, Spectrum>::ScalarSize
Mesh<Float, Spectrum>::primitive_count() const {
    return face_count();
}

MTS_VARIANT typename Mesh<Float, Spectrum>::ScalarFloat
Mesh<Float, Spectrum>::surface_area() const {
    area_distr_ensure();
    return m_area_distr.sum();
}

MTS_VARIANT typename Mesh<Float, Spectrum>::PositionSample3f
Mesh<Float, Spectrum>::sample_position(Float time, const Point2f &sample_, Mask active) const {
    area_distr_ensure();

    using Index = replace_scalar_t<Float, ScalarIndex>;
    Index face_idx;
    Point2f sample = sample_;
    std::tie(face_idx, sample.y()) = m_area_distr.sample_reuse(sample.y(), active);

    Array<Index, 3> fi = face_indices(face_idx, active);

    Point3f p0 = vertex_position(fi[0], active),
            p1 = vertex_position(fi[1], active),
            p2 = vertex_position(fi[2], active);

    Vector3f e0 = p1 - p0, e1 = p2 - p0;
    Point2f b = warp::square_to_uniform_triangle(sample);

    PositionSample3f ps;
    ps.p     = p0 + e0 * b.x() + e1 * b.y();
    ps.time  = time;
    ps.pdf   = m_area_distr.normalization();
    ps.delta = false;

    if (has_vertex_texcoords()) {
        Point2f uv0 = vertex_texcoord(fi[0], active),
                uv1 = vertex_texcoord(fi[1], active),
                uv2 = vertex_texcoord(fi[2], active);
        ps.uv = uv0 * (1.f - b.x() - b.y())
              + uv1 * b.x() + uv2 * b.y();
    } else {
        ps.uv = b;
    }

    if (has_vertex_normals()) {
        Normal3f n0 = vertex_normal(fi[0], active),
                 n1 = vertex_normal(fi[1], active),
                 n2 = vertex_normal(fi[2], active);
        ps.n = normalize(n0 * (1.f - b.x() - b.y())
                       + n1 * b.x() + n2 * b.y());
    } else {
        ps.n = normalize(cross(e0, e1));
    }

    return ps;
}

MTS_VARIANT Float Mesh<Float, Spectrum>::pdf_position(const PositionSample3f &, Mask) const {
    area_distr_ensure();
    return m_area_distr.normalization();
}

MTS_VARIANT void Mesh<Float, Spectrum>::fill_surface_interaction(const Ray3f & /*ray*/,
                                                                 const Float *cache,
                                                                 SurfaceInteraction3f &si,
                                                                 Mask active) const {
    // Barycentric coordinates within triangle
    Float b1 = cache[0],
          b2 = cache[1];

    Float b0 = 1.f - b1 - b2;

    auto fi = face_indices(si.prim_index, active);

    Point3f p0 = vertex_position(fi[0], active),
            p1 = vertex_position(fi[1], active),
            p2 = vertex_position(fi[2], active);

    Vector3f dp0 = p1 - p0,
             dp1 = p2 - p0;

    // Re-interpolate intersection using barycentric coordinates
    si.p[active] = p0 * b0 + p1 * b1 + p2 * b2;

    // Face normal
    Normal3f n = normalize(cross(dp0, dp1));
    si.n[active] = n;

    // Texture coordinates (if available)
    auto [dp_du, dp_dv] = coordinate_system(n);
    Point2f uv(b1, b2);
    if (has_vertex_texcoords()) {
        Point2f uv0 = vertex_texcoord(fi[0], active),
                uv1 = vertex_texcoord(fi[1], active),
                uv2 = vertex_texcoord(fi[2], active);

        uv = uv0 * b0 + uv1 * b1 + uv2 * b2;

        Vector2f duv0 = uv1 - uv0,
                 duv1 = uv2 - uv0;

        Float det     = fmsub(duv0.x(), duv1.y(), duv0.y() * duv1.x()),
              inv_det = rcp(det);

        Mask valid = neq(det, 0.f);

        dp_du[valid] = fmsub( duv1.y(), dp0, duv0.y() * dp1) * inv_det;
        dp_dv[valid] = fnmadd(duv1.x(), dp0, duv0.x() * dp1) * inv_det;
    }
    si.uv[active] = uv;

    // Shading normal (if available)
    if (has_vertex_normals()) {
        Normal3f n0 = vertex_normal(fi[0], active),
                 n1 = vertex_normal(fi[1], active),
                 n2 = vertex_normal(fi[2], active);

        n = normalize(n0 * b0 + n1 * b1 + n2 * b2);
    }

    si.sh_frame.n[active] = n;

    // Tangents
    si.dp_du[active] = dp_du;
    si.dp_dv[active] = dp_dv;
}

MTS_VARIANT std::pair<typename Mesh<Float, Spectrum>::Vector3f, typename Mesh<Float, Spectrum>::Vector3f>
Mesh<Float, Spectrum>::normal_derivative(const SurfaceInteraction3f &si, bool shading_frame,
                                         Mask active) const {
    Assert(has_vertex_normals());

    if (!shading_frame)
        return { zero<Vector3f>(), zero<Vector3f>() };

    auto fi = face_indices(si.prim_index, active);

    Point3f p0 = vertex_position(fi[0], active),
            p1 = vertex_position(fi[1], active),
            p2 = vertex_position(fi[2], active);

    Normal3f n0 = vertex_normal(fi[0], active),
             n1 = vertex_normal(fi[1], active),
             n2 = vertex_normal(fi[2], active);

    Vector3f rel = si.p - p0,
            du  = p1 - p0,
            dv  = p2 - p0;

    /* Solve a least squares problem to determine
       the UV coordinates within the current triangle */
    Float b1  = dot(du, rel), b2 = dot(dv, rel),
          a11 = dot(du, du), a12 = dot(du, dv),
          a22 = dot(dv, dv),
          inv_det = rcp(a11 * a22 - a12 * a12);

    Float u = fmsub (a22, b1, a12 * b2) * inv_det,
          v = fnmadd(a12, b1, a11 * b2) * inv_det,
          w = 1.f - u - v;

    /* Now compute the derivative of "normalize(u*n1 + v*n2 + (1-u-v)*n0)"
       with respect to [u, v] in the local triangle parameterization.

       Since d/du [f(u)/|f(u)|] = [d/du f(u)]/|f(u)|
         - f(u)/|f(u)|^3 <f(u), d/du f(u)>, this results in
    */

    Normal3f N(u * n1 + v * n2 + w * n0);
    Float il = rsqrt(squared_norm(N));
    N *= il;

    Vector3f dndu = (n1 - n0) * il;
    Vector3f dndv = (n2 - n0) * il;

    dndu = fnmadd(N, dot(N, dndu), dndu);
    dndv = fnmadd(N, dot(N, dndv), dndv);

    return { dndu, dndv };
}

namespace {
constexpr size_t max_vertices = 10;

template <typename Point3d>
size_t sutherland_hodgman(Point3d *input, size_t in_count, Point3d *output, int axis,
                          double split_pos, bool is_minimum) {
    if (in_count < 3)
        return 0;

    Point3d cur        = input[0];
    double sign        = is_minimum ? 1.0 : -1.0;
    double distance    = sign * (cur[axis] - split_pos);
    bool  cur_is_inside = (distance >= 0);
    size_t out_count    = 0;

    for (size_t i = 0; i < in_count; ++i) {
        size_t next_idx = i + 1;
        if (next_idx == in_count)
            next_idx = 0;

        Point3d next = input[next_idx];
        distance = sign * (next[axis] - split_pos);
        bool next_is_inside = (distance >= 0);

        if (cur_is_inside && next_is_inside) {
            /* Both this and the next vertex are inside, add to the list */
            Assert(out_count + 1 < max_vertices);
            output[out_count++] = next;
        } else if (cur_is_inside && !next_is_inside) {
            /* Going outside -- add the intersection */
            double t = (split_pos - cur[axis]) / (next[axis] - cur[axis]);
            Assert(out_count + 1 < max_vertices);
            Point3d p = cur + (next - cur) * t;
            p[axis] = split_pos; // Avoid roundoff errors
            output[out_count++] = p;
        } else if (!cur_is_inside && next_is_inside) {
            /* Coming back inside -- add the intersection + next vertex */
            double t = (split_pos - cur[axis]) / (next[axis] - cur[axis]);
            Assert(out_count + 2 < max_vertices);
            Point3d p = cur + (next - cur) * t;
            p[axis] = split_pos; // Avoid roundoff errors
            output[out_count++] = p;
            output[out_count++] = next;
        } else {
            /* Entirely outside - do not add anything */
        }
        cur = next;
        cur_is_inside = next_is_inside;
    }
    return out_count;
}
}  // end namespace

MTS_VARIANT typename Mesh<Float, Spectrum>::ScalarBoundingBox3f
Mesh<Float, Spectrum>::bbox(ScalarIndex index, const ScalarBoundingBox3f &clip) const {
    using ScalarPoint3d = mitsuba::Point<double, 3>;

    // Reserve room for some additional vertices
    ScalarPoint3d vertices1[max_vertices], vertices2[max_vertices];
    size_t n_vertices = 3;

    Assert(index <= m_face_count);

    auto idx = (const ScalarIndex *) face(index);
    Assert(idx[0] < m_vertex_count);
    Assert(idx[1] < m_vertex_count);
    Assert(idx[2] < m_vertex_count);

    ScalarPoint3f v0 = vertex_position(idx[0]),
                  v1 = vertex_position(idx[1]),
                  v2 = vertex_position(idx[2]);

    /* The kd-tree code will frequently call this function with
       almost-collapsed bounding boxes. It's extremely important not to
       introduce errors in such cases, otherwise the resulting tree will
       incorrectly remove triangles from the associated nodes. Hence, do
       the following computation in double precision! */

    vertices1[0] = ScalarPoint3d(v0);
    vertices1[1] = ScalarPoint3d(v1);
    vertices1[2] = ScalarPoint3d(v2);

    for (int axis = 0; axis < 3; ++axis) {
        n_vertices = sutherland_hodgman(vertices1, n_vertices, vertices2, axis,
                                        (double) clip.min[axis], true);
        n_vertices = sutherland_hodgman(vertices2, n_vertices, vertices1, axis,
                                        (double) clip.max[axis], false);
    }

    ScalarBoundingBox3f result;
    for (size_t i = 0; i < n_vertices; ++i)
        result.expand(ScalarPoint3f(vertices1[i]));

    result.min = prev_float(result.min);
    result.max = next_float(result.max);

    result.clip(clip);

    return result;
}

MTS_VARIANT std::string Mesh<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
    oss << class_()->name() << "[" << std::endl
        << "  name = \"" << m_name << "\"," << std::endl
        << "  bbox = " << string::indent(m_bbox) << "," << std::endl
        << "  vertex_struct = " << string::indent(m_vertex_struct) << "," << std::endl
        << "  vertex_count = " << m_vertex_count << "," << std::endl
        << "  vertices = [" << util::mem_string(m_vertex_size * m_vertex_count) << " of vertex data]," << std::endl
        << "  face_struct = " << string::indent(m_face_struct) << "," << std::endl
        << "  face_count = " << m_face_count << "," << std::endl
        << "  faces = [" << util::mem_string(m_face_size * m_face_count) << " of face data]," << std::endl
        << "  disable_vertex_normals = " << m_disable_vertex_normals << "," << std::endl
        << "  surface_area = " << m_area_distr.sum() << std::endl
        << "]";
    return oss.str();
}

#if defined(MTS_ENABLE_EMBREE)
MTS_VARIANT RTCGeometry Mesh<Float, Spectrum>::embree_geometry(RTCDevice device) const {
    RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);

    rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, m_vertices.get(),
            0, m_vertex_size, m_vertex_count);
    rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, m_faces.get(),
            0, m_face_size, m_face_count);

    rtcCommitGeometry(geom);
    return geom;
}
#endif

#if defined(MTS_ENABLE_OPTIX)
#define rt_check(err)       __rt_check(err, __FILE__, __LINE__)
#define rt_check_log(err)   __rt_check_log(err, __FILE__, __LINE__)

extern void __rt_check(OptixResult errval, const char *file, const int line);
extern void __rt_check_log(OptixResult errval, const char *file, const int line);

MTS_VARIANT void Mesh<Float, Spectrum>::parameters_changed(const std::vector<std::string> &/*keys*/) {
    if constexpr (is_cuda_array_v<Float>) {
        if (keys.empty() ||
            string::contains(keys, "vertex_positions") ||
            string::contains(keys, "vertex_normals") ||
            string::contains(keys, "vertex_texcoords")) {
            UInt32 vertex_range   = arange<UInt32>(m_vertex_count),
                vertex_range_2 = vertex_range * 2,
                vertex_range_3 = vertex_range * 3,
                face_range_3   = arange<UInt32>(m_face_count) * 3;

            for (size_t i = 0; i < 3; ++i)
                scatter((uint32_t*)m_optix->faces_buf + i, m_optix->faces[i], face_range_3);

            for (size_t i = 0; i < 3; ++i)
                scatter((float*)m_optix->vertex_positions_buf + i, m_optix->vertex_positions[i], vertex_range_3);

            if (has_vertex_texcoords())
                for (size_t i = 0; i < 2; ++i)
                    scatter((float*)m_optix->vertex_texcoords_buf + i, m_optix->vertex_texcoords[i], vertex_range_2);

            if (has_vertex_normals()) {
                if (requires_gradient(m_optix->vertex_positions)) {
                    m_optix->vertex_normals = zero<Vector3f>(m_vertex_count);
                    Vector3f v[3] = {
                        gather<Vector3f>(m_optix->vertex_positions, m_optix->faces.x()),
                        gather<Vector3f>(m_optix->vertex_positions, m_optix->faces.y()),
                        gather<Vector3f>(m_optix->vertex_positions, m_optix->faces.z())
                    };
                    Normal3f n = normalize(cross(v[1] - v[0], v[2] - v[0]));

                    /* Weighting scheme based on "Computing Vertex Normals from Polygonal Facets"
                    by Grit Thuermer and Charles A. Wuethrich, JGT 1998, Vol 3 */
                    for (int i = 0; i < 3; ++i) {
                        Vector3f d0 = normalize(v[(i + 1) % 3] - v[i]);
                        Vector3f d1 = normalize(v[(i + 2) % 3] - v[i]);
                        Float face_angle = safe_acos(dot(d0, d1));
                        scatter_add(m_optix->vertex_normals, n * face_angle, m_optix->faces[i]);
                    }

                    m_optix->vertex_normals = normalize(m_optix->vertex_normals);
                }

                for (size_t i = 0; i < 3; ++i)
                    scatter((float*)m_optix->vertex_normals_buf + i, m_optix->vertex_normals[i], vertex_range_3);
            }

            if (m_area_distr.empty())
                area_distr_build();

            if (m_emitter)
                m_emitter->parameters_changed({"parent"});
        }
    }
}

template <typename Value, size_t Dim, typename Func,
          typename Result = Array<Value, Dim>>
Result cuda_upload(size_t size, Func func) {
    using ScalarValue = scalar_t<Value>;
    ScalarValue *tmp = (ScalarValue *) cuda_host_malloc(size * Dim * sizeof(ScalarValue));

    for (size_t i = 0; i < size; ++i) {
        auto value = func((uint32_t) i);
        for (size_t j = 0; j < Dim; ++j)
            tmp[i + j * size] = value[j];
    }

    Result result;
    for (size_t j = 0; j < Dim; ++j) {
        void *dst = cuda_malloc(size * sizeof(ScalarValue));
        cuda_memcpy_to_device_async(dst, tmp + j * size,
                                    size * sizeof(ScalarValue));
        result[j] = CUDAArray<ScalarValue>::map(dst, size, true);
    }

    cuda_host_free(tmp);

    return result;
}

MTS_VARIANT const uint32_t Mesh<Float, Spectrum>::triangle_input_flags[1] = { OPTIX_GEOMETRY_FLAG_NONE };

MTS_VARIANT void Mesh<Float, Spectrum>::optix_geometry() {
    if constexpr (is_cuda_array_v<Float>) {
        using Index = replace_scalar_t<Float, ScalarIndex>;
        if (m_optix != nullptr)
            Throw("Mesh::optix_geometry(): OptiX geometry was already created.");
        m_optix = std::unique_ptr<OptixData>(new OptixData());

        /// Face indices
        m_optix->faces_buf = cuda_malloc(m_face_count * 3 * sizeof(uint32_t));
        m_optix->faces = cuda_upload<Index, 3>(
            m_face_count, [this](ScalarIndex i) { return face_indices(i); });

        // Vertex positions
        m_optix->vertex_positions_buf = cuda_malloc(m_vertex_count * 3 * sizeof(float));
        m_optix->vertex_positions = cuda_upload<Float, 3>(
            m_vertex_count, [this](ScalarIndex i) { return vertex_position(i); });

        // Vertex texture coordinates
        m_optix->vertex_texcoords_buf = cuda_malloc(has_vertex_texcoords() ? m_vertex_count * 2 * sizeof(float) : 0);
        if (has_vertex_texcoords())
            m_optix->vertex_texcoords = cuda_upload<Float, 2>(
                m_vertex_count, [this](ScalarIndex i) { return vertex_texcoord(i); });

        // Vertex normals
        m_optix->vertex_normals_buf = cuda_malloc(has_vertex_normals() ? m_vertex_count * 3 * sizeof(float) : 0);
        if (has_vertex_normals())
            m_optix->vertex_normals = cuda_upload<Float, 3>(
                m_vertex_count, [this](ScalarIndex i) { return vertex_normal(i); });

        parameters_changed();
        cuda_eval();
    }
}

MTS_VARIANT void Mesh<Float, Spectrum>::optix_build_input(OptixBuildInput &build_input) const {
    build_input.type                           = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
    build_input.triangleArray.vertexFormat     = OPTIX_VERTEX_FORMAT_FLOAT3;
    build_input.triangleArray.indexFormat      = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
    build_input.triangleArray.numVertices      = m_vertex_count;
    build_input.triangleArray.vertexBuffers    = (CUdeviceptr*)&m_optix->vertex_positions_buf;
    build_input.triangleArray.numIndexTriplets = m_face_count;
    build_input.triangleArray.indexBuffer      = (CUdeviceptr)m_optix->faces_buf;
    build_input.triangleArray.flags            = Mesh::triangle_input_flags;
    build_input.triangleArray.numSbtRecords    = 1;
}

MTS_VARIANT void Mesh<Float, Spectrum>::optix_hit_group_data(HitGroupData& hitgroup) const {
    hitgroup.shape_ptr           = (uintptr_t) this;
    hitgroup.faces               = m_optix->faces_buf;
    hitgroup.vertex_positions    = m_optix->vertex_positions_buf;
    hitgroup.vertex_normals      = m_optix->vertex_normals_buf;
    hitgroup.vertex_texcoords    = m_optix->vertex_texcoords_buf;
}

MTS_VARIANT void Mesh<Float, Spectrum>::traverse(TraversalCallback *callback) {
    if constexpr (is_cuda_array_v<Float>) {
        callback->put_parameter("vertex_count",     m_vertex_count);
        callback->put_parameter("face_count",       m_face_count);
        callback->put_parameter("faces",            m_optix->faces);
        callback->put_parameter("vertex_positions", m_optix->vertex_positions);
        callback->put_parameter("vertex_normals",   m_optix->vertex_normals);
        callback->put_parameter("vertex_texcoords", m_optix->vertex_texcoords);
    }
    Base::traverse(callback);
}
#endif

MTS_IMPLEMENT_CLASS_VARIANT(Mesh, Shape)
MTS_INSTANTIATE_CLASS(Mesh)
NAMESPACE_END(mitsuba)
