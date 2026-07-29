#include <mitsuba/render/mesh_utils.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/stream.h>
#include <cmath>
#include <cstring>
#include <utility>

NAMESPACE_BEGIN(mitsuba)

/// Allocate a staging buffer of the flavor appropriate for \c backend
template <typename T>
static drjit::unique_buffer<T> alloc(JitBackend backend, size_t n) {
    return drjit::unique_buffer<T>(backend, n, /* shared */ true);
}

PackedMesh::PackedMesh(JitBackend backend_, size_t vertex_count_,
                       size_t face_count_, Layout layout_,
                       size_t position_count_, size_t normal_count_)
    : backend(backend_), layout(layout_),
      vertex_count(vertex_count_), face_count(face_count_),
      position_count(position_count_), normal_count(normal_count_) {

    vertices = alloc<float>(backend, vertex_count * MeshVertexStride);
    faces    = alloc<uint32_t>(backend, face_count * MeshFaceStride);

    if (position_count)
        position_index = alloc<uint32_t>(backend, vertex_count);
    if (normal_count)
        normal_index = alloc<uint32_t>(backend, vertex_count);
}

void PackedMesh::set_transform(const ScalarAffineTransform4f &to_world,
                               bool flip_normals) {
    if (m_written)
        Throw("PackedMesh::set_transform(): call this method prior to"
              "any use of set_vertex() or set_face().");

    m_to_world       = to_world;
    m_transform      = to_world != ScalarAffineTransform4f();
    m_negate_normals = flip_normals;
    reverse_winding  =
        (dr::det(dr::Matrix<float, 3>(m_to_world.matrix)) < 0.f) != flip_normals;
}

void PackedMesh::transform_records() {
    if (m_written)
        Throw("PackedMesh::transform_records(): this method only exists for "
              "callers that deposit mesh data directly (i.e., not through "
              "set_vertex() or set_face().");

    bool normals  = has_flag(layout, Layout::Normals),
         tangents = has_flag(layout, Layout::Tangents);

    if (m_transform || m_negate_normals) {
        if (m_transform)
            bbox = BoundingBox<ScalarPoint3f>();

        float *rec = vertices.data();
        for (size_t v = 0; v < vertex_count; ++v, rec += MeshVertexStride) {
            if (m_transform) {
                ScalarPoint3f p =
                    m_to_world * dr::load<ScalarPoint3f>(rec + PackedPositionOffset);
                dr::store(rec + PackedPositionOffset, p);
                bbox.expand(p);
            }

            if (!normals)
                continue;

            ScalarVector3f f = dr::load<ScalarVector3f>(rec + PackedFrameOffset);
            if (!tangents) {
                ScalarNormal3f n(f);
                if (m_transform)
                    n = dr::normalize(m_to_world * n);
                f = ScalarVector3f(m_negate_normals ? -n : n);
            } else {
                // Normals and tangents follow the inverse transpose and the
                // matrix respectively, so the frame stays orthogonal
                auto [n, s] = frame_decode(f);
                if (m_transform) {
                    n = dr::normalize(m_to_world * n);
                    s = dr::normalize(m_to_world * s);
                }
                f = frame_encode(m_negate_normals ? -n : n, s);
            }
            dr::store(rec + PackedFrameOffset, f);
        }
    }

    if (reverse_winding) {
        uint32_t *rec = faces.data();
        for (size_t i = 0; i < face_count; ++i, rec += MeshFaceStride) {
            std::swap(rec[0], rec[2]);
            if (tangents)
                rec[3] ^= FaceUVFlipped;
        }
    }

    m_transform = m_negate_normals = reverse_winding = false;
}

void PackedMesh::set_vertex(size_t i, const ScalarPoint3f &p_,
                            const ScalarNormal3f &n_,
                            const ScalarVector2f &uv) {
    using PackedVertex = dr::Array<float, MeshVertexStride>;
    Assert(i < vertex_count && !has_flag(layout, Layout::Tangents));
    m_written = true;

    ScalarPoint3f p = m_transform ? m_to_world * p_ : p_;

    PackedVertex vertex(0.f);
    vertex[PackedPositionOffset]     = p.x();
    vertex[PackedPositionOffset + 1] = p.y();
    vertex[PackedPositionOffset + 2] = p.z();

    if (has_flag(layout, Layout::Normals)) {
        ScalarNormal3f n = m_transform ? m_to_world * n_ : n_;
        float il = dr::rsqrt(dr::squared_norm(n));
        n *= dr::isfinite(il) ? il : 1.f;
        if (m_negate_normals)
            n = -n;
        vertex[PackedFrameOffset]     = n.x();
        vertex[PackedFrameOffset + 1] = n.y();
        vertex[PackedFrameOffset + 2] = n.z();
    }

    if (has_flag(layout, Layout::Texcoords)) {
        vertex[PackedTexcoordOffset]     = uv.x();
        vertex[PackedTexcoordOffset + 1] = uv.y();
    }

    dr::store(vertices.data() + i * MeshVertexStride, vertex);
    bbox.expand(p);
}

void PackedMesh::set_face(size_t i, const ScalarVector3u &indices,
                          uint32_t bsdf) {
    using PackedFace = dr::Array<uint32_t, MeshFaceStride>;
    Assert(i < face_count);
    m_written = true;

    if (indices.x() >= vertex_count || indices.y() >= vertex_count ||
        indices.z() >= vertex_count)
        Throw("PackedMesh::set_face(): face %zu references out-of-bounds "
              "vertices (%u, %u, %u), but the mesh only has %zu vertices.",
              i, indices.x(), indices.y(), indices.z(), vertex_count);

    if (bsdf != 0)
        layout |= Layout::FaceBSDFs;

    uint32_t i0 = indices.x(), i2 = indices.z();
    if (reverse_winding)
        std::swap(i0, i2);

    dr::store(faces.data() + i * MeshFaceStride,
              PackedFace(i0, indices.y(), i2, bsdf));
}

float *PackedMesh::add_attribute(std::string_view name, size_t dim,
                                 bool upsample_srgb) {
    bool face = string::starts_with(name, "face_");
    if (!face && !string::starts_with(name, "vertex_"))
        Throw("add_attribute(): attribute name \"%s\" must start with "
              "\"vertex_\" or \"face_\".", name);
    if (dim == 0 || dim > 4)
        Throw("add_attribute(): attribute \"%s\": 1 to 4 channels are "
              "supported, got %zu.", name, dim);
    for (const Attribute &a : attrs)
        if (a.name == name)
            Throw("add_attribute(): duplicate attribute \"%s\".", name);

    size_t rows = face ? face_count : vertex_count;
    attrs.push_back({ std::string(name), dim, upsample_srgb,
                      alloc<float>(backend, rows * dim) });
    return attrs.back().values.data();
}

/// Missing indexed corner entries resolve to zeros
constexpr uint32_t MissingIndex = (uint32_t) -1;

/**
 * \brief Fetch the bit pattern of one record of attribute \c d at source
 * corner \c sc and advance the output pointer
 *
 * Indices resolve (missing entries yield zeros), and -0.0 folds to +0.0
 * so that equal values also match bitwise.
 */
static uint32_t *fetch_key(const CornerAttribute &d, uint32_t sc,
                           uint32_t *out) {
    size_t dim = d.dim;
    const float *src = d.data + (size_t) sc * dim;
    if (d.indices) {
        uint32_t idx = d.indices[sc];
        src = idx == MissingIndex ? nullptr : d.data + (size_t) idx * dim;
    }

    if (src)
        memcpy(out, src, dim * sizeof(float));
    else
        memset(out, 0, dim * sizeof(float));

    for (size_t k = 0; k < dim; ++k)
        if (out[k] == 0x80000000u)
            out[k] = 0;

    return out + dim;
}

PackedMesh corner_to_packed_mesh(
        JitBackend backend, const CornerMesh &desc, std::string_view name,
        bool face_normals, bool flip_normals,
        const PackedMesh::ScalarAffineTransform4f &to_world) {
    // Prefix errors with the mesh name so users can locate the culprit
    auto fail = [&](const char *fmt, const auto &...args) {
        Throw("corner_to_packed_mesh(): mesh \"%s\": %s", name,
              tfm::format(fmt, args...));
    };

    size_t n_points = desc.vertex_count,
           n_corners = desc.corner_count,
           n_records = desc.record_count ? desc.record_count : n_corners;

    if (!desc.positions && n_points > 0)
        fail("a position array is required.");
    if (!desc.corner_vertex && n_corners > 0)
        fail("a corner_vertex array is required.");
    if (!desc.face_offsets && n_corners % 3 != 0)
        fail("the corner count (%zu) must be a multiple of 3 for triangle "
             "input.", n_corners);

    if (desc.corner_index) {
        for (size_t c = 0; c < n_corners; ++c)
            if (desc.corner_index[c] >= n_records)
                fail("'corner_index' selects record %u at corner %zu, but "
                     "only %zu records were supplied.", desc.corner_index[c],
                     c, n_records);
    }

    size_t n_nonfinite = 0;
    for (size_t i = 0; i < n_points * 3; ++i)
        n_nonfinite += !std::isfinite(desc.positions[i]);
    if (n_nonfinite > 0)
        Log(Warn, "corner_to_packed_mesh(): mesh \"%s\": %zu position "
            "values are not finite.", name, n_nonfinite);

    auto check_data = [&](const CornerAttribute &d) {
        if (d.indices) {
            for (size_t r = 0; r < n_records; ++r)
                if (d.indices[r] != MissingIndex &&
                    d.indices[r] >= d.value_count)
                    fail("attribute \"%s\": index %u at record %zu exceeds "
                         "the value count (%zu).", d.name, d.indices[r], r,
                         d.value_count);
        } else if (d.value_count != n_records) {
            fail("attribute \"%s\": expected one value per corner record "
                 "(record_count=%zu, value_count=%zu).",
                 d.name, n_records, d.value_count);
        }
    };

    // Per-face normals make supplied vertex normals irrelevant: don't let
    // them split vertices or materialize
    const CornerAttribute *normals =
        (desc.normals.data && !face_normals) ? &desc.normals : nullptr;
    const CornerAttribute *texcoords = desc.texcoords.data ? &desc.texcoords
                                                           : nullptr;
    if (normals)
        check_data(*normals);
    if (texcoords)
        check_data(*texcoords);

    for (size_t i = 0; i < desc.attr_count; ++i) {
        const CornerAttribute &attr = desc.attrs[i];
        if (!attr.data)
            fail("attribute \"%s\": a data pointer is required.", attr.name);
        if (!string::starts_with(attr.name, "vertex_"))
            fail("attribute name \"%s\" must start with \"vertex_\".",
                 attr.name);
        check_data(attr);
    }

    // Triangulate. From here on, "corner" refers to a triangle corner, and
    // tri_corner maps back to the input face corners. Triangles pass through.
    // Quads split along corners 0-2. Larger polygons fan around corner 0.

    std::vector<uint32_t> tri_corner, tri_bsdf;
    size_t n_tris = n_corners / 3;
    if (desc.face_offsets) {
        const uint32_t *off = desc.face_offsets;
        size_t n_faces = desc.face_count;
        if (off[0] != 0 || off[n_faces] != n_corners)
            fail("'face_offsets' must start at 0 and end at the corner "
                 "count (%zu).", n_corners);

        n_tris = 0;
        for (size_t f = 0; f < n_faces; ++f) {
            if (off[f + 1] < off[f])
                fail("'face_offsets' must be non-decreasing.");
            uint32_t n = off[f + 1] - off[f];
            n_tris += n >= 3 ? n - 2 : 0;
        }

        tri_corner.reserve(3 * n_tris);
        if (desc.bsdf_index)
            tri_bsdf.reserve(n_tris);
        for (size_t f = 0; f < n_faces; ++f) {
            uint32_t begin = off[f], n = off[f + 1] - off[f];
            for (uint32_t i = 1; i + 1 < n; ++i) {
                tri_corner.push_back(begin);
                tri_corner.push_back(begin + i);
                tri_corner.push_back(begin + i + 1);
                if (desc.bsdf_index)
                    tri_bsdf.push_back(desc.bsdf_index[f]);
            }
        }
    }
    size_t n_tri_corners = 3 * n_tris;

    // Resolve a triangle corner to its record, through the triangulation
    // and the optional corner_index indirection
    auto src_corner = [&](uint32_t c) {
        uint32_t fc = desc.face_offsets ? tri_corner[c] : c;
        return desc.corner_index ? desc.corner_index[fc] : fc;
    };
    auto corner_point = [&](uint32_t c) {
        return desc.corner_vertex[src_corner(c)];
    };

    bool has_normals = normals != nullptr,
         has_uv = texcoords != nullptr,
         // Tangents follow the texture parameterization, so corners at
         // one surface point must split wherever their triangles cannot
         // share a tangent frame; the UV orientation sign of the current
         // triangle therefore joins the vertex key below. The tangent
         // values themselves accumulate per vertex when the mesh is built.
         split_uv_sign = has_uv && !face_normals;

    std::vector<uint8_t> uv_flipped;
    if (split_uv_sign) {
        uv_flipped.resize(n_tris);
        for (size_t t = 0; t < n_tris; ++t) {
            uint32_t bits[6];
            float uv[6];
            for (uint32_t j = 0; j < 3; ++j)
                fetch_key(*texcoords, src_corner((uint32_t) (3 * t + j)),
                          bits + 2 * j);
            memcpy(uv, bits, sizeof(uv));

            float area2 = (uv[2] - uv[0]) * (uv[5] - uv[1]) -
                          (uv[3] - uv[1]) * (uv[4] - uv[0]);
            uv_flipped[t] = !(area2 > 0.f);
        }
    }

    // The canonical key of a corner, with the normal in front so that its
    // 3-word prefix doubles as the normal group key
    size_t vert_dim = (has_normals ? 3 : 0) + (has_uv ? 2 : 0) +
                      (split_uv_sign ? 1 : 0);
    for (size_t i = 0; i < desc.attr_count; ++i)
        vert_dim += desc.attrs[i].dim;
    if (vert_dim == 0)
        vert_dim = 1; // attribute-less corners weld on a constant word
    std::vector<uint32_t> key(vert_dim, 0);

    auto build_key = [&](uint32_t c) {
        uint32_t sc = src_corner(c), *k = key.data();
        if (has_normals)
            k = fetch_key(*normals, sc, k);
        if (has_uv)
            k = fetch_key(*texcoords, sc, k);
        if (split_uv_sign)
            *k++ = uv_flipped[c / 3];
        for (size_t i = 0; i < desc.attr_count; ++i)
            k = fetch_key(desc.attrs[i], sc, k);
    };

    // Stable counting sort of the corners by their source point, checking
    // vertex references along the way. The descending fill turns the
    // inclusive prefix sums back into bucket starts, so point p ends up
    // owning corner_order[point_offsets[p]] up to (exclusive)
    // corner_order[point_offsets[p + 1]], in ascending corner order.
    std::vector<uint32_t> point_offsets(n_points + 1, 0);
    std::vector<uint32_t> corner_order(n_tri_corners);
    for (size_t c = 0; c < n_tri_corners; ++c) {
        uint32_t p = corner_point((uint32_t) c);
        if (p >= n_points)
            fail("record %u references vertex %u, but only %zu vertices "
                 "were supplied.", src_corner((uint32_t) c), p, n_points);
        point_offsets[p]++;
    }
    for (size_t p = 1; p <= n_points; ++p)
        point_offsets[p] += point_offsets[p - 1];
    for (size_t c = n_tri_corners; c-- > 0; )
        corner_order[--point_offsets[corner_point((uint32_t) c)]] =
            (uint32_t) c;

    // Staging storage at the corner-count worst case (every corner its
    // own vertex); the true counts are recorded at the end
    PackedMesh pm(backend, n_tri_corners, n_tris,
                  make_layout(has_normals, has_uv),
                  /* position_count */ n_tri_corners,
                  /* normal_count */ has_normals ? n_tri_corners : 0);
    pm.set_transform(to_world, flip_normals);
    for (size_t i = 0; i < desc.attr_count; ++i)
        pm.add_attribute(desc.attrs[i].name, desc.attrs[i].dim);

    uint32_t *pidx_out = pm.position_index.data(),
             *nidx_out = pm.normal_index.data(),
             *faces_out = pm.faces.data();

    for (size_t t = 0; t < n_tris; ++t)
        faces_out[t * MeshFaceStride + 3] =
            desc.bsdf_index
                ? (desc.face_offsets ? tri_bsdf[t] : desc.bsdf_index[t])
                : 0;

    // Weld each surface point's corners in turn, so that ids follow the
    // source vertex order: conflict-free input keeps its vertex order
    // (minus unreferenced vertices), and split copies slot in behind
    // their point's first vertex. A corner scans the vertices created
    // for its point so far for a full key match and creates a new vertex
    // otherwise, finding its normal group with the same scan restricted
    // to the key's normal prefix. The scans are linear in a point's
    // valence, which is a small number in practice.
    uint32_t vertex_count = 0, position_count = 0, normal_count = 0;
    std::vector<uint32_t> vert_keys; // keys of the current point's vertices

    auto find_local = [&](uint32_t n_local, size_t n_words) -> uint32_t {
        uint32_t j = 0;
        while (j < n_local &&
               memcmp(vert_keys.data() + (size_t) j * vert_dim, key.data(),
                      n_words * sizeof(uint32_t)) != 0)
            j++;
        return j;
    };

    for (size_t p = 0; p < n_points; ++p) {
        uint32_t begin = point_offsets[p], end = point_offsets[p + 1];
        if (begin == end)
            continue; // unreferenced source vertices are dropped

        uint32_t point_id = position_count++, vert_base = vertex_count;
        auto pos =
            dr::load<PackedMesh::ScalarPoint3f>(desc.positions + 3 * p);
        vert_keys.clear();

        for (uint32_t i = begin; i != end; ++i) {
            uint32_t c = corner_order[i];
            build_key(c);

            uint32_t n_local = vertex_count - vert_base,
                     vid = vert_base + find_local(n_local, vert_dim);

            if (vid == vertex_count) {
                vertex_count++;
                pidx_out[vid] = point_id;
                if (has_normals) {
                    uint32_t g = find_local(n_local, 3);
                    nidx_out[vid] = g < n_local ? nidx_out[vert_base + g]
                                                : normal_count++;
                }
                vert_keys.insert(vert_keys.end(), key.begin(), key.end());

                // The record materializes from the canonical key, so
                // vertices of one normal group hold identical lanes
                const uint32_t *k = key.data();
                PackedMesh::ScalarNormal3f nrm(0.f, 0.f, 0.f);
                PackedMesh::ScalarVector2f uv(0.f, 0.f);
                if (has_normals) {
                    memcpy(nrm.data(), k, 3 * sizeof(float));
                    k += 3;
                }
                if (has_uv) {
                    memcpy(uv.data(), k, 2 * sizeof(float));
                    k += 2;
                }
                k += split_uv_sign ? 1 : 0;
                pm.set_vertex(vid, pos, nrm, uv);

                for (size_t a = 0; a < desc.attr_count; ++a) {
                    size_t dim = desc.attrs[a].dim;
                    memcpy(pm.attrs[a].values.data() + (size_t) vid * dim,
                           k, dim * sizeof(float));
                    k += dim;
                }
            }

            uint32_t corner = c % 3;
            faces_out[(c / 3) * MeshFaceStride +
                      (pm.reverse_winding ? 2 - corner : corner)] = vid;
        }
    }

    // Record the true counts; identity maps are dropped
    pm.vertex_count = vertex_count;
    pm.position_count = position_count;
    pm.normal_count = normal_count;

    if (position_count == vertex_count) {
        pm.position_index.reset();
        pm.position_count = 0;
    }

    if (has_normals && normal_count == vertex_count) {
        pm.normal_index.reset();
        pm.normal_count = 0;
    }

    return pm;
}

NAMESPACE_END(mitsuba)
