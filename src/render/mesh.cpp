#include <mitsuba/core/fstream.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
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

    initialize();
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
        m_vertex_count = m_vertex_positions.size() / 3;
    }
    if (m_faces.size() != m_face_count * 3) {
        Log(Debug, "parameters_changed(): Face count changed, updating it.");
        mesh_attributes_changed = true;
        m_face_count = m_faces.size() / 3;
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

    if (keys.empty() || string::contains(keys, "vertex_positions") || mesh_attributes_changed) {
        recompute_bbox();

        if (has_vertex_normals())
            recompute_vertex_normals();

        if (!m_area_pmf.empty())
            m_area_pmf = DiscreteDistribution<Float>();

        if (m_parameterization)
            m_parameterization = nullptr;

#if defined(MI_ENABLE_LLVM) && !defined(MI_ENABLE_EMBREE)
        m_vertex_positions_ptr = m_vertex_positions.data();
        m_faces_ptr = m_faces.data();
#endif
        mark_dirty();
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

        // Disconnect the vertex normal buffer from any pre-existing AD
        // graph. Otherwise an AD graph might be unnecessarily retained
        // here, despite the following lines re-initializing the normals.
        dr::disable_grad(m_vertex_normals);

        UInt32 ni = dr::arange<UInt32>(m_vertex_count) * 3;
        for (size_t i = 0; i < 3; ++i)
            dr::scatter(m_vertex_normals,
                        dr::float32_array_t<Float>(normals[i]), ni + i);

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

    if (!m_area_pmf.empty())
        return; // already built!

    if (m_face_count == 0)
        Throw("Cannot create sampling table for an empty mesh: %s", to_string());

    auto&& vertex_positions = dr::migrate(m_vertex_positions, AllocType::Host);
    auto&& faces = dr::migrate(m_faces, AllocType::Host);
    if constexpr (dr::is_jit_v<Float>)
        dr::sync_thread();

    const InputFloat *pos_p = vertex_positions.data();
    const ScalarIndex *idx_p = faces.data();

    std::vector<ScalarFloat> table(m_face_count);
    for (ScalarIndex i = 0; i < m_face_count; i++) {
        ScalarPoint3u idx = dr::load<ScalarPoint3u>(idx_p + 3 * i);

        ScalarPoint3f p0 = dr::load<InputPoint3f>(pos_p + 3 * idx.x()),
                      p1 = dr::load<InputPoint3f>(pos_p + 3 * idx.y()),
                      p2 = dr::load<InputPoint3f>(pos_p + 3 * idx.z());

        table[i] = .5f * dr::norm(dr::cross(p1 - p0, p2 - p0));
    }

    m_area_pmf = DiscreteDistribution<Float>(
        table.data(),
        m_face_count
    );
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

    Vector3f dp0 = p1 - p0,
             dp1 = p2 - p0;

    SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

    // Re-interpolate intersection using barycentric coordinates
    si.p = dr::fmadd(p0, b0, dr::fmadd(p1, b1, p2 * b2));

    // Potentially recompute the distance traveled to the surface interaction hit point
    if (IsDiff && has_flag(ray_flags, RayFlags::FollowShape))
        t = dr::sqrt(dr::squared_norm(si.p - ray.o) / dr::squared_norm(ray.d));

    si.t = dr::select(active, t, dr::Infinity<Float>);

    // Face normal
    si.n = dr::normalize(dr::cross(dp0, dp1));

    // Texture coordinates (if available)
    si.uv = Point2f(b1, b2);

    std::tie(si.dp_du, si.dp_dv) = coordinate_system(si.n);

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

            Mask valid = dr::neq(det, 0.f);

            si.dp_du[valid] = dr::fmsub( duv1.y(), dp0, duv0.y() * dp1) * inv_det;
            si.dp_dv[valid] = dr::fnmadd(duv1.x(), dp0, duv0.x() * dp1) * inv_det;
        }
    }

    // Fetch shading normal (if available)
    Normal3f n0, n1, n2;
    if (has_vertex_normals() &&
        likely(has_flag(ray_flags, RayFlags::ShadingFrame) ||
               has_flag(ray_flags, RayFlags::dNSdUV) ||
               has_flag(ray_flags, RayFlags::BoundaryTest))) {
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

    if (unlikely(has_flag(ray_flags, RayFlags::BoundaryTest))) {
        Vector3f rel = si.p - p0;

        /* Solve a least squares problem to determine
           the UV coordinates within the current triangle */
        Float bb1 = dr::dot(dp0, rel),
              bb2 = dr::dot(dp1, rel),
              a11 = dr::dot(dp0, dp0),
              a12 = dr::dot(dp0, dp1),
              a22 = dr::dot(dp1, dp1),
              inv_det = dr::rcp(a11 * a22 - a12 * a12);

        Float u = dr::fmsub (a22, bb1, a12 * bb2) * inv_det,
              v = dr::fnmadd(a12, bb1, a11 * bb2) * inv_det,
              w = 1.f - u - v;

        /* If we are using flat shading, just fall back to a signed distance
           field of the hit triangle. */
        if (!has_vertex_normals()) {
            // 2D Triangle SDF from Inigo Quilez
            // https://www.iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm

            // Equilateral triangle
            Point2f tp0 = Point2f(0, 0),
                    tp1 = Point2f(1, 0),
                    tp2 = Point2f(0.5f, 0.5f * dr::sqrt(3.f));

            Point2f p = tp0 * w + tp1 * u + tp2 * v;

            Vector2f e0 = tp1 - tp0,
                     e1 = tp2 - tp1,
                     e2 = tp0 - tp2,
                     v0 = p - tp0,
                     v1 = p - tp1,
                     v2 = p - tp2;
            Vector2f pq0 = v0 - e0 * dr::clamp(dr::dot(v0, e0) / dr::dot(e0, e0), 0, 1),
                     pq1 = v1 - e1 * dr::clamp(dr::dot(v1, e1) / dr::dot(e1, e1), 0, 1),
                     pq2 = v2 - e2 * dr::clamp(dr::dot(v2, e2) / dr::dot(e2, e2), 0, 1);
            Float s = dr::sign(e0.x() * e2.y() - e0.y() * e2.x());
            Vector2f d = dr::minimum(dr::minimum(Vector2f(dr::dot(pq0, pq0), s * (v0.x() * e0.y() - v0.y() * e0.x())),
                                                 Vector2f(dr::dot(pq1, pq1), s * (v1.x() * e1.y() - v1.y() * e1.x()))),
                                                 Vector2f(dr::dot(pq2, pq2), s * (v2.x() * e2.y() - v2.y() * e2.x())));
            Float dist = dr::sqrt(d.x());
            // Scale s.t. farthest point / barycenter is one
            dist /= dr::sqrt(3.f) / 6.f;
            si.boundary_test = dist;
        } else {
            Normal3f normal = dr::fmadd(n0, w, dr::fmadd(n1, u, n2 * v));

            // Dot product between surface normal and the ray direction is 0 at silhouette points
            Float dp = dr::dot(normal, -ray.d);

            // Add non-linearity by squaring the returned value
            si.boundary_test = dr::sqr(dp);
        }
    }

    return si;
}

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
    bool result = false;
    for (auto &[name, attribute]: m_mesh_attributes)
        result |= dr::grad_enabled(attribute.buf);
    result |= dr::grad_enabled(m_vertex_positions);
    result |= dr::grad_enabled(m_vertex_normals);
    result |= dr::grad_enabled(m_vertex_texcoords);
    return result;
}

MI_IMPLEMENT_CLASS_VARIANT(Mesh, Shape)
MI_INSTANTIATE_CLASS(Mesh)
NAMESPACE_END(mitsuba)
