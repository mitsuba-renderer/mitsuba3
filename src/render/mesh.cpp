#include <mitsuba/core/fstream.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>

#if defined(MI_ENABLE_EMBREE)
    #include <embree3/rtcore.h>
#endif

#if defined(MI_ENABLE_CUDA)
# if defined(MI_USE_OPTIX_HEADERS)
    #include <optix_function_table_definition.h>
# endif
    #include "../shapes/optix/mesh.cuh"
#endif

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Mesh<Float, Spectrum>::Mesh(const Properties &props) : Base(props) {
    /* When set to ``true``, Mitsuba will use per-face instead of per-vertex
       normals when rendering the object, which will give it a faceted
       appearance. Default: ``false`` */
    m_face_normals = props.get<bool>("face_normals", false);
    m_flip_normals = props.get<bool>("flip_normals", false);

    m_discontinuity_types = (uint32_t) DiscontinuityFlags::PerimeterType;

    m_shape_type = ShapeType::Mesh;
}

MI_VARIANT
Mesh<Float, Spectrum>::Mesh(const std::string &name, ScalarSize vertex_count,
                            ScalarSize face_count, const Properties &props,
                            bool has_vertex_normals, bool has_vertex_texcoords) : Mesh(props) {
    m_name = name;
    m_vertex_count = vertex_count;
    m_face_count = face_count;

    m_faces = dr::zeros<DynamicBuffer<UInt32>>(m_face_count * 3);
    m_vertex_positions = dr::zeros<FloatStorage>(m_vertex_count * 3);

    if (has_vertex_normals)
        m_vertex_normals = dr::zeros<FloatStorage>(m_vertex_count * 3);
    if (has_vertex_texcoords)
        m_vertex_texcoords = dr::zeros<FloatStorage>(m_vertex_count * 2);
}

MI_VARIANT
void Mesh<Float, Spectrum>::initialize() {
#if defined(MI_ENABLE_LLVM) && !defined(MI_ENABLE_EMBREE)
    m_vertex_positions_ptr = m_vertex_positions.data();
    m_faces_ptr = m_faces.data();
#endif
    if (m_emitter || m_sensor)
        ensure_pmf_built();
    mark_dirty();

    if constexpr (dr::is_jit_v<Float>) {
        if (parameters_grad_enabled()) {
            build_directed_edges();
            build_indirect_silhouette_distribution();
        }
    }

    Base::initialize();
}

MI_VARIANT Mesh<Float, Spectrum>::~Mesh() {}

MI_VARIANT void Mesh<Float, Spectrum>::traverse(TraversalCallback *callback) {
    Base::traverse(callback);

    callback->put_parameter("faces",            m_faces,            +ParamFlags::NonDifferentiable);
    callback->put_parameter("vertex_positions", m_vertex_positions, ParamFlags::Differentiable | ParamFlags::Discontinuous);
    callback->put_parameter("vertex_normals",   m_vertex_normals,   ParamFlags::Differentiable | ParamFlags::Discontinuous);
    callback->put_parameter("vertex_texcoords", m_vertex_texcoords, +ParamFlags::Differentiable);

    // We arbitrarily chose to show all attributes as being differentiable here.
    for (auto &[name, attribute]: m_mesh_attributes)
        callback->put_parameter(name, attribute.buf, +ParamFlags::Differentiable);
}

MI_VARIANT void Mesh<Float, Spectrum>::parameters_changed(const std::vector<std::string> &keys) {
    bool mesh_attributes_changed = false;

    if (m_vertex_positions.size() != m_vertex_count * 3) {
        Log(Debug, "parameters_changed(): Vertex count changed, updating it.");
        mesh_attributes_changed = true;
        m_vertex_count = (uint32_t) m_vertex_positions.size() / 3;
    }
    if (m_faces.size() != m_face_count * 3) {
        Log(Debug, "parameters_changed(): Face count changed, updating it.");
        mesh_attributes_changed = true;
        m_face_count = (uint32_t) m_faces.size() / 3;
    }
    if (has_vertex_normals() && m_vertex_normals.size() != m_vertex_count * 3) {
        Log(Debug, "parameters_changed(): Vertex normal count changed, updating it.");
        mesh_attributes_changed = true;
        m_vertex_normals = dr::zeros<FloatStorage>(m_vertex_count * 3);
    }
    if (has_vertex_texcoords() && m_vertex_texcoords.size() != m_vertex_count * 2) {
        Log(Debug, "parameters_changed(): Vertex count has changed, but no UVs were specified, resetting them.");
        mesh_attributes_changed = true;
        m_vertex_texcoords = dr::zeros<FloatStorage>(m_vertex_count * 2);
    }
    for (auto &[name, attribute]: m_mesh_attributes) {
        size_t expected_size = attribute.size * (attribute.type == MeshAttributeType::Vertex ? m_vertex_count : m_face_count);

        if (attribute.buf.size() != expected_size ) {
            Log(Debug, "parameters_changed(): Vertex or face count changed, but attribute \"%s\" was not updated, resetting it.", name);
            mesh_attributes_changed = true;
            attribute.buf = dr::zeros<FloatStorage>(expected_size);
        }
    }

    if (keys.empty() || string::contains(keys, "faces")) { // Topology changed
        m_E2E_outdated = true;
        if (parameters_grad_enabled())
            build_directed_edges();
    }

    if (keys.empty() || string::contains(keys, "vertex_positions") || mesh_attributes_changed) {
        recompute_bbox();

        if (has_vertex_normals())
            recompute_vertex_normals();

        if (!m_area_pmf.empty() || m_emitter || m_sensor)
            build_pmf();

        if (m_parameterization)
            m_parameterization = nullptr;

        if (parameters_grad_enabled()) {
            // A topology change could have been made in a first update, and
            // then the vertex enabled gradient tracking in a second update
            if (m_E2E_outdated)
                build_directed_edges();
            build_indirect_silhouette_distribution();
        }

#if defined(MI_ENABLE_LLVM) && !defined(MI_ENABLE_EMBREE)
        m_vertex_positions_ptr = m_vertex_positions.data();
        m_faces_ptr = m_faces.data();
#endif
        mark_dirty();

        if (!m_initialized)
            Base::initialize();
    }

    Base::parameters_changed();
}

MI_VARIANT typename Mesh<Float, Spectrum>::ScalarBoundingBox3f
Mesh<Float, Spectrum>::bbox() const {
    return m_bbox;
}

MI_VARIANT typename Mesh<Float, Spectrum>::ScalarBoundingBox3f
Mesh<Float, Spectrum>::bbox(ScalarIndex index) const {
    if constexpr (dr::is_cuda_v<Float>)
        Throw("bbox(ScalarIndex) is not available in CUDA mode!");

    Assert(index <= m_face_count);

    ScalarVector3u fi = face_indices(index);

    Assert(fi[0] < m_vertex_count &&
           fi[1] < m_vertex_count &&
           fi[2] < m_vertex_count);

    ScalarPoint3f v0 = vertex_position(fi[0]),
                  v1 = vertex_position(fi[1]),
                  v2 = vertex_position(fi[2]);

    return typename Mesh<Float, Spectrum>::ScalarBoundingBox3f(dr::minimum(dr::minimum(v0, v1), v2),
                                                               dr::maximum(dr::maximum(v0, v1), v2));
}


MI_VARIANT void Mesh<Float, Spectrum>::write_ply(const std::string &filename) const {
    ref<FileStream> stream =
        new FileStream(filename, FileStream::ETruncReadWrite);

    Timer timer;
    Log(Info, "Writing mesh to \"%s\" ..", filename);
    write_ply(stream);
    Log(Info, "\"%s\": wrote %i faces, %i vertices (%s in %s)", filename,
        m_face_count, m_vertex_count,
        util::mem_string(m_face_count * face_data_bytes() +
                         m_vertex_count * vertex_data_bytes()),
        util::time_string((float) timer.value()));
}

MI_VARIANT void Mesh<Float, Spectrum>::write_ply(Stream *stream) const {
    auto&& vertex_positions = dr::migrate(m_vertex_positions, AllocType::Host);
    auto&& vertex_normals   = dr::migrate(m_vertex_normals, AllocType::Host);
    auto&& vertex_texcoords = dr::migrate(m_vertex_texcoords, AllocType::Host);
    auto&& faces = dr::migrate(m_faces, AllocType::Host);

    std::vector<std::pair<std::string, MeshAttribute>> vertex_attributes;
    std::vector<std::pair<std::string, MeshAttribute>> face_attributes;

    for (const auto&[name, attribute]: m_mesh_attributes) {
        switch (attribute.type) {
            case MeshAttributeType::Vertex:
                vertex_attributes.push_back(
                    { name.substr(7), attribute.migrate(AllocType::Host) });
                break;
            case MeshAttributeType::Face:
                face_attributes.push_back(
                    { name.substr(5), attribute.migrate(AllocType::Host) });
                break;
        }
    }
    // Evaluate buffers if necessary
    if constexpr (dr::is_jit_v<Float>)
        dr::sync_thread();

    stream->write_line("ply");
    if (Struct::host_byte_order() == Struct::ByteOrder::BigEndian)
        stream->write_line("format binary_big_endian 1.0");
    else
        stream->write_line("format binary_little_endian 1.0");

    stream->write_line(tfm::format("element vertex %i", m_vertex_count));
    stream->write_line("property float x");
    stream->write_line("property float y");
    stream->write_line("property float z");

    if (has_vertex_normals()) {
        stream->write_line("property float nx");
        stream->write_line("property float ny");
        stream->write_line("property float nz");
    }

    if (has_vertex_texcoords()) {
        stream->write_line("property float s");
        stream->write_line("property float t");
    }

    for (const auto&[name, attribute]: vertex_attributes)
        for (size_t i = 0; i < attribute.size; ++i)
            stream->write_line(tfm::format("property float %s_%zu", name.c_str(), i));

    stream->write_line(tfm::format("element face %i", m_face_count));
    stream->write_line("property list uchar int vertex_indices");

    for (const auto&[name, attribute]: face_attributes)
        for (size_t i = 0; i < attribute.size; ++i)
            stream->write_line(tfm::format("property float %s_%zu", name.c_str(), i));

    stream->write_line("end_header");

    // Write vertices data
    const InputFloat* position_ptr = vertex_positions.data();
    const InputFloat* normal_ptr   = vertex_normals.data();
    const InputFloat* texcoord_ptr = vertex_texcoords.data();

    std::vector<const InputFloat*> vertex_attributes_ptr;
    for (const auto&[name, attribute]: vertex_attributes)
        vertex_attributes_ptr.push_back(attribute.buf.data());

    for (size_t i = 0; i < m_vertex_count; i++) {
        // Write positions
        stream->write(position_ptr, 3 * sizeof(InputFloat));
        position_ptr += 3;
        // Write normals
        if (has_vertex_normals()) {
            stream->write(normal_ptr, 3 * sizeof(InputFloat));
            normal_ptr += 3;
        }
        // Write texture coordinates
        if (has_vertex_texcoords()) {
            stream->write(texcoord_ptr, 2 * sizeof(InputFloat));
            texcoord_ptr += 2;
        }

        for (size_t j = 0; j < vertex_attributes_ptr.size(); ++j) {
            const auto&[name, attribute] = vertex_attributes[j];
            stream->write(vertex_attributes_ptr[j], attribute.size * sizeof(InputFloat));
            vertex_attributes_ptr[j] += attribute.size;
        }
    }

    const ScalarIndex* face_ptr = faces.data();

    std::vector<const InputFloat*> face_attributes_ptr;
    for (const auto&[name, attribute]: face_attributes)
        face_attributes_ptr.push_back(attribute.buf.data());

    // Write faces data
    uint8_t vertex_indices_count = 3;
    for (size_t i = 0; i < m_face_count; i++) {
        // Write vertex count
        stream->write(&vertex_indices_count, sizeof(uint8_t));

        // Write positions
        stream->write(face_ptr, 3 * sizeof(ScalarIndex));
        face_ptr += 3;

        for (size_t j = 0; j < face_attributes_ptr.size(); ++j) {
            const auto&[name, attribute] = face_attributes[j];
            stream->write(face_attributes_ptr[j], attribute.size * sizeof(InputFloat));
            face_attributes_ptr[j] += attribute.size;
        }
    }
}

MI_VARIANT void Mesh<Float, Spectrum>::recompute_vertex_normals() {
    if (!has_vertex_normals())
        Throw("Storing new normals in a Mesh that didn't have normals at "
              "construction time is not implemented yet.");

    /* Weighting scheme based on "Computing Vertex Normals from Polygonal Facets"
       by Grit Thuermer and Charles A. Wuethrich, JGT 1998, Vol 3 */

    if constexpr (!dr::is_dynamic_v<Float>) {
        size_t invalid_counter = 0;
        std::vector<InputNormal3f> normals(m_vertex_count, dr::zeros<InputNormal3f>());

        for (ScalarSize i = 0; i < m_face_count; ++i) {
            auto fi = face_indices(i);
            Assert(fi[0] < m_vertex_count &&
                   fi[1] < m_vertex_count &&
                   fi[2] < m_vertex_count);

            InputPoint3f v[3] = { vertex_position(fi[0]),
                                  vertex_position(fi[1]),
                                  vertex_position(fi[2]) };

            InputVector3f side_0 = v[1] - v[0],
                          side_1 = v[2] - v[0];
            InputNormal3f n = dr::cross(side_0, side_1);
            InputFloat length_sqr = dr::squared_norm(n);
            if (likely(length_sqr > 0)) {
                n *= dr::rsqrt(length_sqr);

                // Use DrJit to compute the face angles at the same time
                auto side1 = transpose(dr::Array<dr::Packet<InputFloat, 3>, 3>{ side_0, v[2] - v[1], v[0] - v[2] });
                auto side2 = transpose(dr::Array<dr::Packet<InputFloat, 3>, 3>{ side_1, v[0] - v[1], v[1] - v[2] });
                InputVector3f face_angles = unit_angle(dr::normalize(side1), dr::normalize(side2));

                for (size_t j = 0; j < 3; ++j)
                    normals[fi[j]] += n * face_angles[j];
            }
        }

        for (ScalarSize i = 0; i < m_vertex_count; i++) {
            InputNormal3f n = normals[i];
            InputFloat length = dr::norm(n);
            if (likely(length != 0.f)) {
                n /= length;
            } else {
                n = InputNormal3f(1, 0, 0); // Choose some bogus value
                invalid_counter++;
            }

            dr::store(m_vertex_normals.data() + 3 * i, n);
        }

        if (invalid_counter > 0)
            Log(Warn, "\"%s\": computed vertex normals (%i invalid vertices!)",
                m_name, invalid_counter);
    } else {
        // The following is JITed into two separate kernel launches

        // --------------------- Kernel 1 starts here ---------------------
        Vector3u fi = face_indices(dr::arange<UInt32>(m_face_count));

        Vector3f v[3] = { vertex_position(fi[0]),
                          vertex_position(fi[1]),
                          vertex_position(fi[2]) };

        Vector3f n = dr::normalize(dr::cross(v[1] - v[0], v[2] - v[0]));

        Vector3f normals = dr::zeros<Vector3f>(m_vertex_count);
        for (int i = 0; i < 3; ++i) {
            Vector3f d0 = dr::normalize(v[(i + 1) % 3] - v[i]);
            Vector3f d1 = dr::normalize(v[(i + 2) % 3] - v[i]);
            Float face_angle = dr::safe_acos(dr::dot(d0, d1));

            Vector3f nn = n * face_angle;

            for (int j = 0; j < 3; ++j)
                dr::scatter_reduce(ReduceOp::Add, normals[j], nn[j], fi[i]);
        }

        // --------------------- Kernel 2 starts here ---------------------

        normals = dr::normalize(normals);

        // Convert to 32-bit precision
        using JitInputNormal3f = Normal<dr::replace_scalar_t<Float, InputFloat>, 3>;
        JitInputNormal3f input_normals(normals);

        // Disconnect the vertex normal buffer from any pre-existing AD
        // graph. Otherwise an AD graph might be unnecessarily retained
        // here, despite the following lines re-initializing the normals.
        dr::disable_grad(m_vertex_normals);

        UInt32 ni = dr::arange<UInt32>(m_vertex_count) * 3;
        for (uint32_t i = 0; i < 3; ++i)
            dr::scatter(m_vertex_normals, input_normals[i], ni + i);

        dr::eval(m_vertex_normals);
    }
}

MI_VARIANT void Mesh<Float, Spectrum>::recompute_bbox() {
    auto&& vertex_positions = dr::migrate(m_vertex_positions, AllocType::Host);
    if constexpr (dr::is_jit_v<Float>)
        dr::sync_thread();

    const InputFloat *ptr = vertex_positions.data();

    m_bbox.reset();
    for (ScalarSize i = 0; i < m_vertex_count; ++i)
        m_bbox.expand(
            ScalarPoint3f(ptr[3 * i + 0], ptr[3 * i + 1], ptr[3 * i + 2]));
}

MI_VARIANT void Mesh<Float, Spectrum>::build_pmf() {
    std::lock_guard<std::mutex> lock(m_mutex);
    dr::scoped_symbolic_independence<Float> guard{};

    if (m_face_count == 0)
        Throw("Cannot create sampling table for an empty mesh: %s", to_string());

    if constexpr (!dr::is_jit_v<Float>) {
        if (!m_area_pmf.empty())
            return; // already built!

        auto &&vertex_positions =
            dr::migrate(m_vertex_positions, AllocType::Host);
        auto &&faces = dr::migrate(m_faces, AllocType::Host);
        if constexpr (dr::is_jit_v<Float>)
            dr::sync_thread();

        const InputFloat *pos_p  = vertex_positions.data();
        const ScalarIndex *idx_p = faces.data();

        std::vector<ScalarFloat> table(m_face_count);
        for (ScalarIndex i = 0; i < m_face_count; i++) {
            ScalarPoint3u idx = dr::load<ScalarPoint3u>(idx_p + 3 * i);

            ScalarPoint3f p0 = dr::load<InputPoint3f>(pos_p + 3 * idx.x()),
                          p1 = dr::load<InputPoint3f>(pos_p + 3 * idx.y()),
                          p2 = dr::load<InputPoint3f>(pos_p + 3 * idx.z());

            table[i] = .5f * dr::norm(dr::cross(p1 - p0, p2 - p0));
        }

        m_area_pmf = DiscreteDistribution<Float>(table.data(), m_face_count);
    } else {
        Vector3u v_idx = face_indices(dr::arange<UInt32>(m_face_count));
        Point3f p0 = vertex_position(v_idx[0]), p1 = vertex_position(v_idx[1]),
                p2 = vertex_position(v_idx[2]);

        Float face_surface_area = .5f * dr::norm(dr::cross(p1 - p0, p2 - p0));

        m_area_pmf = DiscreteDistribution<Float>(dr::detach(face_surface_area));
    }
}

MI_VARIANT void Mesh<Float, Spectrum>::build_directed_edges() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_face_count == 0)
        Throw("Cannot create directed edges for an empty mesh: %s", to_string());

    auto&& faces = dr::migrate(m_faces, AllocType::Host);
    if constexpr (dr::is_array_v<Float>)
        dr::sync_thread();

    std::vector<ScalarIndex> V2E(m_vertex_count, m_invalid_dedge);
    std::vector<ScalarIndex> E2E(m_face_count * 3, m_invalid_dedge);

    /* For an edge e1 = (v1, v2), tmp is defined as:
    /     tmp[e1].first  = v2,
    /     tmp[e1].second = (next edge e_k that also starts from v1) or (m_invalid_dedge)
    */
    std::vector<std::pair<ScalarIndex, ScalarIndex>> tmp(m_face_count * 3);

    // 1. Fill `tmp` and `V2E`
    const ScalarIndex *face_data = faces.data();
    for (ScalarIndex f = 0; f < m_face_count; f++) {
        ScalarPoint3u triangle_indices =
            dr::load<ScalarPoint3u>(face_data + 3 * f);
        for (ScalarIndex i = 0; i < 3; i++) {
            ScalarIndex idx_cur = triangle_indices[i],
                        idx_nxt = triangle_indices[(i + 1) % 3],
                        edge_id = 3 * f + i;
            if (idx_cur == idx_nxt)
                continue;

            tmp[edge_id] = std::make_pair(idx_nxt, m_invalid_dedge);
            if (V2E[idx_cur] != m_invalid_dedge) {
                ScalarIndex last_edge_idx = V2E[idx_cur];

                while (tmp[last_edge_idx].second != m_invalid_dedge)
                    last_edge_idx = tmp[last_edge_idx].second;

                if (tmp[last_edge_idx].second == m_invalid_dedge)
                    tmp[last_edge_idx].second = edge_id;
            } else {
                V2E[idx_cur] = edge_id;
            }
        }
    }

    // 2. Manifold check & assign `E2E`
    std::vector<bool> non_manifold(m_vertex_count, false);
    for (ScalarIndex f = 0; f < m_face_count; f++) {
        ScalarPoint3u tri = dr::load<ScalarPoint3u>(face_data + 3 * f);
        for (ScalarIndex i = 0; i < 3; i++) {
            ScalarIndex idx_cur = tri[i],
                        idx_nxt = tri[(i + 1) % 3],
                        edge_id_cur = 3 * f + i;
            if (idx_cur == idx_nxt)
                continue;

            ScalarIndex it = V2E[idx_nxt], edge_id_opp = m_invalid_dedge;
            while (it != m_invalid_dedge) {
                if (tmp[it].first == idx_cur) {
                    if (edge_id_opp == m_invalid_dedge) {
                        edge_id_opp = it;
                    } else {
                        non_manifold[idx_cur] = true;
                        non_manifold[idx_nxt] = true;
                        edge_id_opp           = m_invalid_dedge;
                        break;
                    }
                }
                it = tmp[it].second;
            }

            if (edge_id_opp != m_invalid_dedge && edge_id_cur < edge_id_opp) {
                E2E[edge_id_cur] = edge_id_opp;
                E2E[edge_id_opp] = edge_id_cur;
            }
        }
    }

    // 3. Log
    ScalarIndex non_manifold_count = 0;
    std::vector<bool> boundary(m_vertex_count, false);
    for (ScalarIndex i = 0; i < m_vertex_count; i++) {
        if (non_manifold[i]) {
            non_manifold_count++;
            continue;
        }
    }

    if (non_manifold_count > 0)
        Log(Warn,
            "Mesh::build_directed_edges(): there are %d non-manifold vertices in the "
            "follwing mesh: %s",
            non_manifold_count, to_string());

    m_E2E = dr::load<DynamicBuffer<UInt32>>(E2E.data(), m_face_count * 3);
    m_E2E_outdated = false;
}

/**
 * \brief Picks a vertex index from \c vec using \c offset
 *
 * This helper functions is used to pick a vertex index from a set of 3 vertex
 * indices corresponding to a single face. The \c offset parameter is the directed
 * edge index which is used to pick the vertex. The picked vertex is the starting
 * vertex of the directed edge.
 */
template <typename Index>
MI_INLINE auto pick_vertex(const dr::Array<dr::uint32_array_t<Index>, 3> &vec, const Index &offset) {
    Index dim_mod = dr::imod(offset, 3u);
    Index res = dr::select(dim_mod == 1u, vec[1], vec[0]);
    res = dr::select(dim_mod == 2u, vec[2], res);
    return res;
}

MI_VARIANT void
Mesh<Float, Spectrum>::build_indirect_silhouette_distribution() {
    UInt32 dedge = dr::arange<UInt32>(m_face_count * 3),
           dedge_oppo = opposite_dedge(dedge);
    Mask boundary = (dedge_oppo == m_invalid_dedge);
    // One edge can be represented by two dedge indices, we use the smaller index
    Mask valid = (dedge_oppo > dedge) & !boundary;

    auto [face_idx, edge_idx] = dr::idivmod(dedge, 3u);
    auto [face_idx_oppo, edge_idx_oppo] = dr::idivmod(dedge_oppo, 3u);

    Normal3f n_curr = face_normal(face_idx, valid),
             n_oppo = face_normal(face_idx_oppo, valid);
    valid &= dr::dot(n_curr, n_oppo) < 1.f; // Flat surfaces are not on the silhouette

    Vector3u v_idx_oppo = face_indices(face_idx_oppo, valid);
    Point3f p0 = vertex_position(pick_vertex(v_idx_oppo, edge_idx_oppo     ), valid),
            p1 = vertex_position(pick_vertex(v_idx_oppo, edge_idx_oppo + 1u), valid),
            p2 = vertex_position(pick_vertex(v_idx_oppo, edge_idx_oppo + 2u), valid);

    if (m_bsdf && !has_flag(m_bsdf->flags(), BSDFFlags::BackSide)) {
        // Concave surfaces do not contribute to visibility contours.
        Vector3f v_oppo = dr::normalize(p2 - p1);
        valid &= dr::dot(n_curr, v_oppo) < 0.f;
    }

    Vector3u v_indices_curr = face_indices(face_idx, boundary);
    dr::masked(p0, boundary) = vertex_position(pick_vertex(v_indices_curr, edge_idx     ), boundary);
    dr::masked(p1, boundary) = vertex_position(pick_vertex(v_indices_curr, edge_idx + 1u), boundary);

    Float weight = dr::zeros<Float>(m_face_count * 3);
    dr::masked(weight, valid || boundary) = dr::detach(dr::norm(p1 - p0));

    m_sil_dedge_pmf = DiscreteDistribution<Float>(weight);
}

MI_VARIANT
ref<Mesh<Float, Spectrum>>
Mesh<Float, Spectrum>::merge(const Mesh *other) const {
    if (other->emitter() != m_emitter || other->sensor() != m_sensor ||
        other->bsdf() != m_bsdf ||
        other->interior_medium() != m_interior_medium ||
        other->exterior_medium() != m_exterior_medium ||
        other->has_vertex_normals() != has_vertex_normals() ||
        other->has_vertex_texcoords() != has_vertex_texcoords() ||
        other->has_face_normals() != has_face_normals() ||
        other->has_mesh_attributes() || has_mesh_attributes())
        Throw("Mesh::merge(): the two meshes are incompatible (%s and %s)!",
              to_string(), other->to_string());

    Properties props;
    if (m_bsdf)
        props.set_object("bsdf", (Object *) m_bsdf.get());
    if (m_interior_medium)
        props.set_object("interior", (Object *) m_interior_medium.get());
    if (m_exterior_medium)
        props.set_object("exterior", (Object *) m_exterior_medium.get());
    if (m_sensor)
        props.set_object("sensor", (Object *) m_sensor.get());
    if (m_emitter)
        props.set_object("emitter", (Object *) m_emitter.get());
    props.set_bool("face_normals", m_face_normals);

    ref<Mesh> result = new Mesh(
        m_name + " + " + other->m_name, m_vertex_count + other->vertex_count(),
        m_face_count + other->face_count(), props, has_vertex_normals(),
        has_vertex_texcoords());

    result->m_vertex_positions =
        dr::concat(m_vertex_positions, other->m_vertex_positions);

    if (has_vertex_normals())
        result->m_vertex_normals =
            dr::concat(m_vertex_normals, other->m_vertex_normals);

    if (has_vertex_texcoords())
        result->m_vertex_texcoords =
            dr::concat(m_vertex_texcoords, other->m_vertex_texcoords);

    result->m_faces = dr::concat(m_faces, other->m_faces);
    result->m_bbox = m_bbox;
    result->m_bbox.expand(other->m_bbox);

    if constexpr (dr::is_jit_v<Float>) {
        UInt32 threshold = dr::opaque<UInt32>(face_count() * 3),
               offset    = dr::opaque<UInt32>(vertex_count()),
               index     = dr::arange<UInt32>(result->face_count() * 3);

        result->m_faces = dr::select(index < threshold, result->m_faces,
                                     result->m_faces + offset);

        dr::eval(result->m_faces, result->m_vertex_positions,
                 result->m_vertex_normals, result->m_vertex_texcoords);
    } else {
        uint32_t  offset = vertex_count(),
                 *ptr    = result->m_faces.data() + face_count() * 3;
        for (size_t i = 0; i < other->face_count() * 3; ++i)
            *ptr++ += offset;
    }

    result->initialize();

    return result;
}

MI_VARIANT void Mesh<Float, Spectrum>::build_parameterization() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_parameterization)
        return; // already built!

    if (!has_vertex_texcoords())
        Throw("eval_parameterization(): mesh does not have UV coordinates!");

    Properties props;
    ref<Mesh> mesh =
        new Mesh(m_name + "_param", m_vertex_count, m_face_count,
                 props, false, false);
    mesh->m_faces = m_faces;

    auto&& vertex_texcoords = dr::migrate(m_vertex_texcoords, AllocType::Host);
    if constexpr (dr::is_jit_v<Float>)
        dr::sync_thread();

    const InputFloat *ptr = vertex_texcoords.data();

    std::vector<InputFloat> pos_out(m_vertex_count * 3);
    ScalarBoundingBox3f bbox;
    for (size_t i = 0; i < m_vertex_count; ++i) {
        InputFloat u = ptr[2*i + 0],
                   v = ptr[2*i + 1];
        pos_out[i*3 + 0] = u;
        pos_out[i*3 + 1] = v;
        pos_out[i*3 + 2] = 0.f;
        bbox.expand(ScalarPoint3f(u, v, 0.f));
    }
    mesh->m_vertex_positions =
        dr::load<FloatStorage>(pos_out.data(), m_vertex_count * 3);
    mesh->m_bbox = bbox;
    mesh->initialize();

    props.set_object("mesh", mesh.get());

    if (m_scene)
        props.set_object("parent_scene", m_scene);

    m_parameterization = new Scene<Float, Spectrum>(props);
}

MI_VARIANT typename Mesh<Float, Spectrum>::ScalarSize
Mesh<Float, Spectrum>::primitive_count() const {
    return face_count();
}

MI_VARIANT Float Mesh<Float, Spectrum>::surface_area() const {
    ensure_pmf_built();
    return m_area_pmf.sum();
}

// =============================================================
//! @{ \name Surface sampling routines
// =============================================================

MI_VARIANT typename Mesh<Float, Spectrum>::PositionSample3f
Mesh<Float, Spectrum>::sample_position(Float time, const Point2f &sample_, Mask active) const {
    ensure_pmf_built();

    using Index = dr::replace_scalar_t<Float, ScalarIndex>;
    Index face_idx;
    Point2f sample = sample_;

    std::tie(face_idx, sample.y()) =
        m_area_pmf.sample_reuse(sample.y(), active);

    Vector3u fi = face_indices(face_idx, active);

    Point3f p0 = vertex_position(fi[0], active),
            p1 = vertex_position(fi[1], active),
            p2 = vertex_position(fi[2], active);

    Vector3f e0 = p1 - p0, e1 = p2 - p0;
    Point2f b = warp::square_to_uniform_triangle(sample);

    PositionSample3f ps;
    ps.p     = dr::fmadd(e0, b.x(), dr::fmadd(e1, b.y(), p0));
    ps.time  = time;
    ps.pdf   = m_area_pmf.normalization();
    ps.delta = false;

    if (has_vertex_texcoords()) {
        Point2f uv0 = vertex_texcoord(fi[0], active),
                uv1 = vertex_texcoord(fi[1], active),
                uv2 = vertex_texcoord(fi[2], active);

        ps.uv = dr::fmadd(uv0, (1.f - b.x() - b.y()),
                          dr::fmadd(uv1, b.x(), uv2 * b.y()));
    } else {
        ps.uv = b;
    }

    if (has_vertex_normals()) {
        Normal3f n0 = vertex_normal(fi[0], active),
                 n1 = vertex_normal(fi[1], active),
                 n2 = vertex_normal(fi[2], active);

        ps.n = dr::fmadd(n0, (1.f - b.x() - b.y()),
                         dr::fmadd(n1, b.x(), n2 * b.y()));
    } else {
        ps.n = dr::cross(e0, e1);
    }

    ps.n = dr::normalize(ps.n);

    if (m_flip_normals)
        ps.n = -ps.n;

    return ps;
}

MI_VARIANT

typename Mesh<Float, Spectrum>::SurfaceInteraction3f
Mesh<Float, Spectrum>::eval_parameterization(const Point2f &uv,
                                             uint32_t ray_flags,
                                             Mask active) const {
    if (!m_parameterization)
        const_cast<Mesh *>(this)->build_parameterization();

    Ray3f ray(Point3f(uv.x(), uv.y(), -1), Vector3f(0, 0, 1), 0, Wavelength(0));

    PreliminaryIntersection3f pi =
        m_parameterization->ray_intersect_preliminary(
            ray, /* coherent = */ true, active);
    active &= pi.is_valid();

    if (dr::none_or<false>(active))
        return dr::zeros<SurfaceInteraction3f>();

    SurfaceInteraction3f si =
        compute_surface_interaction(ray, pi, ray_flags, 0, active);
    si.finalize_surface_interaction(pi, ray, ray_flags, active);

    return si;
}

MI_VARIANT Float Mesh<Float, Spectrum>::pdf_position(const PositionSample3f &, Mask) const {
    ensure_pmf_built();
    return m_area_pmf.normalization();
}

//! @}
// =============================================================

// =============================================================
//! @{ \name Silhouette sampling routines and other utilities
// =============================================================

MI_VARIANT typename Mesh<Float, Spectrum>::SilhouetteSample3f
Mesh<Float, Spectrum>::sample_silhouette(const Point3f &sample_,
                                         uint32_t flags,
                                         Mask active) const {
    MI_MASK_ARGUMENT(active);

    if (!has_flag(flags, DiscontinuityFlags::PerimeterType) || m_E2E_outdated)
        return dr::zeros<SilhouetteSample3f>();

    SilhouetteSample3f ss = dr::zeros<SilhouetteSample3f>();

    /// Sample a point on one of the edges
    UInt32 dedge;
    Float sample_x;
    Float pmf_edge;
    std::tie(dedge, sample_x, pmf_edge) =
        m_sil_dedge_pmf.sample_reuse_pmf(sample_.x(), active);
    Point3f sample(sample_x, sample_.y(), sample_.z());
    active &= (pmf_edge != 0.f);

    auto [face_idx, edge_idx] = dr::idivmod(dedge, 3u);
    Vector3u v_idx = face_indices(face_idx, active);
    Point3f p0 = vertex_position(pick_vertex(v_idx, edge_idx     ), active),
            p1 = vertex_position(pick_vertex(v_idx, edge_idx + 1u), active),
            p2 = vertex_position(pick_vertex(v_idx, edge_idx + 2u), active);

    ss.p = dr::lerp(p0, p1, sample.x());

    // Face local barycentric UV coordinates
    ss.uv = dr::select(edge_idx == 0u,
                       Point2f(sample.x(), 0.f),
                       Point2f(1 - sample.x(), sample.x()));
    ss.uv = dr::select(edge_idx == 2u,
                       Point2f(0.f, 1 - sample.x()),
                       ss.uv);

    /// Sample a tangential direction at the point
    Normal3f n_curr = face_normal(face_idx, active);

    UInt32 dedge_oppo = opposite_dedge(dedge, active);
    UInt32 face_idx_oppo = dr::idiv(dedge_oppo, 3u);
    Mask has_opposite = (dedge_oppo != m_invalid_dedge) & active;
    Normal3f n_oppo = face_normal(face_idx_oppo, has_opposite);

    bool is_lune = has_flag(flags, DiscontinuityFlags::DirectionLune);
    bool is_sphere = has_flag(flags, DiscontinuityFlags::DirectionSphere);

    // Flip normals if they define a concave surface
    Vector3f v_oppo = dr::normalize(p2 - p1);
    Mask concave = dr::dot(n_curr, v_oppo) > 0.f;
    dr::masked(n_curr, concave & has_opposite) = -n_curr;
    dr::masked(n_oppo, concave & has_opposite) = -n_oppo;

    if (is_lune) {
        ss.d = warp::square_to_uniform_spherical_lune(
            Point2f(dr::tail<2>(sample)), n_curr, n_oppo);
        ss.pdf =
            warp::square_to_uniform_spherical_lune_pdf(ss.d, n_curr, n_oppo);

        // For boundary edges we sample the entire sphere
        dr::masked(ss.d, !has_opposite) =
            warp::square_to_uniform_sphere(Point2f(dr::tail<2>(sample)));
        dr::masked(ss.pdf, !has_opposite) =
            warp::square_to_uniform_sphere_pdf(ss.d);
    } else if (is_sphere) {
        ss.d = warp::square_to_uniform_sphere(Point2f(dr::tail<2>(sample)));
        ss.pdf = warp::square_to_uniform_sphere_pdf(ss.d);
    } else {
        Throw("Mesh::sample_silhouette(): invalid direction encoding!");
    }

    /// Fill other fields
    ss.discontinuity_type = (uint32_t) DiscontinuityFlags::PerimeterType;
    ss.flags = flags;

    ss.silhouette_d = dr::normalize(p1 - p0);
    ss.n = dr::normalize(dr::cross(ss.d, ss.silhouette_d));
    Vector3f inward_dir = p2 - ss.p;
    dr::masked(ss.n, dr::dot(ss.n, inward_dir) > 0.f) *= -1.f;

    dr::masked(ss.pdf, !active) = 0.f;
    // Check that direction is actually a boundary segment
    Mask valid = ((dr::dot(ss.d, n_curr) * dr::dot(ss.d, n_oppo) < 0.f) ||
                  !has_opposite) && active;
    ss.pdf = dr::select(valid, ss.pdf, 0.f);
    dr::masked(ss.pdf, valid) *= dr::rcp(dr::norm(p0 - p1)) * pmf_edge;

    ss.foreshortening = dr::norm(dr::cross(ss.silhouette_d, ss.d));
    ss.projection_index = edge_idx;
    ss.prim_index = face_idx;
    ss.shape = this;
    ss.offset = 0.f;

    // Mark failed samples
    Mask failed = (ss.pdf == 0.f) || !active;
    dr::masked(ss, failed) = dr::zeros<SilhouetteSample3f>();

    return ss;
}

MI_VARIANT typename Mesh<Float, Spectrum>::Point3f
Mesh<Float, Spectrum>::invert_silhouette_sample(const SilhouetteSample3f &ss,
                                                Mask active_) const {
    // Do not trace this function if it's not differentiated
    if (m_E2E_outdated)
        return dr::zeros<Point3f>();

    // Safley ignore invalid boundary segments
    Mask active =
        active_ && (ss.discontinuity_type ==
                           (uint32_t) DiscontinuityFlags::PerimeterType);

    UInt32 dedge_curr = ss.prim_index * 3u + ss.projection_index,
           dedge_oppo = opposite_dedge(dedge_curr, active);

    // One edge can be represented by two dedge indices, we use the smaller index
    Mask swap = dedge_curr > dedge_oppo;
    UInt32 dedge_curr_tmp = dedge_curr;
    dr::masked(dedge_curr, swap) = dedge_oppo;
    dr::masked(dedge_oppo, swap) = dedge_curr_tmp;

    Mask has_opposite = (dedge_oppo != m_invalid_dedge) && active;
    Normal3f n_curr = face_normal(dr::idiv(dedge_curr, 3u), active),
             n_oppo = face_normal(dr::idiv(dedge_oppo, 3u), has_opposite);

    Point3f sample = dr::zeros<Point3f>(dr::width(ss));
    Float pmf = m_sil_dedge_pmf.eval_pmf(dedge_curr, active),
          cdf = m_sil_dedge_pmf.eval_cdf(dedge_curr, active);

    // Do not use `ss.prim_index`, because we might have swapped
    UInt32 face_idx, edge_idx;
    std::tie(face_idx, edge_idx) = dr::idivmod(dedge_curr, 3u);
    Vector3u fi = face_indices(face_idx, active);
    Point3f p0 = vertex_position(pick_vertex(fi, edge_idx     ), active),
            p1 = vertex_position(pick_vertex(fi, edge_idx + 1u), active),
            p2 = vertex_position(pick_vertex(fi, edge_idx + 2u), active);
    Float alpha = dr::norm(ss.p - p0) * dr::rcp(dr::norm(p1 - p0));

    // We sacrifice the last bit of precision to avoid numerical issues
    alpha = dr::clip(alpha, dr::Epsilon<Float>, 1.f - dr::Epsilon<Float>);

    dr::masked(sample.x(), active) =
        (cdf + (alpha - 1.f) * pmf) * m_sil_dedge_pmf.normalization();

    Mask is_lune = has_flag(ss.flags, DiscontinuityFlags::DirectionLune);
    Mask is_sphere = has_flag(ss.flags, DiscontinuityFlags::DirectionSphere);

    // Sphere sampling is used for boundary edges
    is_lune &= has_opposite;
    is_sphere |= !has_opposite;

    // Flip normals if they define a concave surface
    Vector3f v_oppo = dr::normalize(p2 - p1);
    Mask concave = dr::dot(n_curr, v_oppo) > 0.f;
    dr::masked(n_curr, concave & has_opposite) = -n_curr;
    dr::masked(n_oppo, concave & has_opposite) = -n_oppo;

    Point2f sample_yz_lune = warp::uniform_spherical_lune_to_square(ss.d, n_curr, n_oppo);
    Point2f sample_yz_sphere = warp::uniform_sphere_to_square(ss.d);

    dr::masked(sample.y(), is_lune) = sample_yz_lune.x();
    dr::masked(sample.z(), is_lune) = sample_yz_lune.y();
    dr::masked(sample.y(), is_sphere) = sample_yz_sphere.x();
    dr::masked(sample.z(), is_sphere) = sample_yz_sphere.y();

    return sample;
}

MI_VARIANT typename Mesh<Float, Spectrum>::Point3f
Mesh<Float, Spectrum>::differential_motion(const SurfaceInteraction3f &si,
                                           Mask active) const {
    MI_MASK_ARGUMENT(active);

    if constexpr (!dr::is_diff_v<Float>) {
        return si.p;
    } else {
        Point2f uv = dr::detach(si.uv);

        Vector3u fi = face_indices(si.prim_index, active);
        Point3f p0  = vertex_position(fi[0], active),
                p1  = vertex_position(fi[1], active),
                p2  = vertex_position(fi[2], active);

        // Barycentric coordinates
        Float b = uv.x(), c = uv.y(), a = 1.f - b - c;

        Point3f p_diff = dr::fmadd(p0, a, dr::fmadd(p1, b, p2 * c));

        return dr::replace_grad(si.p, p_diff);
    }
}

MI_VARIANT typename Mesh<Float, Spectrum>::SilhouetteSample3f
Mesh<Float, Spectrum>::primitive_silhouette_projection(const Point3f &viewpoint,
                                                       const SurfaceInteraction3f &si,
                                                       uint32_t flags,
                                                       Float sample,
                                                       Mask active) const {
    MI_MASK_ARGUMENT(active);

    if (!has_flag(flags, DiscontinuityFlags::PerimeterType))
        return dr::zeros<SilhouetteSample3f>();

    /* To obtain the silhouette sample on an edge, we do not project `si.p` to
    the nearest edge, instead we randomly sample a point on any silhouette edge.
    This ensures that the triangle corners do not receive minimal samples. */

    // Directed edge data structure was not prepared,
    // e.g. due to the shape not being differentiated.
    if (dr::width(m_E2E) == 0)
        return dr::zeros<SilhouetteSample3f>();

    Vector3u fi = face_indices(si.prim_index, active);
    Vector3f p0 = vertex_position(fi[0], active),
             p1 = vertex_position(fi[1], active),
             p2 = vertex_position(fi[2], active);
    // Face geometry normals of the current and three neighboring triangles
    UInt32 dedge_oppo_0 = opposite_dedge(si.prim_index * 3u     , active),
           dedge_oppo_1 = opposite_dedge(si.prim_index * 3u + 1u, active),
           dedge_oppo_2 = opposite_dedge(si.prim_index * 3u + 2u, active);
    Mask boundary_0 = active && (dedge_oppo_0 == m_invalid_dedge),
         boundary_1 = active && (dedge_oppo_1 == m_invalid_dedge),
         boundary_2 = active && (dedge_oppo_2 == m_invalid_dedge);
    UInt32 prim_idx_0 = dr::select(boundary_0, si.prim_index, dr::idiv(dedge_oppo_0, 3u)),
           prim_idx_1 = dr::select(boundary_1, si.prim_index, dr::idiv(dedge_oppo_1, 3u)),
           prim_idx_2 = dr::select(boundary_2, si.prim_index, dr::idiv(dedge_oppo_2, 3u));
    Normal3f normal_0 = face_normal(prim_idx_0, active && !boundary_0),
             normal_1 = face_normal(prim_idx_1, active && !boundary_1),
             normal_2 = face_normal(prim_idx_2, active && !boundary_2);

    Normal3f normal = face_normal(si.prim_index, active);

    // Compute the "viewing" angle of three neighboring triangles
    Vector3f ray_d_0 = dr::normalize(p0 - viewpoint),
             ray_d_1 = dr::normalize(p1 - viewpoint),
             ray_d_2 = dr::normalize(p2 - viewpoint);

    Vector3f cos_theta_oppo;
    cos_theta_oppo.x() = dr::dot(ray_d_1, normal_0) * dr::sign(dr::dot(ray_d_1, normal));
    cos_theta_oppo.y() = dr::dot(ray_d_2, normal_1) * dr::sign(dr::dot(ray_d_2, normal));
    cos_theta_oppo.z() = dr::dot(ray_d_0, normal_2) * dr::sign(dr::dot(ray_d_0, normal));

    // Boundary edges are always silhouettes
    dr::masked(cos_theta_oppo.x(), boundary_0) = -1.f;
    dr::masked(cos_theta_oppo.y(), boundary_1) = -1.f;
    dr::masked(cos_theta_oppo.z(), boundary_2) = -1.f;

    Vector3f weight;
    Mask failed_proj;
    SilhouetteSample3f ss = dr::zeros<SilhouetteSample3f>();

    if (has_flag(flags, DiscontinuityFlags::HeuristicWalk)) {
        /// Project to any edge with heuristic probability. Note that this flag
        /// modifies `ss.prim_index` directly to the selected new triangle.
        weight = dr::safe_acos(cos_theta_oppo);

        // All silhouette edges are equally good regardless of the angle
        // Note that we still consider non-silhouette edges even if there is at
        // least one neighboring silhouette edge. This can alleviate issues
        // with small "bumpy" features on the mesh.
        const float max_weight = dr::Pi<ScalarFloat> / 2.f;
        weight[0] = dr::select(cos_theta_oppo[0] <= 0.f, max_weight, weight[0]);
        weight[1] = dr::select(cos_theta_oppo[1] <= 0.f, max_weight, weight[1]);
        weight[2] = dr::select(cos_theta_oppo[2] <= 0.f, max_weight, weight[2]);

        // In case the weights are too small
        Float min_weight = dr::deg_to_rad(1.f);
        weight[0] = dr::maximum(weight[0], min_weight);
        weight[1] = dr::maximum(weight[1], min_weight);
        weight[2] = dr::maximum(weight[2], min_weight);

        Float sum = weight[0] + weight[1] + weight[2];
        weight /= sum;

        ss.projection_index = dr::select(sample >= weight[0], 1u, 0u);
        ss.projection_index = dr::select(sample >= weight[0] + weight[1], 2u, ss.projection_index);

        ss.prim_index = dr::select(sample >= weight[0], prim_idx_1, prim_idx_0);
        ss.prim_index = dr::select(sample >= weight[0] + weight[1], prim_idx_2, ss.prim_index);

        failed_proj = ((ss.projection_index == 0u) && (cos_theta_oppo[0] > 0.f)) ||
                      ((ss.projection_index == 1u) && (cos_theta_oppo[1] > 0.f)) ||
                      ((ss.projection_index == 2u) && (cos_theta_oppo[2] > 0.f));
    } else {
        /// Project to any silhouette edge with equal probability.
        weight.x() = dr::select(cos_theta_oppo.x() < 0.f, 1.f, 0.f);
        weight.y() = dr::select(cos_theta_oppo.y() < 0.f, 1.f, 0.f);
        weight.z() = dr::select(cos_theta_oppo.z() < 0.f, 1.f, 0.f);

        Float sum = weight[0] + weight[1] + weight[2];

        // If none of the edges are on the silhouette, pick one uniformly
        failed_proj = (sum == 0.f);
        dr::masked(weight, failed_proj) = Vector3f(1.f, 1.f, 1.f);
        dr::masked(sum, failed_proj) = 3.f;
        weight /= sum;

        ss.prim_index = si.prim_index;

        ss.projection_index = dr::select(sample >= weight[0], 1u, 0u);
        ss.projection_index = dr::select(sample >= weight[0] + weight[1], 2u, ss.projection_index);
    }

    // Reuse sample
    sample = dr::select(
        ss.projection_index == 0u,
        sample / weight[0],
        sample);
    sample = dr::select(
        ss.projection_index == 1u,
        (sample - weight[0]) / weight[1],
        sample);
    sample = dr::select(
        ss.projection_index == 2u,
        (sample - weight[1] - weight[0]) / weight[2],
        sample);

    // Sample a point on the selected edge
    ss.p = dr::select(
        ss.projection_index == 1u,
        dr::lerp(p1, p2, sample), dr::lerp(p0, p1, sample)
    );
    ss.p = dr::select(
        ss.projection_index == 2u,
        dr::lerp(p2, p0, sample), ss.p
    );

    ss.d = dr::normalize(ss.p - viewpoint);
    ss.shape = this;

    ss.discontinuity_type = dr::select(
        active & !failed_proj,
        (uint32_t) DiscontinuityFlags::PerimeterType,
        (uint32_t) DiscontinuityFlags::Empty);

    return ss;
}

MI_VARIANT
std::tuple<DynamicBuffer<typename CoreAliases<Float>::UInt32>,
           DynamicBuffer<Float>>
Mesh<Float, Spectrum>::precompute_silhouette(
    const ScalarPoint3f &viewpoint) const {
    if constexpr (!dr::is_jit_v<Float>) {
        using Vec3f = ScalarVector3f;
        using Pt3f  = ScalarPoint3f;

        auto &&vertex_positions =
            dr::migrate(m_vertex_positions, AllocType::Host);
        auto &&faces = dr::migrate(m_faces, AllocType::Host);
        auto &&E2E   = dr::migrate(m_E2E, AllocType::Host);

        if constexpr (dr::is_array_v<Float>)
            dr::sync_thread();

        const InputFloat *V          = vertex_positions.data();
        const ScalarIndex *E2E_data  = E2E.data();
        const ScalarIndex *face_data = faces.data();

        ScalarIndex prim_count = 0u;
        std::vector<ScalarIndex> indices(m_face_count * 3u);
        std::vector<ScalarFloat> weight(m_face_count * 3u);
        ScalarFloat weight_sum = 0.f;

        for (ScalarIndex f = 0; f < m_face_count; f++) {
            ScalarPoint3u idx = dr::load<ScalarPoint3u>(face_data + 3 * f);
            Pt3f v0           = dr::load<Pt3f>(V + 3 * idx.x());
            Pt3f v1           = dr::load<Pt3f>(V + 3 * idx.y());
            Pt3f v2           = dr::load<Pt3f>(V + 3 * idx.z());
            Vec3f n           = dr::normalize(dr::cross(v1 - v0, v2 - v0));

            Vec3f to_v0 = dr::normalize(v0 - viewpoint);
            Vec3f to_v1 = dr::normalize(v1 - viewpoint);
            Vec3f to_v2 = dr::normalize(v2 - viewpoint);

            auto check_edge = [&](const ScalarIndex dedge_curr,
                                  const Vec3f &dir1,
                                  const Vec3f &dir2) -> void {
                ScalarIndex dedge_oppo =
                    dr::load<ScalarIndex>(E2E_data + dedge_curr);
                bool valid = false;

                if (dedge_oppo == m_invalid_dedge) {
                    valid = true;
                } else if (dedge_oppo > dedge_curr) {
                    ScalarIndex face_index_oppo = dr::idiv(dedge_oppo, 3u);
                    ScalarPoint3u v_idx_oppo    = dr::load<ScalarPoint3u>(
                        face_data + 3 * face_index_oppo);

                    Pt3f v0_oppo = dr::load<Pt3f>(V + 3 * v_idx_oppo.x());
                    Pt3f v1_oppo = dr::load<Pt3f>(V + 3 * v_idx_oppo.y());
                    Pt3f v2_oppo = dr::load<Pt3f>(V + 3 * v_idx_oppo.z());
                    Vec3f n_oppo = dr::normalize(
                        dr::cross(v1_oppo - v0_oppo, v2_oppo - v0_oppo));

                    if (dr::dot(dir1, n) * dr::dot(dir1, n_oppo) <= 0.f &&
                        dr::abs(dr::dot(n, n_oppo)) < 1.f) {
                        valid = true;
                    }
                }

                if (valid) {
                    indices[prim_count] = dedge_curr;

                    // The arclength weight is not perfect for perspective
                    // cameras. But it is a close approximation.
                    weight[prim_count] = unit_angle(dir1, dir2);
                    weight_sum += weight[prim_count];
                    prim_count++;
                }
            };

            check_edge(f * 3u, to_v0, to_v1);
            check_edge(f * 3u + 1u, to_v1, to_v2);
            check_edge(f * 3u + 2u, to_v2, to_v0);
        }

        indices.resize(prim_count);
        weight.resize(prim_count);

        DynamicBuffer<UInt32> out_indices = dr::load<UInt32>(indices.data(), indices.size());
        DynamicBuffer<Float> out_weights= dr::load<Float>(weight.data(), weight.size());

        return std::make_tuple(out_indices, out_weights);
    } else {
        UInt32 dedge_curr = dr::arange<UInt32>(m_face_count * 3);
        auto [face_idx, e] = dr::idivmod(dedge_curr, 3u);
        Vector3u fi = face_indices(face_idx);
        Point3f p0 = vertex_position(pick_vertex(fi, e + 0u)),
                p1 = vertex_position(pick_vertex(fi, e + 1u));

        Normal3f n = face_normal(face_idx);
        Vector3f to_p0 = dr::normalize(p0 - viewpoint);
        Vector3f to_p1 = dr::normalize(p1 - viewpoint);

        // The arclength weight is not perfect for perspective
        // cameras. But it is a close approximation.
        Float weight = unit_angle(to_p0, to_p1);

        UInt32 dedge_oppo = opposite_dedge(dedge_curr);
        Mask has_opposite = (dedge_oppo != m_invalid_dedge);

        auto face_idx_oppo = dr::idiv(dedge_oppo, 3u);
        Normal3f n_oppo = face_normal(face_idx_oppo, has_opposite);

        Mask greater_dedge_idx = dedge_oppo > dedge_curr;
        Mask not_flat = dr::abs(dr::dot(n, n_oppo)) < 1.f;
        Mask only_one_visible_face =
            dr::dot(to_p0, n) * dr::dot(to_p0, n_oppo) <= 0.f;

        Mask valid = !has_opposite || (greater_dedge_idx &&
                                       only_one_visible_face &&
                                       not_flat);

        UInt32 valid_indices = dr::compress(valid);
        dr::masked(weight, !valid) = 0.f;
        Float valid_weight = dr::gather<Float>(weight, valid_indices);

        return std::make_tuple(valid_indices, valid_weight);
    }
}

MI_VARIANT typename Mesh<Float, Spectrum>::SilhouetteSample3f
Mesh<Float, Spectrum>::sample_precomputed_silhouette(const Point3f &viewpoint,
                                                     Index sample1 /*=dedge*/,
                                                     Float sample2,
                                                     Mask active) const {

    auto [face_idx, e] = dr::idivmod(sample1, 3u);
    Vector3u fi = face_indices(face_idx, active);
    Point3f p0 = vertex_position(pick_vertex(fi, e     ), active),
            p1 = vertex_position(pick_vertex(fi, e + 1u), active),
            p2 = vertex_position(pick_vertex(fi, e + 2u), active);

    SilhouetteSample3f ss = dr::zeros<SilhouetteSample3f>();
    ss.p = dr::lerp(p0, p1, sample2);
    ss.d = dr::normalize(ss.p - viewpoint);
    ss.silhouette_d = dr::normalize(p1 - p0);
    ss.pdf = dr::rsqrt(dr::squared_norm(p0 - p1));
    ss.offset = 0.f;
    ss.prim_index = face_idx;
    ss.shape = this;
    ss.discontinuity_type = (uint32_t) DiscontinuityFlags::PerimeterType;

    Vector3f inward_dir = p2 - ss.p;
    ss.n = dr::normalize(dr::cross(ss.d, ss.silhouette_d));
    dr::masked(ss.n, dr::dot(ss.n, inward_dir) > 0.f) *= -1.f;

    // Face local barycentric UV coordinates used by `differential_motion`
    ss.uv = dr::select(e == 0u,
                       Point2f(sample2, 0.f),
                       Point2f(1 - sample2, sample2));
    ss.uv = dr::select(e == 2u,
                       Point2f(0.f, 1 - sample2),
                       ss.uv);

    return ss;
}

//! @}
// =============================================================

// =============================================================
//! @{ \name Ray tracing routines
// =============================================================

MI_VARIANT typename Mesh<Float, Spectrum>::Point3f
Mesh<Float, Spectrum>::barycentric_coordinates(const SurfaceInteraction3f &si,
                                               Mask active) const {
    Vector3u fi = face_indices(si.prim_index, active);

    Point3f p0 = vertex_position(fi[0], active),
            p1 = vertex_position(fi[1], active),
            p2 = vertex_position(fi[2], active);

    Vector3f rel = si.p - p0,
             du  = p1 - p0,
             dv  = p2 - p0;

    /* Solve a least squares problem to determine
       the UV coordinates within the current triangle */
    Float b1  = dr::dot(du, rel), b2 = dr::dot(dv, rel),
          a11 = dr::dot(du, du), a12 = dr::dot(du, dv),
          a22 = dr::dot(dv, dv),
          inv_det = dr::rcp(a11 * a22 - a12 * a12);

    Float u = dr::fmsub (a22, b1, a12 * b2) * inv_det,
          v = dr::fnmadd(a12, b1, a11 * b2) * inv_det,
          w = 1.f - u - v;

    return {w, u, v};
}


MI_VARIANT typename Mesh<Float, Spectrum>::SurfaceInteraction3f
Mesh<Float, Spectrum>::compute_surface_interaction(const Ray3f &ray,
                                                   const PreliminaryIntersection3f &pi,
                                                   uint32_t ray_flags,
                                                   uint32_t recursion_depth,
                                                   Mask active) const {
    MI_MASK_ARGUMENT(active);

    // Early exit when tracing isn't necessary
    if (!m_is_instance && recursion_depth > 0)
        return dr::zeros<SurfaceInteraction3f>();

    constexpr bool IsDiff = dr::is_diff_v<Float>;

    Vector3u fi = face_indices(pi.prim_index, active);

    Point3f p0 = vertex_position(fi[0], active),
            p1 = vertex_position(fi[1], active),
            p2 = vertex_position(fi[2], active);

    Float t = pi.t;
    Point2f prim_uv = pi.prim_uv;

    if constexpr (IsDiff) {
        /* On a high level, the computed surface interaction has gradients
           attached due to (1) ray.o, (2) ray.d, (3) motion of the intersected
           triangle.
           Moeller and Trumbore method bridges the gradients at 'ray' and the
           computed surface interaction. But the effects of the third part
           remains ambiguous. 'DetachShape' explicitly detaches the three
           vertices, which is equivalent to computing a 'hit point' of a laser
           characterized by 'ray'. 'FollowShape' on the other hand first finds
           the 'hit point', then glues the interaction point with the
           intersected triangle. For this reason, it no longer tracks
           infinitesimal changes of the laser ('ray') itself.
           Note that these two flags not only affects the interaction point
           position, but also the distance and local differential geometry. */
        if (has_flag(ray_flags, RayFlags::DetachShape) &&
            has_flag(ray_flags, RayFlags::FollowShape))
            Throw("Invalid combination of RayFlags: DetachShape | FollowShape");

        if (has_flag(ray_flags, RayFlags::DetachShape)) {
            p0 = dr::detach<true>(p0);
            p1 = dr::detach<true>(p1);
            p2 = dr::detach<true>(p2);
        }

        /* When either the input ray or the vertex positions (p0, p1, p2) have
           gradient tracking enabled, we need to perform a differentiable
           ray-triangle intersection (done here via the method by Moeller and
           Trumbore). The result is mapped through `dr::replace_grad` so that we
           don't actually recompute the primal intersection but only use the
           intersection computation graph for derivative tracking (this assumes
           that the function is eventually differentiated). When the
           'FollowShape' ray flag is specified, we skip this part since the
           intersection position should be rigidly attached to the mesh. */
        if (dr::grad_enabled(p0, p1, p2, ray.o, ray.d /* <- any enabled? */) &&
            !has_flag(ray_flags, RayFlags::FollowShape)) {
            auto [t_d, prim_uv_d, hit] =
                moeller_trumbore(ray, p0, p1, p2);

            prim_uv = dr::replace_grad(prim_uv, prim_uv_d);
            t = dr::replace_grad(t, t_d);
        }
    }

    Float b1 = prim_uv.x(),
          b2 = prim_uv.y(),
          b0 = 1.f - b1 - b2;

    SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

    // Re-interpolate intersection using barycentric coordinates
    si.p = dr::fmadd(p0, b0, dr::fmadd(p1, b1, p2 * b2));

    // Potentially recompute the distance traveled to the surface interaction hit point
    if (IsDiff && has_flag(ray_flags, RayFlags::FollowShape))
        t = dr::sqrt(dr::squared_norm(si.p - ray.o) / dr::squared_norm(ray.d));

    si.t = dr::select(active, t, dr::Infinity<Float>);

    // Face normal
    si.n = face_normal(pi.prim_index, active);

    // Texture coordinates (if available)
    si.uv = Point2f(b1, b2);

    std::tie(si.dp_du, si.dp_dv) = coordinate_system(si.n);

    Vector3f dp0 = p1 - p0,
             dp1 = p2 - p0;

    if (has_vertex_texcoords() &&
        likely(has_flag(ray_flags, RayFlags::UV) ||
               has_flag(ray_flags, RayFlags::dPdUV))) {
        Point2f uv0 = vertex_texcoord(fi[0], active),
                uv1 = vertex_texcoord(fi[1], active),
                uv2 = vertex_texcoord(fi[2], active);
        if (IsDiff && has_flag(ray_flags, RayFlags::DetachShape)) {
            uv0 = dr::detach<true>(uv0);
            uv1 = dr::detach<true>(uv1);
            uv2 = dr::detach<true>(uv2);
        }

        si.uv = dr::fmadd(uv2, b2, dr::fmadd(uv1, b1, uv0 * b0));

        if (likely(has_flag(ray_flags, RayFlags::dPdUV))) {
            Vector2f duv0 = uv1 - uv0,
                     duv1 = uv2 - uv0;

            Float det     = dr::fmsub(duv0.x(), duv1.y(), duv0.y() * duv1.x()),
                  inv_det = dr::rcp(det);

            Mask valid = (det != 0.f);

            si.dp_du[valid] = dr::fmsub( duv1.y(), dp0, duv0.y() * dp1) * inv_det;
            si.dp_dv[valid] = dr::fnmadd(duv1.x(), dp0, duv0.x() * dp1) * inv_det;
        }
    }

    // Fetch shading normal (if available)
    Normal3f n0, n1, n2;
    if (has_vertex_normals() &&
        likely(has_flag(ray_flags, RayFlags::ShadingFrame) ||
               has_flag(ray_flags, RayFlags::dNSdUV))) {
        n0 = vertex_normal(fi[0], active);
        n1 = vertex_normal(fi[1], active);
        n2 = vertex_normal(fi[2], active);
    }

    if (has_vertex_normals() &&
        likely(has_flag(ray_flags, RayFlags::ShadingFrame) ||
               has_flag(ray_flags, RayFlags::dNSdUV))) {
        if (IsDiff && has_flag(ray_flags, RayFlags::DetachShape)) {
            n0 = dr::detach<true>(n0);
            n1 = dr::detach<true>(n1);
            n2 = dr::detach<true>(n2);
        }

        Normal3f n = dr::fmadd(n2, b2, dr::fmadd(n1, b1, n0 * b0));
        Float il = dr::rsqrt(dr::squared_norm(n));
        n *= il;

        si.sh_frame.n = n;

        if (has_flag(ray_flags, RayFlags::dNSdUV)) {
            /* Now compute the derivative of "normalize(u*n1 + v*n2 + (1-u-v)*n0)"
               with respect to [u, v] in the local triangle parameterization.

               Since d/du [f(u)/|f(u)|] = [d/du f(u)]/|f(u)|
                   - f(u)/|f(u)|^3 <f(u), d/du f(u)>, this results in
            */
            si.dn_du = (n1 - n0) * il;
            si.dn_dv = (n2 - n0) * il;

            si.dn_du = dr::fnmadd(n, dr::dot(n, si.dn_du), si.dn_du);
            si.dn_dv = dr::fnmadd(n, dr::dot(n, si.dn_dv), si.dn_dv);
        } else {
            si.dn_du = si.dn_dv = dr::zeros<Vector3f>();
        }
    } else {
        si.sh_frame.n = si.n;
    }

    if (m_flip_normals) {
        si.n = -si.n;
        si.sh_frame.n = -si.sh_frame.n;
    }

    si.shape    = this;
    si.instance = nullptr;

    return si;
}

//! @}
// =============================================================

// =============================================================
//! @{ \name Mesh attributes
// =============================================================

MI_VARIANT void Mesh<Float, Spectrum>::add_attribute(const std::string& name,
                                                     size_t dim,
                                                     const std::vector<InputFloat>& data) {
    auto attribute = m_mesh_attributes.find(name);
    if (attribute != m_mesh_attributes.end())
        Throw("add_attribute(): attribute %s already exists.", name.c_str());

    bool is_vertex_attr = name.find("vertex_") == 0;
    bool is_face_attr   = name.find("face_") == 0;
    if (!is_vertex_attr && !is_face_attr)
        Throw("add_attribute(): attribute name must start with either \"vertex_\" of \"face_\".");

    MeshAttributeType type = is_vertex_attr ? MeshAttributeType::Vertex : MeshAttributeType::Face;
    size_t count = is_vertex_attr ? m_vertex_count : m_face_count;

    // In spectral modes, convert RGB color to srgb model coefs if attribute name contains 'color'
    if constexpr (is_spectral_v<Spectrum>) {
        if (dim == 3 && name.find("color") != std::string::npos) {
            InputFloat *ptr = (InputFloat *) data.data();
            for (size_t i = 0; i < count; ++i) {
                dr::store(ptr, srgb_model_fetch(dr::load<Color<InputFloat, 3>>(ptr)));
                ptr += 3;
            }
        }
    }

    FloatStorage buffer = dr::load<FloatStorage>(data.data(), count * dim);
    m_mesh_attributes.insert({ name, { dim, type, buffer } });
}

MI_VARIANT typename Mesh<Float, Spectrum>::Mask
Mesh<Float, Spectrum>::has_attribute(const std::string& name, Mask active) const {
    const auto& it = m_mesh_attributes.find(name);
    if (it == m_mesh_attributes.end())
        return Base::has_attribute(name, active);
    return true;
}

MI_VARIANT typename Mesh<Float, Spectrum>::UnpolarizedSpectrum
Mesh<Float, Spectrum>::eval_attribute(const std::string& name,
                                      const SurfaceInteraction3f &si,
                                      Mask active) const {
    const auto& it = m_mesh_attributes.find(name);
    if (it == m_mesh_attributes.end())
        return Base::eval_attribute(name, si, active);

    const auto& attr = it->second;
    if (attr.size == 1)
        return interpolate_attribute<1, false>(attr.type, attr.buf, si, active);
    else if (attr.size == 3) {
        auto result = interpolate_attribute<3, false>(attr.type, attr.buf, si, active);
        if constexpr (is_monochromatic_v<Spectrum>)
            return luminance(result);
        else
            return result;
    } else {
        if constexpr (dr::is_jit_v<Float>)
            return 0.f;
        else
            Throw("eval_attribute(): Attribute \"%s\" requested but had size %u.", name, attr.size);
    }
}

MI_VARIANT Float
Mesh<Float, Spectrum>::eval_attribute_1(const std::string& name,
                                        const SurfaceInteraction3f &si,
                                        Mask active) const {
    const auto& it = m_mesh_attributes.find(name);
    if (it == m_mesh_attributes.end())
        return Base::eval_attribute_1(name, si, active);

    const auto& attr = it->second;
    if (attr.size == 1) {
        return interpolate_attribute<1, true>(attr.type, attr.buf, si, active);
    } else {
        if constexpr (dr::is_jit_v<Float>)
            return 0.f;
        else
            Throw("eval_attribute_1(): Attribute \"%s\" requested but had size %u.", name, attr.size);
    }
}

MI_VARIANT typename Mesh<Float, Spectrum>::Color3f
Mesh<Float, Spectrum>::eval_attribute_3(const std::string& name,
                                        const SurfaceInteraction3f &si,
                                        Mask active) const {
    const auto& it = m_mesh_attributes.find(name);
    if (it == m_mesh_attributes.end())
        return Base::eval_attribute_3(name, si, active);

    const auto& attr = it->second;
    if (attr.size == 3) {
        return interpolate_attribute<3, true>(attr.type, attr.buf, si, active);
    } else {
        if constexpr (dr::is_jit_v<Float>)
            return 0.f;
        else
            Throw("eval_attribute_3(): Attribute \"%s\" requested but had size %u.", name, attr.size);
    }
}

//! @}
// =============================================================

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
    bool cur_is_inside = (distance >= 0);
    size_t out_count   = 0;

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

MI_VARIANT typename Mesh<Float, Spectrum>::ScalarBoundingBox3f
Mesh<Float, Spectrum>::bbox(ScalarIndex index, const ScalarBoundingBox3f &clip) const {
    using ScalarPoint3d = mitsuba::Point<double, 3>;

    // Reserve room for some additional vertices
    ScalarPoint3d vertices1[max_vertices], vertices2[max_vertices];
    size_t n_vertices = 3;

    Assert(index <= m_face_count);

    ScalarVector3u fi = face_indices(index);
    Assert(fi[0] < m_vertex_count);
    Assert(fi[1] < m_vertex_count);
    Assert(fi[2] < m_vertex_count);

    ScalarPoint3f v0 = vertex_position(fi[0]),
                  v1 = vertex_position(fi[1]),
                  v2 = vertex_position(fi[2]);

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

MI_VARIANT std::string Mesh<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
    oss << class_()->name() << "[" << std::endl
        << "  name = \"" << m_name << "\"," << std::endl
        << "  bbox = " << string::indent(m_bbox) << "," << std::endl
        << "  vertex_count = " << m_vertex_count << "," << std::endl
        << "  vertices = [" << util::mem_string(vertex_data_bytes() * m_vertex_count) << " of vertex data]," << std::endl
        << "  face_count = " << m_face_count << "," << std::endl
        << "  faces = [" << util::mem_string(face_data_bytes() * m_face_count) << " of face data]," << std::endl;

    if (!m_area_pmf.empty())
        oss << "  surface_area = " << m_area_pmf.sum() << "," << std::endl;

    oss << "  face_normals = " << m_face_normals;

    if (!m_mesh_attributes.empty()) {
        oss << "," << std::endl << "  mesh attributes = [" << std::endl;
        size_t i = 0;
        for(const auto &[name, attribute]: m_mesh_attributes)
            oss << "    " << name << ": " << attribute.size
                << (attribute.size == 1 ? " float" : " floats")
                << (++i == m_mesh_attributes.size() ? "" : ",") << std::endl;
        oss << "  ]" << std::endl;
    } else {
        oss << std::endl;
    }

    oss << "]";
    return oss.str();
}

MI_VARIANT size_t Mesh<Float, Spectrum>::vertex_data_bytes() const {
    size_t vertex_data_bytes = 3 * sizeof(InputFloat);

    if (has_vertex_normals())
        vertex_data_bytes += 3 * sizeof(InputFloat);
    if (has_vertex_texcoords())
        vertex_data_bytes += 2 * sizeof(InputFloat);

    for (const auto&[name, attribute]: m_mesh_attributes)
        if (attribute.type == MeshAttributeType::Vertex)
            vertex_data_bytes += attribute.size * sizeof(InputFloat);

    return vertex_data_bytes;
}

MI_VARIANT size_t Mesh<Float, Spectrum>::face_data_bytes() const {
    size_t face_data_bytes = 3 * sizeof(ScalarIndex);

    for (const auto&[name, attribute]: m_mesh_attributes)
        if (attribute.type == MeshAttributeType::Face)
            face_data_bytes += attribute.size * sizeof(InputFloat);

    return face_data_bytes;
}

#if defined(MI_ENABLE_EMBREE)
MI_VARIANT RTCGeometry Mesh<Float, Spectrum>::embree_geometry(RTCDevice device) {
    RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);

    rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3,
                               m_vertex_positions.data(), 0, 3 * sizeof(InputFloat),
                               m_vertex_count);
    rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3,
                               m_faces.data(), 0, 3 * sizeof(ScalarIndex),
                               m_face_count);

    rtcCommitGeometry(geom);
    return geom;
}
#endif

#if defined(MI_ENABLE_CUDA)
static const uint32_t triangle_input_flags = OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT;

MI_VARIANT void Mesh<Float, Spectrum>::optix_prepare_geometry() { }

MI_VARIANT void Mesh<Float, Spectrum>::optix_build_input(OptixBuildInput &build_input) const {
    m_vertex_buffer_ptr = (void*) m_vertex_positions.data(); // triggers dr::eval()
    build_input.type                           = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
    build_input.triangleArray.vertexFormat     = OPTIX_VERTEX_FORMAT_FLOAT3;
    build_input.triangleArray.indexFormat      = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
    build_input.triangleArray.numVertices      = m_vertex_count;
    build_input.triangleArray.vertexBuffers    = (CUdeviceptr*) &m_vertex_buffer_ptr;
    build_input.triangleArray.numIndexTriplets = m_face_count;
    build_input.triangleArray.indexBuffer      = (CUdeviceptr) m_faces.data();
    build_input.triangleArray.flags            = &triangle_input_flags;
    build_input.triangleArray.numSbtRecords    = 1;
}
#endif

MI_VARIANT bool Mesh<Float, Spectrum>::parameters_grad_enabled() const {
    return dr::grad_enabled(m_vertex_positions);
}

MI_IMPLEMENT_CLASS_VARIANT(Mesh, Shape)
MI_INSTANTIATE_CLASS(Mesh)
NAMESPACE_END(mitsuba)
