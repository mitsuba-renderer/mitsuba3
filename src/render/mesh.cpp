#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/zstream.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/mesh_utils.h>
#include <mitsuba/render/scene_ir.h>
#include <mitsuba/render/records.h>
#include <drjit/util.h>
#include <mitsuba/render/scene.h>
#include <algorithm>
#include <tuple>

#if defined(MI_ENABLE_EMBREE)
    #include <embree3/rtcore.h>
#endif

#if defined(MI_ENABLE_CUDA)
# if defined(MI_USE_OPTIX_HEADERS)
    #include <optix_function_table_definition.h>
# endif
#endif

NAMESPACE_BEGIN(mitsuba)
namespace {

// ---------------------------------------------------------------------------
// Helpers to unify JIT and scalar code paths in several Mesh functions
// ---------------------------------------------------------------------------

/// Invoke ``func(UInt32)`` for every index in ``[0, count)``.
template <typename UInt32, typename Func>
void foreach_index(size_t count, Func &&func) {
    if constexpr (dr::is_jit_v<UInt32>) {
        func(dr::arange<UInt32>(count));
    } else {
        for (uint32_t i = 0; i < (uint32_t) count; ++i)
            func(i);
    }
}

/// Evaluate map[idx], treating an empty map as the identity
template <typename Index, typename Buffer>
DRJIT_INLINE Index gather_map(const Buffer &map, const Index &idx) {
    if (map.empty())
        return idx;

    if constexpr (std::is_integral_v<Index>)
        return map.data()[idx];
    else
        return dr::gather<Index>(map, idx);
}

/// Construct the array [func(i/dim, i%dim) for i in range(rows*dim)],
/// where ``func`` returns one lane
template <typename Buf, typename F>
Buf element_view(size_t rows, uint32_t dim, F &&func) {
    if (rows == 0)
        return Buf();
    if constexpr (dr::is_jit_v<Buf>) {
        using UInt32 = dr::uint32_array_t<Buf>;
        UInt32 j    = dr::arange<UInt32>(rows * dim),
               r    = j / dim,
               lane = j - r * dim;
        return func(r, lane);
    } else {
        Buf result = dr::empty<Buf>(rows * dim);
        auto *dst = result.data();
        for (size_t r = 0; r < rows; ++r)
            for (uint32_t lane = 0; lane < dim; ++lane)
                *dst++ = func((uint32_t) r, lane);
        return result;
    }
}

/// Selects how `interleaved()` evaluates in JIT variants
enum Eval { Eager, Symbolic };

/// Construct the array [func(i/N)[i%N] for i in range(count*N)], where
/// ``func`` returns a whole row. ``Eager`` scatters into fresh storage,
/// ``Symbolic`` builds an unevaluated lane selection.
template <size_t N, typename Buf, Eval E = Eager, typename Func>
Buf interleaved(size_t count, Func &&func) {
    using UInt32 = dr::uint32_array_t<Buf>;
    if constexpr (dr::is_jit_v<Buf> && E == Symbolic) {
        if (count == 0)
            return Buf();
        UInt32 j    = dr::arange<UInt32>(count * N),
               r    = j / (uint32_t) N,
               lane = j - r * (uint32_t) N;
        auto v = func(r);
        Buf result = v[N - 1];
        for (size_t k = N - 1; k-- > 0; )
            result = dr::select(lane == (uint32_t) k, v[k], result);
        return result;
    } else {
        Buf result = dr::empty<Buf>(count * N);
        if constexpr (dr::is_jit_v<Buf>) {
            UInt32 i = dr::arange<UInt32>(count);
            dr::scatter(result, func(i), i, true, ReduceMode::NoConflicts);
        } else {
            auto *dst = result.data();
            for (uint32_t i = 0; i < (uint32_t) count; ++i)
                dr::store(dst + N * (size_t) i, func(i));
        }
        return result;
    }
}

/// Gather the ``Dim``-wide row ``idx`` of an interleaved tensor or buffer
template <size_t Dim, typename Source, typename Index>
DRJIT_INLINE auto deinterleave(const Source &src, const Index &idx) {
    if constexpr (dr::is_tensor_v<Source>) {
        return deinterleave<Dim>(src.array(), idx);
    } else if constexpr (std::is_integral_v<Index>) {
        using Value = dr::scalar_t<Source>;
        return dr::load<dr::Array<Value, Dim>>(src.data() + Dim * idx);
    } else {
        return dr::gather<dr::Array<Source, Dim>>(src, idx);
    }
}

/// Add ``value`` to the ``Dim``-wide row ``idx`` of the interleaved buffer ``buf``
template <size_t Dim, typename Buffer, typename Value, typename Index,
          typename Mask>
DRJIT_INLINE void interleaved_add(Buffer &buf, const Value &value,
                                  const Index &idx, const Mask &active) {
    Index base = Dim * idx;
    for (size_t k = 0; k < Dim; ++k)
        dr::scatter_reduce(ReduceOp::Add, buf, value[k],
                           Index(base + (uint32_t) k), active);
}

/// Build a first-vertex representative map of ``map``
template <typename IndexBuffer>
static IndexBuffer build_rep(const IndexBuffer &map, size_t groups) {
    size_t n = map.size();

    if (n == 0)
        return IndexBuffer();

    IndexBuffer rep = dr::full<IndexBuffer>(0xFFFFFFFFu, groups);
    if constexpr (dr::is_jit_v<IndexBuffer>) {
        dr::scatter_reduce(ReduceOp::Min, rep, dr::arange<IndexBuffer>(n), map);
    } else {
        // Descending order, so that the first vertex of a group wins
        auto *dst = rep.data();
        const auto *src = map.data();
        for (size_t i = n; i-- > 0; )
            dst[src[i]] = (uint32_t) i;
    }
    return rep;
}

}

MI_VARIANT Mesh<Float, Spectrum>::Mesh(const Properties &props) : Base(props) {
    // Use per-face instead of per-vertex normals? This will give a faceted appearance.
    m_face_normals = props.get<bool>("face_normals", false);
    m_flip_normals = props.get<bool>("flip_normals", false);

    if (props.has_property("filename")) {
        m_source_path = file_resolver()->resolve(
            props.get<std::string_view>("filename"));
        if (!fs::exists(m_source_path))
            Throw("Mesh file \"%s\" not found!", m_source_path.string());
        m_filename = m_source_path.filename().string();
    } else {
        m_filename = std::string(props.id());
    }

    m_discontinuity_types = (uint32_t) DiscontinuityFlags::PerimeterType;
    m_shape_type = ShapeType::Mesh;
}

MI_VARIANT Mesh<Float, Spectrum>::Mesh(std::string_view name,
                                       bool face_normals, bool flip_normals) {
    this->set_id(name);
    m_filename = std::string(name);
    m_face_normals = face_normals;
    m_flip_normals = flip_normals;
    m_discontinuity_types = (uint32_t) DiscontinuityFlags::PerimeterType;
    m_shape_type = ShapeType::Mesh;

    m_bsdf = PluginManager::instance()->create_object<BSDF>(
        Properties("diffuse"));
}

/// Check the shape of a ``(rows, dim)`` tensor and return its row count.
/// Empty tensors yield zero. ``ctx`` names the caller in error messages.
template <typename Tensor>
static size_t check_rows(const Tensor &t, size_t dim, const char *name,
                         std::string_view ctx) {
    if (t.array().size() == 0)
        return (size_t) 0;
    if (t.ndim() != 2 || t.shape(1) != dim)
        Throw("%s: '%s' must be a (rows, %zu) tensor.", ctx, name, dim);
    return t.shape(0);
}

MI_VARIANT Mesh<Float, Spectrum>::Mesh(std::string_view name,
                                       const TensorXu32 &faces,
                                       const TensorXf32 &positions,
                                       const TensorXf32 &normals,
                                       const TensorXf32 &texcoords,
                                       const IndexBuffer &position_index,
                                       const IndexBuffer &normal_index,
                                       const IndexBuffer &bsdf_index,
                                       bool face_normals, bool flip_normals)
    : Mesh(name, face_normals, flip_normals) {
    from_fields(faces, positions, normals, texcoords, position_index,
                normal_index, bsdf_index);
}

/// Raise when the mesh was already built
static void require_unbuilt(bool built, const std::string &filename,
                            const char *ctx) {
    if (built)
        Throw("Mesh::%s(): mesh \"%s\" was already built. Use the \n"
              "parameter interface to change the mesh state.", ctx, filename);
}

/// Raise when a placement or orientation is still pending, see
/// `from_packed()` and `PackedMesh::set_transform()`
static void require_baked(bool pending, const std::string &filename) {
    if (pending)
        Throw("Mesh::from_packed(): mesh \"%s\": it's the caller's "
              "responsibility to apply 'to_world' and orientation changes "
              "when constructing a Mesh from a packed representation.",
              filename);
}

MI_VARIANT
void Mesh<Float, Spectrum>::from_fields(const TensorXu32 &faces,
                                        const TensorXf32 &positions,
                                        const TensorXf32 &normals,
                                        const TensorXf32 &texcoords,
                                        const IndexBuffer &position_index,
                                        const IndexBuffer &normal_index,
                                        const IndexBuffer &bsdf_index) {
    require_unbuilt(m_built, m_filename, "from_fields");
    drop_views();

    if (m_to_world.scalar() != ScalarAffineTransform4f())
        Throw("from_fields(): mesh \"%s\": please preapply any 'to_world' "
              "transformations to the provided fields (better) or use the "
              "transform() method to apply it after construction (slower).",
              m_filename);

    if (!m_face_normals && normal_index.size() != 0 &&
        normals.array().empty())
        Throw("from_fields(): mesh \"%s\": passing 'normal_index' requires a "
              "'normals' array.", m_filename);

    m_faces          = faces;
    m_positions      = positions;
    m_texcoords      = texcoords;
    m_position_index = position_index;
    m_bsdf_index     = bsdf_index;
    m_normals        = m_face_normals ? TensorXf32() : normals;
    m_normal_index   = m_face_normals ? IndexBuffer() : normal_index;

    pack(/* regenerate_normals */ normals.array().empty(), m_flip_normals);
    m_flip_normals = false;
    drop_views();
}

MI_VARIANT void Mesh<Float, Spectrum>::drop_views() {
    m_positions  = TensorXf32(FloatBuffer(), { 0, 3 });
    m_normals    = TensorXf32(FloatBuffer(), { 0, 3 });
    m_texcoords  = TensorXf32(FloatBuffer(), { 0, 2 });
    m_faces      = TensorXu32(IndexBuffer(), { 0, 3 });
    m_tangents   = TensorXf32(FloatBuffer(), { 0, 3 });
    m_bsdf_index = IndexBuffer();
}

MI_VARIANT void
Mesh<Float, Spectrum>::from_packed(Layout layout,
                                   const TensorXu32 &packed_faces,
                                   const TensorXf32 &packed_vertices,
                                   const IndexBuffer &position_index,
                                   const IndexBuffer &normal_index,
                                   size_t position_count,
                                   size_t normal_count,
                                   const ScalarBoundingBox3f *bbox) {
    require_unbuilt(m_built, m_filename, "from_packed");
    require_baked(m_flip_normals ||
                  m_to_world.scalar() != ScalarAffineTransform4f(),
                  m_filename);
    drop_views();

    std::string ctx = tfm::format("from_packed(): mesh \"%s\"", m_filename);

    size_t vertex_count = check_rows(packed_vertices, MeshVertexStride,
                                     "packed_vertices", ctx),
           face_count   = check_rows(packed_faces, MeshFaceStride,
                                     "packed_faces", ctx);

    auto check_map = [&](const IndexBuffer &map, size_t groups,
                         const char *name, const char *count_name) {
        size_t w = map.size();
        if (w == 0)
            return;
        if (w != vertex_count)
            Throw("%s: '%s' has %zu entries, expected one per vertex (%zu).",
                  ctx, name, w, vertex_count);
        if (groups == 0)
            Throw("%s: '%s' requires a nonzero '%s'.", ctx, name, count_name);
    };

    check_map(position_index, position_count, "position_index",
              "position_count");
    check_map(normal_index, normal_count, "normal_index", "normal_count");

    bool layout_normals = has_flag(layout, Layout::Normals);
    if (has_flag(layout, Layout::Tangents) &&
        !(layout_normals && has_flag(layout, Layout::Texcoords)))
        Throw("%s: the 'Tangents' layout requires 'Normals' and "
              "'Texcoords'.", ctx);

    bool normals = layout_normals && !m_face_normals;
    if (!normals)
        layout &= ~(Layout::Normals | Layout::Tangents);

    m_vertex_count   = (ScalarSize) vertex_count;
    m_face_count     = (ScalarSize) face_count;
    m_position_count = (ScalarSize) (position_count ? position_count
                                                    : vertex_count);
    m_packed_vertices = packed_vertices.array();
    m_packed_faces    = packed_faces.array();
    m_position_index  = position_index;
    m_position_rep    = IndexBuffer();
    m_normal_rep      = IndexBuffer();
    m_layout          = layout;

    if (normals) {
        m_normal_count = (ScalarSize) (normal_count ? normal_count
                                                    : vertex_count);
        m_normal_index = normal_index;
    } else {
        m_normal_count = 0;
        m_normal_index = IndexBuffer();
    }

    // Records with usable frames are adopted verbatim. A mesh whose
    // normal/tangent state does not match the requirements will need
    // to be re-packed.
    if ((!normals && !m_face_normals) || needs_tangents() != packs_tangent()) {
        pack(/* regenerate_normals */ !normals && !m_face_normals);
        drop_views();
    } else {
        refresh(bbox);
        m_built = true;
    }
}

/// Rebind ``dst`` to ``view``, carrying over the AD identity of the previous value
template <typename Tensor> static void rebind(Tensor &dst, Tensor &&view) {
    using Array = std::decay_t<decltype(dst.array())>;
    if constexpr (dr::is_diff_v<Array>) {
        // replace_grad() hard-fails on mismatched widths
        if (dr::grad_enabled(dst.array()) &&
            dst.array().size() == view.array().size()) {
            dst = Tensor(dr::replace_grad(view.array(), dst.array()),
                         view.ndim(), view.shape().data());
            return;
        }
    }
    dst = std::move(view);
}

MI_VARIANT void Mesh<Float, Spectrum>::build_views() {
    dr::suspend_grad<Float> guard;

    size_t V = m_vertex_count,
           P = m_position_count,
           N = m_normal_count,
           F = m_face_count;

    bool normals   = has_normals(),
         texcoords = has_texcoords(),
         tangents  = packs_tangent();

    const FloatBuffer &packed = m_packed_vertices;

    m_faces = TensorXu32(
        element_view<IndexBuffer>(F, 3, [&](const UInt32 &f, const UInt32 &lane) {
            return dr::gather<UInt32>(m_packed_faces, f * 4u + lane);
        }),
        { F, 3 });

    if (has_face_bsdfs())
        m_bsdf_index = element_view<IndexBuffer>(
            F, 1, [&](const UInt32 &f, const UInt32 &) {
                return dr::gather<UInt32>(m_packed_faces, f * 4u + 3u) &
                       FaceBSDFIndexMask;
            });
    else
        m_bsdf_index = IndexBuffer();

    // Invert the index maps, reusing the cached result when the maps did
    // not change. An empty map is the identity and needs no inverse.
    if (m_position_rep.size() != (m_position_index.empty() ? 0 : P))
        m_position_rep = build_rep(m_position_index, P);

    if (!normals)
        m_normal_rep = IndexBuffer();
    else if (m_normal_rep.size() != (m_normal_index.empty() ? 0 : N))
        m_normal_rep = build_rep(m_normal_index, N);

    const IndexBuffer &position_rep = m_position_rep,
                      &normal_rep   = m_normal_rep;

    rebind(m_positions, TensorXf32(
        element_view<FloatBuffer>(P, 3, [&](const UInt32 &p, const UInt32 &lane) {
            return dr::gather<Float32>(
                packed, gather_map(position_rep, p) * MeshVertexStride + lane);
        }),
        { P, 3 }));

    if (normals)
        rebind(m_normals, TensorXf32(
            interleaved<3, FloatBuffer, Symbolic>(N, [&](const UInt32 &g) {
                UInt32 base = gather_map(normal_rep, g) * MeshVertexStride +
                              PackedFrameOffset;
                Vector<Float32, 3> f(dr::gather<Float32>(packed, base),
                                     dr::gather<Float32>(packed, base + 1u),
                                     dr::gather<Float32>(packed, base + 2u));
                return tangents ? Vector<Float32, 3>(frame_decode(f).first) : f;
            }),
            { N, 3 }));
    else
        m_normals = TensorXf32(FloatBuffer(), { 0, 3 });

    if (texcoords)
        rebind(m_texcoords, TensorXf32(
            element_view<FloatBuffer>(
                V, 2, [&](const UInt32 &v, const UInt32 &lane) {
                    return dr::gather<Float32>(
                        packed,
                        v * MeshVertexStride + PackedTexcoordOffset + lane);
                }),
            { V, 2 }));
    else
        m_texcoords = TensorXf32(FloatBuffer(), { 0, 2 });

    if (!tangents) {
        m_tangents = TensorXf32(FloatBuffer(), { 0, 3 });
    } else {
        m_tangents = TensorXf32(
            interleaved<3, FloatBuffer, Symbolic>(V, [&](const UInt32 &v) {
                UInt32 base = v * MeshVertexStride;
                Vector<Float32, 3> f;
                for (uint32_t k = 0; k < 3; ++k)
                    f[k] = dr::gather<Float32>(packed,
                                               base + PackedFrameOffset + k);
                return frame_decode(f).second;
            }),
            { V, 3 });
    }
}

MI_VARIANT void Mesh<Float, Spectrum>::ensure_views() const {
    if (unlikely(m_positions.array().empty()))
        const_cast<Mesh *>(this)->build_views();
}

MI_VARIANT void Mesh<Float, Spectrum>::validate(bool check_bounds) const {
    validate_impl(check_bounds, /* updating */ false);
}

MI_VARIANT void Mesh<Float, Spectrum>::validate_impl(bool check_bounds,
                                                     bool updating) const {
    ensure_views();

    std::string ctx = tfm::format("Mesh \"%s\"", m_filename);

    size_t F = check_rows(m_faces, 3, "faces", ctx),
           P = check_rows(m_positions, 3, "positions", ctx),
           N = check_rows(m_normals, 3, "normals", ctx),
           T = check_rows(m_texcoords, 2, "texcoords", ctx),
           V = m_position_index.empty() ? P : m_position_index.size();

    if (F == 0 || P == 0)
        Throw("Mesh \"%s\": the mesh (%zu faces, %zu surface "
              "positions) must have at least one face and one vertex.",
              m_filename, F, P);

    // Rewriting 'positions' or 'faces' can change the vertex and face counts,
    // which strands every field that was not written alongside them
    auto stale = [updating](const char *field) {
        if (!updating)
            return std::string();
        return tfm::format(" Write a matching '%s' in the same batch, or "
                           "assign an empty tensor to drop it.", field);
    };

    if (T != 0 && T != V)
        Throw("Mesh \"%s\": 'texcoords' has %zu rows, expected one per "
              "vertex (%zu).%s", m_filename, T, V, stale("texcoords"));

    size_t wn = m_normal_index.size();
    if (wn != 0 && wn != V)
        Throw("Mesh \"%s\": 'normal_index' has %zu entries, expected one per "
              "vertex (%zu).%s", m_filename, wn, V, stale("normal_index"));
    if (N != 0 && wn == 0 && N != V)
        Throw("Mesh \"%s\": 'normals' has %zu rows, expected one per vertex "
              "(%zu) or an explicit 'normal_index' map.%s", m_filename, N, V,
              stale("normals"));

    size_t wb = m_bsdf_index.size();
    if (wb != 0 && wb != F)
        Throw("Mesh \"%s\": 'bsdf_index' has %zu entries, expected one per "
              "face (%zu).%s", m_filename, wb, F, stale("bsdf_index"));

    if (packs_tangent() && !(has_normals() && has_texcoords()))
        Throw("Mesh \"%s\": tangent computation requires both normals and "
              "texture coordinates.", m_filename);

    for (const auto &[name, attr] : m_mesh_attributes) {
        bool vertex = is_vertex_attribute(name);
        size_t arows = vertex ? V : F;
        if (attr.data.ndim() != 2 || attr.data.shape(0) != arows ||
            attr.data.shape(1) != attr.dim)
            Throw("Mesh \"%s\": attribute '%s' must be a (%zu, %zu) tensor "
                  "with one row per %s.", m_filename, name, arows,
                  (size_t) attr.dim, vertex ? "vertex" : "face");
    }

    if (!check_bounds)
        return;

    auto check_range = [&](const IndexBuffer &b, size_t limit,
                           const char *name) {
        if (b.empty())
            return;

        uint32_t max;
        if constexpr (dr::is_jit_v<Float>)
            max = dr::max(b)[0];
        else
            max = dr::max(b);

        if ((size_t) max >= limit)
            Throw("Mesh \"%s\": '%s' contains the out-of-range index %u "
                  "(valid range: [0, %zu)).", m_filename, name, max, limit);
    };

    check_range(m_faces.array(), V, "faces");
    check_range(m_position_index, P, "position_index");
    check_range(m_normal_index, m_normal_count, "normal_index");
}

MI_VARIANT
void Mesh<Float, Spectrum>::pack(bool regenerate_normals, bool flip_normals,
                                 bool updating) {
    ensure_views();

    size_t P = m_positions.ndim() == 2 ? m_positions.shape(0) : 0,
           V = m_position_index.empty() ? P : m_position_index.size();

    if (m_face_normals) {
        m_normals      = TensorXf32();
        m_normal_index = IndexBuffer();
        m_normal_count = 0;
    } else if (regenerate_normals) {
        m_normals = TensorXf32();
        size_t covered = m_normal_index.empty() ? m_normal_count
                                                : m_normal_index.size();
        if (covered != V) {
            // 'm_normal_index' no longer has the right size following a topology change
            m_normal_index = m_position_index;
            m_normal_count = (ScalarSize) P;
            m_normal_rep   = m_position_rep;
        }
    } else {
        size_t N = m_normals.ndim() == 2 ? m_normals.shape(0) : 0;
        if (N == 0)
            Throw("Mesh \"%s\": writing an empty 'normals' tensor is not "
                  "supported.", m_filename);

        // An empty index map is the identity. Normals at surface
        // position granularity adopt the position map instead
        if (m_normal_index.empty() && N != V && N == P) {
            m_normal_index = m_position_index;
            m_normal_rep   = m_position_rep;
        }
        m_normal_count = (ScalarSize) N;
    }

    validate_impl(/* check_bounds */ false, updating);

    // The directed edge data structure depends on the position count.
    if (m_dedge && m_dedge->vertex_count() != P) {
        m_dedge = nullptr;
        m_sil_dedge_pmf = DiscreteDistribution<Float>();
    }

    m_face_count     = (ScalarSize) m_faces.shape(0);
    m_position_count = (ScalarSize) P;
    m_vertex_count   = (ScalarSize) V;

    bool normals   = !m_face_normals,
         texcoords = !m_texcoords.array().empty(),
         tangents  = normals && texcoords && m_bsdf &&
                     has_flag(m_bsdf->flags(), BSDFFlags::NeedsTangents),
         has_bsdf  = !m_bsdf_index.empty();

    m_layout = make_layout(normals, texcoords, tangents, has_bsdf);

    if (normals && regenerate_normals)
        m_normals = compute_normals();

    TensorXf32 tan;
    if (tangents)
        tan = compute_tangents();

    m_packed_faces = interleaved<MeshFaceStride, IndexBuffer>(
        m_face_count, [&](const UInt32 &f) {
            Vector3u fi = deinterleave<3>(m_faces, f);
            if (flip_normals)
                fi = Vector3u(fi[2], fi[1], fi[0]);

            UInt32 flags = has_bsdf ? dr::gather<UInt32>(m_bsdf_index, f)
                                    : UInt32(0);

            if (tangents) {
                auto uv = [&](const UInt32 &v) {
                    return dr::detach(deinterleave<2>(m_texcoords, v));
                };
                auto uv0 = uv(fi[0]), duv0 = uv(fi[1]) - uv0,
                                      duv1 = uv(fi[2]) - uv0;
                auto flipped =
                    dr::fmsub(duv0.x(), duv1.y(), duv0.y() * duv1.x()) < 0.f;

                flags |= dr::select(flipped, UInt32(FaceUVFlipped), UInt32(0));
            }

            return dr::Array<UInt32, MeshFaceStride>(fi[0], fi[1], fi[2], flags);
        });

    m_packed_vertices = interleaved<MeshVertexStride, FloatBuffer>(
        m_vertex_count, [&](const UInt32 &v) {
            // Lanes that the layout does not use stay zero
            PackedVertex rec(0.f);

            Vector<Float32, 3> pos =
                deinterleave<3>(m_positions, gather_map(m_position_index, v));
            for (uint32_t k = 0; k < 3; ++k)
                rec[PackedPositionOffset + k] = pos[k];

            if (normals) {
                Vector<Float32, 3> f(
                    deinterleave<3>(m_normals, gather_map(m_normal_index, v)));
                if (flip_normals)
                    f = -f;
                if (tangents)
                    f = frame_encode(
                        Normal<Float32, 3>(f),
                        Vector<Float32, 3>(deinterleave<3>(tan, v)));
                for (uint32_t k = 0; k < 3; ++k)
                    rec[PackedFrameOffset + k] = f[k];
            }

            if (texcoords) {
                Vector<Float32, 2> uv = deinterleave<2>(m_texcoords, v);
                rec[PackedTexcoordOffset]     = uv[0];
                rec[PackedTexcoordOffset + 1] = uv[1];
            }

            return rec;
        });

    refresh();
    m_built = true;
}

MI_VARIANT void Mesh<Float, Spectrum>::from_packed(PackedMesh &&data) {
    require_unbuilt(m_built, m_filename, "from_packed");
    require_baked(m_flip_normals ||
                  m_to_world.scalar() != ScalarAffineTransform4f(),
                  m_filename);

    static constexpr JitBackend Backend = dr::backend_v<Float>;
    std::string ctx = tfm::format("from_packed(): mesh \"%s\"", m_filename);

    size_t V = data.vertex_count, F = data.face_count;

    ScalarBoundingBox3f bbox = data.bbox;
    const float *vp = data.vertices.data();
    if (!bbox.valid() && V > 0) {
        for (size_t v = 0; v < V; ++v) {
            const float *r = vp + v * MeshVertexStride;
            bbox.expand(ScalarPoint3f(r[0], r[1], r[2]));
        }
    }

    // Upload or adopt each buffer exactly once
    auto adopt = [](auto &&buf, size_t n) {
        using T = std::decay_t<decltype(*buf.data())>;
        using Buf = std::conditional_t<std::is_same_v<T, float>,
                                       FloatBuffer, IndexBuffer>;
        if (n == 0)
            return Buf();
        if constexpr (Backend == JitBackend::LLVM) {
            if (n == buf.size()) // Adopt only if the buffer has the right size
                return Buf::map_(buf.release(), n, /* free */ true);
        } else if constexpr (Backend != JitBackend::None) {
            // Schedule an asynchronous device copy from the staging buffer
            using Detached = dr::detached_t<Buf>;
            Buf result = Detached::steal(jit_var_mem_copy(
                Backend, Detached::Type, buf.data(), n, 0));
            buf.reset();
            return result;
        }
        Buf result = dr::load<Buf>(buf.data(), n);
        buf.reset();
        return result;
    };

    FloatBuffer vertices = adopt(data.vertices, V * MeshVertexStride);
    IndexBuffer faces = adopt(data.faces, F * MeshFaceStride),
                pidx  = adopt(data.position_index, data.position_count ? V : 0),
                nidx  = adopt(data.normal_index,   data.normal_count   ? V : 0);

    from_packed(data.layout,
                TensorXu32(std::move(faces), { F, MeshFaceStride }),
                TensorXf32(std::move(vertices), { V, MeshVertexStride }),
                pidx, nidx, data.position_count, data.normal_count, &bbox);

    // In spectral variants, color data converts to rgb2spec coefficients
    // in-place in the staging buffer, unless the producer opts out.
    for (PackedMesh::Attribute &a : data.attrs) {
        size_t rows = is_vertex_attribute(a.name) ? V : F;
        if (a.values.size() < rows * a.dim)
            Throw("%s: attribute \"%s\" has %zu values, expected %zu.",
                  ctx, a.name, a.values.size(), rows * a.dim);

        if (a.upsample_srgb && holds_rgb2spec_coeffs(a.name, a.dim))
            to_rgb2spec_coeffs(a.values.data(), rows);

        m_mesh_attributes.insert(
            { a.name,
              { (uint32_t) a.dim, TensorXf32(adopt(a.values, rows * a.dim),
                                             { rows, a.dim }) } });
    }
}

MI_VARIANT void Mesh<Float, Spectrum>::from_corners(const CornerMesh &desc) {
    require_unbuilt(m_built, m_filename, "from_corners");

    PackedMesh data =
        corner_to_packed_mesh(dr::backend_v<Float>, desc, m_filename,
                              has_face_normals(), m_flip_normals,
                              m_to_world.scalar());
    m_flip_normals = false;
    m_to_world = ScalarAffineTransform4f();
    from_packed(std::move(data));
}

MI_VARIANT bool Mesh<Float, Spectrum>::needs_tangents() const {
    return has_tangents() && m_bsdf &&
           has_flag(m_bsdf->flags(), BSDFFlags::NeedsTangents);
}

MI_VARIANT typename Mesh<Float, Spectrum>::TensorXu32
Mesh<Float, Spectrum>::geometric_faces() const {
    if (m_position_index.empty())
        return faces();

    return TensorXu32(
        element_view<IndexBuffer>(
            m_face_count, 3, [&](const UInt32 &f, const UInt32 &lane) {
                UInt32 vi = dr::gather<UInt32>(m_packed_faces, f * 4u + lane);
                return gather_map(m_position_index, vi);
            }),
        { m_face_count, 3 });
}

MI_VARIANT
void Mesh<Float, Spectrum>::refresh(const ScalarBoundingBox3f *bbox) {
    if (bbox)
        m_bbox = *bbox;
    else
        recompute_bbox();

    // Eagerly build sampling tables for emitters/sensors
    m_area_pmf = DiscreteDistribution<Float>();
    if (m_emitter || m_sensor)
        build_pmf();

    m_parameterization = nullptr;
    if (m_scene && needs_parameterization())
        build_parameterization();

    // The silhouette density depends on the vertex positions, which may have
    // just moved. The adjacency is purely combinatorial and survives.
    m_sil_dedge_pmf = DiscreteDistribution<Float>();

#if defined(MI_ENABLE_LLVM) && !defined(MI_ENABLE_EMBREE)
    m_packed_vertices_ptr = m_packed_vertices.data();
    m_packed_faces_ptr = m_packed_faces.data();
#endif

    mark_dirty();

    if (!m_initialized)
        Base::initialize();

    // Potentially rebuild views into the new packed state
    if (m_built && !m_positions.array().empty())
        build_views();
}

MI_VARIANT void Mesh<Float, Spectrum>::traverse(TraversalCallback *cb) {
    Base::traverse(cb);

    ensure_views();

    cb->put("positions", m_positions,
            ParamFlags::Differentiable | ParamFlags::Discontinuous);

    if (has_normals())
        cb->put("normals", m_normals,
                ParamFlags::Differentiable | ParamFlags::Discontinuous);

    if (has_texcoords())
        cb->put("texcoords", m_texcoords, ParamFlags::Differentiable);

    cb->put("faces", m_faces,
            ParamFlags::NonDifferentiable | ParamFlags::Discontinuous);
    cb->put("bsdf_index", m_bsdf_index, ParamFlags::NonDifferentiable);
    cb->put("position_index", m_position_index,
            ParamFlags::NonDifferentiable | ParamFlags::Discontinuous);
    cb->put("normal_index", m_normal_index, ParamFlags::NonDifferentiable);

    for (auto &[name, attr] : m_mesh_attributes)
        cb->put(name, attr.data, ParamFlags::Differentiable);
}

MI_VARIANT void Mesh<Float, Spectrum>::parameters_changed(const std::vector<std::string> &keys) {
    bool all = keys.empty();
    auto has = [&](const char *k) { return all || string::contains(keys, k); };

    bool topology = has("faces") || has("position_index"),
         fields   = topology || has("positions") || has("normals") ||
                    has("normal_index") || has("texcoords") ||
                    has("bsdf_index");

    if (fields) {
        // The group count of a written 'normal_index' would only be
        // knowable from a device reduction
        if (has("normal_index") && !has("normals"))
            Throw("parameters_changed(): mesh \"%s\": writing 'normal_index' "
                  "requires writing 'normals' in the same batch.", m_filename);

        if (topology) {
            m_dedge = nullptr;
            // Derived from the pairing, so it cannot outlive it
            m_sil_dedge_pmf = DiscreteDistribution<Float>();
        }

        // The inverse index maps follow the maps they were built from
        if (has("position_index"))
            m_position_rep = IndexBuffer();
        if (has("normal_index"))
            m_normal_rep = IndexBuffer();

        pack(/* regenerate_normals */
             (topology || has("positions")) && !has("normals"),
             /* flip_normals */ false, /* updating */ true);
    } else if (needs_tangents() != packs_tangent()) {
        // Nothing of the mesh itself changed, but the notification may
        // originate from the attached BSDF, whose flags decide the layout
        pack(false);
    }

    // Schedule the written attributes for evaluation. An attribute-only
    // batch skips pack() and hence needs an explicit validation call.
    bool attributes = false;
    for (const std::string &k : keys) {
        auto it = m_mesh_attributes.find(k);
        if (it == m_mesh_attributes.end())
            continue;
        dr::eval(it->second.data.array());
        attributes = true;
    }

    if (attributes && !fields)
        validate_impl(/* check_bounds */ false, /* updating */ true);

    Base::parameters_changed(keys);
}

MI_VARIANT typename Mesh<Float, Spectrum>::ScalarBoundingBox3f
Mesh<Float, Spectrum>::bbox() const {
    return m_bbox;
}

MI_VARIANT typename Mesh<Float, Spectrum>::ScalarBoundingBox3f
Mesh<Float, Spectrum>::bbox(ScalarIndex index) const {
    if constexpr (dr::is_cuda_v<Float> || dr::is_metal_v<Float>)
        Throw("bbox(ScalarIndex) is not available in GPU mode!");

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

MI_VARIANT void
Mesh<Float, Spectrum>::set_bsdf(typename Mesh<Float, Spectrum>::BSDF *bsdf) {
    bool backside_changed =
        !m_bsdf || (bsdf && (has_flag(m_bsdf->flags(), BSDFFlags::BackSide) !=
                             has_flag(bsdf->flags(), BSDFFlags::BackSide)));
    m_bsdf = bsdf;

    // The silhouette density depends on whether the BSDF is single-sided
    if (backside_changed)
        m_sil_dedge_pmf = DiscreteDistribution<Float>();

    if (m_built && needs_tangents() != packs_tangent())
        pack(false);
}

MI_VARIANT void Mesh<Float, Spectrum>::write_ply(const fs::path &filename) const {
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
    if (position_count() != m_vertex_count ||
        (has_normals() && normal_count() != m_vertex_count))
        Log(Warn, "write_ply(\"%s\"): the mesh has %u surface points, %u normal"
                  " groups, and %u vertices. Only seamless meshes where these agree are supported.",
            m_filename, m_position_count, m_normal_count, m_vertex_count);

    const FloatBuffer &vertices = dr::migrate(m_packed_vertices, JitBackend::None);
    const IndexBuffer &faces = dr::migrate(m_packed_faces, JitBackend::None);

    using NamedAttribute = std::pair<std::string, MeshAttribute>;
    std::vector<NamedAttribute> vertex_attributes;
    std::vector<NamedAttribute> face_attributes;

    for (const auto&[name, attribute]: m_mesh_attributes) {
        bool vertex = is_vertex_attribute(name);
        std::vector<NamedAttribute> &target =
            vertex ? vertex_attributes : face_attributes;
        // Strip the domain prefix, which PLY does not carry
        target.push_back({ name.substr(vertex ? 7 : 5),
                           attribute.migrate(JitBackend::None) });
    }
    // Evaluate buffers if necessary
    if constexpr (dr::is_jit_v<Float>)
        dr::sync_thread();

    stream->write_line("ply");
    if (sj::native_byte_order() == sj::ByteOrder::BigEndian)
        stream->write_line("format binary_big_endian 1.0");
    else
        stream->write_line("format binary_little_endian 1.0");

    stream->write_line(tfm::format("element vertex %i", m_vertex_count));
    stream->write_line("property float x");
    stream->write_line("property float y");
    stream->write_line("property float z");

    if (has_normals()) {
        stream->write_line("property float nx");
        stream->write_line("property float ny");
        stream->write_line("property float nz");
    }

    if (has_texcoords()) {
        stream->write_line("property float s");
        stream->write_line("property float t");
    }

    for (const auto&[name, attribute]: vertex_attributes)
        for (size_t i = 0; i < attribute.dim; ++i)
            stream->write_line(tfm::format("property float %s_%zu", name.c_str(), i));

    stream->write_line(tfm::format("element face %i", m_face_count));
    stream->write_line("property list uchar int vertex_indices");

    for (const auto&[name, attribute]: face_attributes)
        for (size_t i = 0; i < attribute.dim; ++i)
            stream->write_line(tfm::format("property float %s_%zu", name.c_str(), i));

    stream->write_line("end_header");

    // Write vertex data straight from the packed records
    const InputFloat *rec_base = vertices.data();
    bool tangents = packs_tangent();

    std::vector<const InputFloat*> vertex_attributes_ptr;
    for (const auto&[name, attribute]: vertex_attributes)
        vertex_attributes_ptr.push_back(attribute.data.array().data());

    for (size_t i = 0; i < m_vertex_count; i++) {
        const InputFloat *rec = rec_base + i * MeshVertexStride;
        stream->write(rec, 3 * sizeof(InputFloat));

        if (has_normals()) {
            InputVector3f f = dr::load<InputVector3f>(rec + PackedFrameOffset);
            InputNormal3f n = tangents ? frame_decode(f).first
                                       : InputNormal3f(f);
            InputFloat buf[3];
            dr::store(buf, n);
            stream->write(buf, 3 * sizeof(InputFloat));
        }

        if (has_texcoords())
            stream->write(rec + PackedTexcoordOffset,
                          2 * sizeof(InputFloat));

        for (size_t j = 0; j < vertex_attributes_ptr.size(); ++j) {
            const auto&[name, attribute] = vertex_attributes[j];
            stream->write(vertex_attributes_ptr[j], attribute.dim * sizeof(InputFloat));
            vertex_attributes_ptr[j] += attribute.dim;
        }
    }

    const ScalarIndex* face_ptr = faces.data();

    std::vector<const InputFloat*> face_attributes_ptr;
    for (const auto&[name, attribute]: face_attributes)
        face_attributes_ptr.push_back(attribute.data.array().data());

    // Write faces data
    uint8_t vertex_indices_count = 3;
    for (size_t i = 0; i < m_face_count; i++) {
        // Write vertex count
        stream->write(&vertex_indices_count, sizeof(uint8_t));

        // Write the vertex indices, skipping the per-face BSDF lane
        stream->write(face_ptr, 3 * sizeof(ScalarIndex));
        face_ptr += 4;

        for (size_t j = 0; j < face_attributes_ptr.size(); ++j) {
            const auto&[name, attribute] = face_attributes[j];
            stream->write(face_attributes_ptr[j], attribute.dim * sizeof(InputFloat));
            face_attributes_ptr[j] += attribute.dim;
        }
    }
}

MI_VARIANT void Mesh<Float, Spectrum>::write_serialized(const fs::path &filename) const {
    ref<FileStream> stream =
        new FileStream(filename, FileStream::ETruncReadWrite);

    Timer timer;
    Log(Info, "Writing mesh to \"%s\" ..", filename);
    write_serialized(stream);

    // Trailing dictionary indexing the single mesh in this file
    stream->write((uint64_t) 0);
    stream->write((uint32_t) 1);

    Log(Info, "\"%s\": wrote %i faces, %i vertices (in %s)", filename,
        m_face_count, m_vertex_count,
        util::time_string((float) timer.value()));
}

MI_VARIANT void Mesh<Float, Spectrum>::write_serialized(Stream *stream) const {
    const FloatBuffer &vertices_host = dr::migrate(m_packed_vertices, JitBackend::None);
    const IndexBuffer &faces_host    = dr::migrate(m_packed_faces, JitBackend::None);
    const IndexBuffer &pidx_host     = dr::migrate(m_position_index, JitBackend::None);
    const IndexBuffer &nidx_host     = dr::migrate(m_normal_index, JitBackend::None);

    using NamedAttribute = std::pair<std::string, MeshAttribute>;
    std::vector<NamedAttribute> attributes;
    for (const auto &[name, attribute] : m_mesh_attributes)
        attributes.push_back({ name, attribute.migrate(JitBackend::None) });
    std::sort(attributes.begin(), attributes.end(),
              [](const NamedAttribute &a, const NamedAttribute &b) {
                  return a.first < b.first;
              });

    if constexpr (dr::is_jit_v<Float>)
        dr::sync_thread();

    // The low flag bits carry the record layout verbatim
    uint32_t flags = (uint32_t) SerializedFlags::SinglePrecision |
                     (uint32_t) m_layout;
    if (has_face_normals())
        flags |= (uint32_t) SerializedFlags::FaceNormals;

    bool pmap = m_position_index.size() != 0,
         nmap = has_normals() && m_normal_index.size() != 0;

    stream->set_byte_order(Stream::ELittleEndian);
    stream->write(SerializedMagic);
    stream->write(SerializedVersion);

    ref<ZStream> z = new ZStream(stream);
    z->set_byte_order(Stream::ELittleEndian);
    z->write(flags);
    z->write(m_filename);
    z->write((uint64_t) m_vertex_count);
    z->write((uint64_t) m_face_count);
    z->write((uint64_t) (pmap ? m_position_count : 0));
    z->write((uint64_t) (nmap ? m_normal_count : 0));

    z->write_array(vertices_host.data(),
                   (size_t) m_vertex_count * MeshVertexStride);
    z->write_array(faces_host.data(),
                   (size_t) m_face_count * MeshFaceStride);
    if (pmap)
        z->write_array(pidx_host.data(), m_vertex_count);
    if (nmap)
        z->write_array(nidx_host.data(), m_vertex_count);

    z->write((uint32_t) attributes.size());
    for (const auto &[name, attribute] : attributes) {
        z->write(name);
        z->write((uint8_t) (holds_rgb2spec_coeffs(name, attribute.dim) ? 1
                                                                      : 0));
        z->write((uint32_t) attribute.dim);
        size_t rows = is_vertex_attribute(name) ? m_vertex_count
                                                : m_face_count;
        z->write_array(attribute.data.array().data(), rows * attribute.dim);
    }

    z->close();
}

MI_VARIANT void Mesh<Float, Spectrum>::recompute_normals() {
    // Tangents follow along: they regenerate in the new normals' plane
    pack(/* regenerate_normals */ true);
}

MI_VARIANT
void Mesh<Float, Spectrum>::transform(const AffineTransform4f &t) {
    bool normals  = has_normals(),
         tangents = packs_tangent();

    // Map each record through 't', leaving the texcoord lanes alone
    m_packed_vertices = interleaved<MeshVertexStride, FloatBuffer>(
        m_vertex_count, [&](const UInt32 &v) {
            constexpr uint32_t P = PackedPositionOffset,
                               F = PackedFrameOffset;

            PackedVertex rec = packed_vertex(v);

            Vector<Float32, 3> p(t * Point3f(rec[P], rec[P + 1], rec[P + 2])),
                               f(rec[F], rec[F + 1], rec[F + 2]);

            if (normals) {
                if (tangents) {
                    // The two transforms preserve the frame's orthogonality
                    auto [n, s] = frame_decode(f);
                    f = frame_encode(
                        Normal<Float32, 3>(dr::normalize(t * Normal3f(n))),
                        Vector<Float32, 3>(dr::normalize(t * Vector3f(s))));
                } else {
                    f = Vector<Float32, 3>(dr::normalize(t * Normal3f(f)));
                }
            }

            for (uint32_t k = 0; k < 3; ++k) {
                rec[P + k] = p[k];
                rec[F + k] = f[k];
            }

            return rec;
        });

    // A mirroring transform reverses the orientation of the geometry.
    if (dr::slice(dr::det(Matrix3f(t.matrix))) < 0.f)
        flip_winding();

    refresh();
}

MI_VARIANT void Mesh<Float, Spectrum>::flip_winding() {
    bool tangents = packs_tangent();

    m_packed_faces = interleaved<MeshFaceStride, IndexBuffer>(
        m_face_count, [&](const UInt32 &f) {
            PackedFace<UInt32> rec = packed_face(f);
            UInt32 flags = tangents ? rec[3] ^ UInt32(FaceUVFlipped) : rec[3];
            return PackedFace<UInt32>(rec[2], rec[1], rec[0], flags);
        });

    // Invalidate directed edge data structure
    m_dedge = nullptr;
    m_sil_dedge_pmf = DiscreteDistribution<Float>();
}

MI_VARIANT typename Mesh<Float, Spectrum>::TensorXf32
Mesh<Float, Spectrum>::compute_normals() const {
    if (!has_normals())
        Throw("compute_normals(): mesh \"%s\" is flat shaded and stores no "
              "vertex normals.", m_filename);

    // Weighting scheme based on "Computing Vertex Normals from Polygonal
    // Facets" by Grit Thuermer and Charles A. Wuethrich, JGT 1998, Vol 3.
    // Each face accumulates its unit normal into the normal groups of its
    // corners, weighted by the corner's interior angle.

    auto position = [&](const UInt32 &i) {
        return Point3f(
            deinterleave<3>(m_positions, gather_map(m_position_index, i)));
    };

    DynamicBuffer<Float> acc =
        dr::zeros<DynamicBuffer<Float>>(3 * m_normal_count);

    foreach_index<UInt32>(m_face_count, [&](const UInt32 &f) {
        Vector3u fi = deinterleave<3>(m_faces, f);

        Point3f p[3] = { position(fi[0]), position(fi[1]), position(fi[2]) };

        Vector3f n = dr::cross(p[1] - p[0], p[2] - p[0]);
        Float length_sqr = dr::squared_norm(n);
        Mask valid = length_sqr > 0.f;
        n *= dr::rsqrt(length_sqr);

        for (int k = 0; k < 3; ++k) {
            Float angle = unit_angle(dr::normalize(p[(k + 1) % 3] - p[k]),
                                     dr::normalize(p[(k + 2) % 3] - p[k]));

            interleaved_add<3>(acc, Vector3f(n * angle),
                               gather_map(m_normal_index, fi[k]), valid);
        }
    });

    // Normalize each group's normal; groups without a valid contribution
    // fall back to a bogus value
    FloatBuffer flat =
        interleaved<3, FloatBuffer>(m_normal_count, [&](const UInt32 &g) {
            Vector3f n = deinterleave<3>(acc, g);
            Float length_sqr = dr::squared_norm(n);
            return Vector<Float32, 3>(
                dr::select(length_sqr > 0.f, n * dr::rsqrt(length_sqr),
                           Vector3f(1.f, 0.f, 0.f)));
        });

    return TensorXf32(std::move(flat), { m_normal_count, 3 });
}

MI_VARIANT typename Mesh<Float, Spectrum>::TensorXf32
Mesh<Float, Spectrum>::compute_tangents() const {
    if (!has_texcoords() || !has_normals())
        Throw("compute_tangents(): texture coordinates and shading "
              "normals are required.");

    // Each corner of a non-degenerate triangle contributes it to the corner's
    // vertex, projected into the plane of the vertex normal and weighted by
    // the corner's interior angle.

    auto position = [&](const UInt32 &i) {
        return Point3f(
            deinterleave<3>(m_positions, gather_map(m_position_index, i)));
    };
    auto normal = [&](const UInt32 &i) {
        return Vector3f(
            deinterleave<3>(m_normals, gather_map(m_normal_index, i)));
    };
    auto texcoord = [&](const UInt32 &i) {
        return Point2f(deinterleave<2>(m_texcoords, i));
    };

    DynamicBuffer<Float> acc =
        dr::zeros<DynamicBuffer<Float>>(3 * m_vertex_count);

    foreach_index<UInt32>(m_face_count, [&](const UInt32 &f) {
        Vector3u fi = deinterleave<3>(m_faces, f);

        Point3f p[3] = { position(fi[0]), position(fi[1]), position(fi[2]) };
        Point2f uv[3] = { texcoord(fi[0]), texcoord(fi[1]), texcoord(fi[2]) };

        Vector2f t1 = uv[1] - uv[0], t2 = uv[2] - uv[0];
        Float area2 = dr::fmsub(t1.x(), t2.y(), t1.y() * t2.x());
        Vector3f vos = dr::fmsub(t2.y(), p[1] - p[0], t1.y() * (p[2] - p[0]));
        Float len = dr::norm(vos);

        // Faces that are degenerate in position or UV space (this
        // includes collapsed vertices) contribute nothing
        Mask valid =
            dr::abs(area2) > 0.f && len > 0.f &&
            dr::squared_norm(dr::cross(p[1] - p[0], p[2] - p[0])) > 0.f;
        vos *= dr::select(area2 > 0.f, 1.f, -1.f) / len;

        // Project an edge into the plane of the vertex normal and normalize
        auto project = [](const Vector3f &e, const Vector3f &n) {
            Vector3f d = dr::fnmadd(n, dr::dot(n, e), e);
            Float l = dr::norm(d);
            return dr::select(l > 0.f, d / l, Vector3f(0.f));
        };

        for (int k = 0; k < 3; ++k) {
            Vector3f n = normal(fi[k]);
            Vector3f t = dr::fnmadd(n, dr::dot(n, vos), vos);
            Float tl = dr::norm(t);

            Vector3f e1 = project(p[(k + 1) % 3] - p[k], n),
                     e2 = project(p[(k + 2) % 3] - p[k], n);

            // An edge parallel to the normal leaves the angle undefined,
            // which MikkTSpace resolves to a right angle
            Float angle =
                dr::select(dr::squared_norm(e1) * dr::squared_norm(e2) > 0.f,
                           unit_angle(e1, e2), dr::Pi<Float> * .5f);

            interleaved_add<3>(acc, Vector3f(t * (angle / tl)), fi[k],
                               valid && tl > 0.f);
        }
    });

    // Normalize each vertex's tangent. Vertices without a valid
    // contribution fall back to an arbitrary perpendicular direction
    FloatBuffer flat =
        interleaved<3, FloatBuffer>(m_vertex_count, [&](const UInt32 &v) {
            Vector3f t = deinterleave<3>(acc, v);
            Float len = dr::norm(t);
            return Vector<Float32, 3>(
                dr::select(len > 0.f, t / len,
                           coordinate_system(normal(v)).first));
        });

    return TensorXf32(std::move(flat), { m_vertex_count, 3 });
}

MI_VARIANT void Mesh<Float, Spectrum>::recompute_bbox() {
    m_bbox = reduce_bbox<
        /* Type = */ ScalarPoint3f,
        /* Stride = */ MeshVertexStride>(m_packed_vertices, m_vertex_count);
}

MI_VARIANT void Mesh<Float, Spectrum>::build_pmf() {
    if (m_face_count == 0)
        Throw("Cannot create sampling table for an empty mesh: %s", to_string());

    dr::scoped_eval_scope<Float> guard;

    DynamicBuffer<Float> area = interleaved<1, DynamicBuffer<Float>>(
        m_face_count, [&](const UInt32 &f) {
            Vector3u fi = face_indices(f);
            Point3f p0 = vertex_position(fi[0]),
                    p1 = vertex_position(fi[1]),
                    p2 = vertex_position(fi[2]);
            return dr::detach(.5f * dr::norm(dr::cross(p1 - p0, p2 - p0)));
        });

    m_area_pmf = DiscreteDistribution<Float>(std::move(area));
}

MI_VARIANT const typename Mesh<Float, Spectrum>::DirectedEdge *
Mesh<Float, Spectrum>::dedge() const {
    // Unsynchronized, which is fine in the expected usage (JIT variants),
    // where a single thread orchestrates the parallel computation
    if (!m_dedge) {
        // The guard must cover 'geometric_faces()' too: it is traced before
        // the constructor runs and would otherwise carry the symbolic mask
        dr::scoped_eval_scope<Float> guard;

        // Reporting defects would synchronize, see DirectedEdge::flag_count()
        m_dedge = new DirectedEdge(geometric_faces().array(), m_position_count,
                                   m_filename, /* warn_defects */ false);
    }

    return m_dedge.get();
}

MI_VARIANT const DiscreteDistribution<Float> &
Mesh<Float, Spectrum>::sil_dedge_pmf() const {
    if (!m_sil_dedge_pmf.empty())
        return m_sil_dedge_pmf;

    if constexpr (!dr::is_jit_v<Float>)
        Throw("Mesh::sil_dedge_pmf(): silhouette sampling is only available in "
              "JIT variants.");

    dr::scoped_eval_scope<Float> guard;

    UInt32 e = dr::arange<UInt32>(m_face_count * 3),
           e_oppo = dedge_opposite(e);
    Mask boundary = (e_oppo == DirectedEdge::Invalid);
    // One edge can be represented by two dedge indices, we use the smaller index
    Mask valid = (e_oppo > e) & !boundary;

    // The three positions of a face also determine its geometric normal, since
    // the cross product does not care where the triple starts. Both uses
    // therefore share a single set of loads. Nothing here is differentiated,
    // so the positions enter detached.
    Mask keep = valid || boundary;
    auto face_data = [&](const UInt32 &index, const Mask &active) {
        Vector3u vi = dedge_indices(index, active);
        dr::Array<Point3f, 3> p{ dr::detach(vertex_position(vi[0], active)),
                                 dr::detach(vertex_position(vi[1], active)),
                                 dr::detach(vertex_position(vi[2], active)) };
        return std::make_pair(p, face_normal(p[0], p[1], p[2]));
    };

    auto [q, n_curr] = face_data(e, keep);
    auto [p, n_oppo] = face_data(e_oppo, valid);

    valid &= dr::dot(n_curr, n_oppo) < 1.f; // Flat surfaces are not on the silhouette

    if (m_bsdf && !has_flag(m_bsdf->flags(), BSDFFlags::BackSide)) {
        // Concave surfaces do not contribute to visibility contours.
        Vector3f v_oppo = dr::normalize(p[2] - p[1]);
        valid &= dr::dot(n_curr, v_oppo) < 0.f;
    }

    Point3f p0 = dr::select(boundary, q[0], p[0]),
            p1 = dr::select(boundary, q[1], p[1]);

    Float weight = dr::zeros<Float>(m_face_count * 3);
    dr::masked(weight, valid || boundary) = dr::norm(p1 - p0);

    m_sil_dedge_pmf = DiscreteDistribution<Float>(weight);
    return m_sil_dedge_pmf;
}

MI_VARIANT MergeKey Mesh<Float, Spectrum>::merge_key() const {
    return { m_bsdf.get(), m_emitter.get(), m_sensor.get(),
             m_interior_medium.get(), m_exterior_medium.get(),
             (Layout) (m_layout & ~Layout::FaceBSDFs) };
}

MI_VARIANT
ref<Mesh<Float, Spectrum>>
Mesh<Float, Spectrum>::merge(const std::vector<Shape<Float, Spectrum> *> &shapes) {
    if (shapes.empty())
        Throw("Mesh::merge(): the shape list is empty!");

    std::vector<const Mesh *> meshes;
    meshes.reserve(shapes.size());
    for (Base *shape : shapes) {
        const Mesh *m = dynamic_cast<const Mesh *>(shape);
        if (!m)
            Throw("Mesh::merge(): the shape list may only contain meshes!");
        meshes.push_back(m);
    }

    const Mesh *first = meshes[0];
    if (meshes.size() == 1)
        return ref<Mesh>(const_cast<Mesh *>(first));

    // Metadata pass: check compatibility and compute the prefix offsets,
    // totals, bounding box, and name of the result
    MergeKey key = first->merge_key();

    size_t V = 0, F = 0, P = 0, N = 0;
    bool any_pmap = false, any_nmap = false, any_bsdf = false;
    ScalarBoundingBox3f bbox;

    for (const Mesh *m : meshes) {
        if (m->merge_key() != key || m->has_mesh_attributes())
            Throw("Mesh::merge(): the meshes are incompatible (%s and %s)!",
                  first->to_string(), m->to_string());

        V += m->m_vertex_count;   F += m->m_face_count;
        P += m->m_position_count; N += m->m_normal_count;
        any_pmap |= m->m_position_index.size() != 0;
        any_nmap |= m->m_normal_index.size() != 0;
        any_bsdf |= m->has_face_bsdfs();
        bbox.expand(m->m_bbox);
    }

    // Name the result after its inputs. A scene may merge many thousands of
    // meshes, so list only the first few and summarize the rest.
    constexpr size_t NameLimit = 4;
    size_t named = std::min(meshes.size(), NameLimit),
           extra = meshes.size() - named;

    std::string filename = "Merged mesh (" + first->m_filename;
    for (size_t i = 1; i < named; ++i) {
        // Oxford comma, except when the list has only two entries
        const char *sep = (i + 1 < named || extra) ? ", "
                          : (named > 2)            ? ", and "
                                                   : " and ";
        filename += sep + meshes[i]->m_filename;
    }
    if (extra)
        filename += tfm::format(", and %zu more", extra);
    filename += ")";

    bool normals = first->has_normals();
    any_nmap &= normals;

    // Allocate the final buffers once and copy/scatter each input
    FloatBuffer vertices = dr::empty<FloatBuffer>(V * MeshVertexStride);
    IndexBuffer faces = dr::empty<IndexBuffer>(F * MeshFaceStride);
    IndexBuffer pidx, nidx;
    if (any_pmap)
        pidx = dr::empty<IndexBuffer>(V);
    if (any_nmap)
        nidx = dr::empty<IndexBuffer>(V);

    size_t vbase = 0, fbase = 0, pbase = 0, nbase = 0;
    for (const Mesh *m : meshes) {
        size_t nv = m->m_vertex_count, nf = m->m_face_count;
        if (nv == 0)
            continue;

        // Opaque offsets keep the input-specific values out of the generated
        // code, so that every input after the first hits the kernel cache
        UInt32 voff = dr::opaque<UInt32>((uint32_t) vbase),
               foff = dr::opaque<UInt32>((uint32_t) fbase);

        if constexpr (dr::is_jit_v<Float>) {
            dr::scatter(vertices, m->m_packed_vertices,
                        dr::arange<IndexBuffer>(nv * MeshVertexStride) +
                            voff * MeshVertexStride,
                        true, ReduceMode::NoConflicts);

            if (nf > 0) {
                IndexBuffer j = dr::arange<IndexBuffer>(nf * MeshFaceStride);
                dr::scatter(faces,
                            dr::select((j & 3u) == 3u, m->m_packed_faces,
                                       m->m_packed_faces + voff),
                            j + foff * MeshFaceStride,
                            true, ReduceMode::NoConflicts);
            }
        } else {
            std::memcpy(vertices.data() + vbase * MeshVertexStride,
                        m->m_packed_vertices.data(),
                        nv * MeshVertexStride * sizeof(InputFloat));

            ScalarIndex *d = faces.data() + fbase * MeshFaceStride;
            const ScalarIndex *s = m->m_packed_faces.data();
            for (size_t j = 0; j < nf * MeshFaceStride; ++j)
                d[j] = s[j] + ((j & 3) == 3 ? 0u : (ScalarIndex) vbase);
        }

        // Copy one input's index map into its slice, renumbering the
        // entries into the merged surface point / normal group range
        auto copy_map = [&](IndexBuffer &dst, const IndexBuffer &map,
                            size_t base) {
            if constexpr (dr::is_jit_v<Float>) {
                IndexBuffer local = map.size()
                                        ? map
                                        : dr::arange<IndexBuffer>(nv);
                dr::scatter(dst, local + dr::opaque<UInt32>((uint32_t) base),
                            dr::arange<IndexBuffer>(nv) + voff,
                            true, ReduceMode::NoConflicts);
            } else {
                ScalarIndex *d = dst.data() + vbase;
                const ScalarIndex *s = map.empty() ? nullptr : map.data();
                for (size_t j = 0; j < nv; ++j)
                    d[j] = (ScalarIndex) (base + (s ? s[j] : j));
            }
        };

        if (any_pmap)
            copy_map(pidx, m->m_position_index, pbase);
        if (any_nmap)
            copy_map(nidx, m->m_normal_index, nbase);

        // Launch a kernel per mesh. Otherwise Dr.Jit will potentially fuses
        // many writes into a single large kernel that will be slow to compile.
        dr::eval(vertices, faces, pidx, nidx);

        vbase += nv; fbase += nf;
        pbase += m->m_position_count; nbase += m->m_normal_count;
    }

    Properties props;
    auto set_object = [&](const char *name, const Object *object) {
        if (object)
            props.set(name, const_cast<Object *>(object));
    };
    set_object("bsdf", first->m_bsdf.get());
    set_object("interior", first->m_interior_medium.get());
    set_object("exterior", first->m_exterior_medium.get());
    set_object("sensor", first->m_sensor.get());
    set_object("emitter", first->m_emitter.get());
    props.set("face_normals", first->has_face_normals());

    ref<Mesh> result = new Mesh(props);
    result->m_filename = filename;

    Layout layout = first->m_layout;
    if (any_bsdf)
        layout |= Layout::FaceBSDFs;

    result->from_packed(layout,
                        TensorXu32(std::move(faces), { F, MeshFaceStride }),
                        TensorXf32(std::move(vertices), { V, MeshVertexStride }),
                        pidx, nidx, any_pmap ? P : 0, any_nmap ? N : 0, &bbox);

    return result;
}

MI_VARIANT bool Mesh<Float, Spectrum>::needs_parameterization() const {
    return m_emitter &&
           has_flag(m_emitter->flags(), EmitterFlags::SpatiallyVarying) &&
           has_texcoords() && m_vertex_count > 0;
}

MI_VARIANT void Mesh<Float, Spectrum>::build_parameterization() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_parameterization)
        return; // already built!

    if (!has_texcoords())
        Throw("eval_parameterization(): mesh does not have UV coordinates!");

    Properties mesh_props;
    mesh_props.set_id(m_filename + "_param");
    // Nothing shades this helper mesh, so it needs no normals
    mesh_props.set("face_normals", true);
    ref<Mesh> mesh = new Mesh(mesh_props);

    const FloatBuffer &packed_host = dr::migrate(m_packed_vertices, JitBackend::None);
    if constexpr (dr::is_jit_v<Float>)
        dr::sync_thread();

    const InputFloat *ptr_uv = packed_host.data() + PackedTexcoordOffset;

    // The helper mesh places each vertex at (u, v, 0) and reuses the face records
    std::vector<InputFloat> rec((size_t) m_vertex_count * MeshVertexStride,
                                0.f);
    ScalarBoundingBox3f bbox;
    for (size_t i = 0; i < m_vertex_count; ++i) {
        InputFloat u = ptr_uv[MeshVertexStride * i],
                   v = ptr_uv[MeshVertexStride * i + 1];
        rec[i * MeshVertexStride + 0] = u;
        rec[i * MeshVertexStride + 1] = v;
        bbox.expand(ScalarPoint3f(u, v, 0.f));
    }

    mesh->from_packed(
        Layout::Positions,
        TensorXu32(m_packed_faces, { m_face_count, MeshFaceStride }),
        TensorXf32(dr::load<FloatBuffer>(rec.data(), rec.size()),
                   { m_vertex_count, MeshVertexStride }),
        IndexBuffer(), IndexBuffer(), 0, 0, &bbox);

    Properties props;
    props.set("mesh", mesh.get());

    if (m_scene)
        props.set("parent_scene", m_scene);

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
// Surface sampling routines
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

    if (has_texcoords()) {
        Point2f uv0 = vertex_texcoord(fi[0], active),
                uv1 = vertex_texcoord(fi[1], active),
                uv2 = vertex_texcoord(fi[2], active);

        ps.uv = dr::fmadd(uv0, (1.f - b.x() - b.y()),
                          dr::fmadd(uv1, b.x(), uv2 * b.y()));
    } else {
        ps.uv = b;
    }

    if (has_normals()) {
        Normal3f n0 = vertex_normal(fi[0], active),
                 n1 = vertex_normal(fi[1], active),
                 n2 = vertex_normal(fi[2], active);

        ps.n = dr::fmadd(n0, (1.f - b.x() - b.y()),
                         dr::fmadd(n1, b.x(), n2 * b.y()));
    } else {
        ps.n = dr::cross(e0, e1);
    }

    ps.n = dr::normalize(ps.n);

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
            ray, /* coherent = */ true, false, 0, 0, active);
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

// =============================================================

// =============================================================
// Silhouette sampling routines and other utilities
// =============================================================

MI_VARIANT typename Mesh<Float, Spectrum>::SilhouetteSample3f
Mesh<Float, Spectrum>::sample_silhouette(const Point3f &sample_,
                                         uint32_t flags,
                                         Mask active) const {
    MI_MASK_ARGUMENT(active);

    if (!has_flag(flags, DiscontinuityFlags::PerimeterType) ||
        !parameters_grad_enabled())
        return dr::zeros<SilhouetteSample3f>();

    SilhouetteSample3f ss = dr::zeros<SilhouetteSample3f>();

    /// Sample a point on one of the edges
    UInt32 e;
    Float sample_x;
    Float pmf_edge;
    std::tie(e, sample_x, pmf_edge) =
        sil_dedge_pmf().sample_reuse_pmf(sample_.x(), active);
    Point3f sample(sample_x, sample_.y(), sample_.z());
    active &= (pmf_edge != 0.f);

    UInt32 corner = DirectedEdge::corner(e);
    Vector3u vi = dedge_indices(e, active);
    Point3f p0 = vertex_position(vi[0], active),
            p1 = vertex_position(vi[1], active),
            p2 = vertex_position(vi[2], active);

    ss.p = dr::lerp(p0, p1, sample.x());

    // Face local barycentric UV coordinates
    ss.uv = dr::select(corner == 0u,
                       Point2f(sample.x(), 0.f),
                       Point2f(1 - sample.x(), sample.x()));
    ss.uv = dr::select(corner == 2u,
                       Point2f(0.f, 1 - sample.x()),
                       ss.uv);

    /// Sample a tangential direction at the point
    Normal3f n_curr = face_normal(DirectedEdge::face(e), active);

    UInt32 e_oppo = dedge_opposite(e, active);
    Mask has_opposite = (e_oppo != DirectedEdge::Invalid) & active;
    Normal3f n_oppo = face_normal(DirectedEdge::face(e_oppo), has_opposite);

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
    ss.projection_index = corner;
    ss.prim_index = DirectedEdge::face(e);
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
    if (!parameters_grad_enabled())
        return dr::zeros<Point3f>();

    // Safely ignore invalid boundary segments
    Mask active =
        active_ && (ss.discontinuity_type ==
                           (uint32_t) DiscontinuityFlags::PerimeterType);

    UInt32 e = ss.prim_index * 3u + ss.projection_index,
           e_oppo = dedge_opposite(e, active);

    // One edge can be represented by two dedge indices, we use the smaller index
    Mask swap = e > e_oppo;
    UInt32 e_tmp = e;
    dr::masked(e, swap) = e_oppo;
    dr::masked(e_oppo, swap) = e_tmp;

    Mask has_opposite = (e_oppo != DirectedEdge::Invalid) && active;
    Normal3f n_curr = face_normal(DirectedEdge::face(e), active),
             n_oppo = face_normal(DirectedEdge::face(e_oppo), has_opposite);

    Point3f sample = dr::zeros<Point3f>(dr::width(ss));
    const DiscreteDistribution<Float> &distr = sil_dedge_pmf();
    Float pmf = distr.eval_pmf(e, active),
          cdf = distr.eval_cdf(e, active);

    // Do not use `ss.prim_index`, because we might have swapped
    Vector3u vi = dedge_indices(e, active);
    Point3f p0 = vertex_position(vi[0], active),
            p1 = vertex_position(vi[1], active),
            p2 = vertex_position(vi[2], active);
    Float alpha = dr::norm(ss.p - p0) * dr::rcp(dr::norm(p1 - p0));

    // We sacrifice the last bit of precision to avoid numerical issues
    alpha = dr::clip(alpha, dr::Epsilon<Float>, 1.f - dr::Epsilon<Float>);

    dr::masked(sample.x(), active) =
        dr::fmadd(alpha - 1.f, pmf, cdf) * distr.normalization();

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

    // To obtain the silhouette sample on an edge, we do not project `si.p` to
    // the nearest edge, instead we randomly sample a point on any silhouette edge.
    // This ensures that the triangle corners do not receive minimal samples.

    // Shapes that are not differentiated have no silhouette to project onto,
    // and building adjacency for every mesh a ray happens to hit would be
    // wasteful.
    if (!parameters_grad_enabled())
        return dr::zeros<SilhouetteSample3f>();

    Vector3u fi = face_indices(si.prim_index, active);
    Vector3f p0 = vertex_position(fi[0], active),
             p1 = vertex_position(fi[1], active),
             p2 = vertex_position(fi[2], active);
    // Face geometry normals of the current and three neighboring triangles
    Vector3u e_oppo(dedge_opposite(si.prim_index * 3u     , active),
                    dedge_opposite(si.prim_index * 3u + 1u, active),
                    dedge_opposite(si.prim_index * 3u + 2u, active));
    dr::mask_t<Vector3u> boundary = (e_oppo == DirectedEdge::Invalid) && active;
    Vector3u prim_idx =
        dr::select(boundary, si.prim_index, DirectedEdge::face(e_oppo));
    Normal3f normal_0 = face_normal(prim_idx[0], active && !boundary[0]),
             normal_1 = face_normal(prim_idx[1], active && !boundary[1]),
             normal_2 = face_normal(prim_idx[2], active && !boundary[2]);

    Normal3f normal = face_normal(si.prim_index, active);

    // Compute the "viewing" angle of three neighboring triangles
    Vector3f ray_d_0 = dr::normalize(p0 - viewpoint),
             ray_d_1 = dr::normalize(p1 - viewpoint),
             ray_d_2 = dr::normalize(p2 - viewpoint);

    Vector3f cos_theta_oppo(
        dr::dot(ray_d_1, normal_0) * dr::sign(dr::dot(ray_d_1, normal)),
        dr::dot(ray_d_2, normal_1) * dr::sign(dr::dot(ray_d_2, normal)),
        dr::dot(ray_d_0, normal_2) * dr::sign(dr::dot(ray_d_0, normal)));

    // Boundary edges are always silhouettes
    dr::masked(cos_theta_oppo, boundary) = -1.f;

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
        weight = dr::select(cos_theta_oppo <= 0.f, max_weight, weight);

        // In case the weights are too small
        weight = dr::maximum(weight, dr::deg_to_rad(1.f));
        weight /= dr::sum(weight);

        ss.projection_index = dr::select(sample >= weight[0], 1u, 0u);
        ss.projection_index = dr::select(sample >= weight[0] + weight[1],
                                         2u, ss.projection_index);

        ss.prim_index = dr::select(sample >= weight[0],
                                   prim_idx[1], prim_idx[0]);
        ss.prim_index = dr::select(sample >= weight[0] + weight[1],
                                   prim_idx[2], ss.prim_index);

        failed_proj = ((ss.projection_index == 0u) && (cos_theta_oppo[0] > 0.f)) ||
                      ((ss.projection_index == 1u) && (cos_theta_oppo[1] > 0.f)) ||
                      ((ss.projection_index == 2u) && (cos_theta_oppo[2] > 0.f));
    } else {
        /// Project to any silhouette edge with equal probability.
        weight = dr::select(cos_theta_oppo < 0.f, 1.f, 0.f);
        Float sum = dr::sum(weight);

        // If none of the edges are on the silhouette, pick one uniformly
        failed_proj = (sum == 0.f);
        dr::masked(weight, failed_proj) = 1.f;
        dr::masked(sum, failed_proj) = 3.f;
        weight /= sum;

        ss.prim_index = si.prim_index;

        ss.projection_index = dr::select(sample >= weight[0], 1u, 0u);
        ss.projection_index = dr::select(sample >= weight[0] + weight[1],
                                         2u, ss.projection_index);
    }

    // Reuse sample
    sample = dr::select(ss.projection_index == 0u, sample / weight[0], sample);
    sample = dr::select(ss.projection_index == 1u,
                        (sample - weight[0]) / weight[1], sample);
    sample = dr::select(ss.projection_index == 2u,
                        (sample - weight[0] - weight[1]) / weight[2], sample);

    // Sample a point on the selected edge
    ss.p = dr::select(ss.projection_index == 1u, dr::lerp(p1, p2, sample),
                      dr::lerp(p0, p1, sample));
    ss.p = dr::select(ss.projection_index == 2u, dr::lerp(p2, p0, sample),
                      ss.p);

    ss.d = dr::normalize(ss.p - viewpoint);
    ss.shape = this;

    ss.discontinuity_type = dr::select(
        active & !failed_proj,
        (uint32_t) DiscontinuityFlags::PerimeterType,
        (uint32_t) DiscontinuityFlags::Empty);

    return ss;
}

MI_VARIANT
auto Mesh<Float, Spectrum>::precompute_silhouette(
    const ScalarPoint3f &viewpoint) const
    -> std::tuple<IndexBuffer, DynamicBuffer<Float>> {
    if (m_face_count == 0)
        return { IndexBuffer(), DynamicBuffer<Float>() };

    if constexpr (!dr::is_jit_v<Float>) {
        using Vec3f = ScalarVector3f;
        using Pt3f  = ScalarPoint3f;

        const InputFloat *V          = m_packed_vertices.data();
        const size_t vstride         = MeshVertexStride;
        const ScalarIndex *E2E_data  = dedge()->E2E().data();
        const ScalarIndex *face_data = m_packed_faces.data();

        ScalarIndex prim_count = 0u;
        std::vector<ScalarIndex> indices(m_face_count * 3u);
        std::vector<ScalarFloat> weight(m_face_count * 3u);
        ScalarFloat weight_sum = 0.f;

        for (ScalarIndex f = 0; f < m_face_count; f++) {
            ScalarPoint3u idx =
                dr::load<ScalarPoint3u>(face_data + (size_t) f * MeshFaceStride);
            Pt3f v0 = dr::load<Pt3f>(V + vstride * idx.x());
            Pt3f v1 = dr::load<Pt3f>(V + vstride * idx.y());
            Pt3f v2 = dr::load<Pt3f>(V + vstride * idx.z());
            Vec3f n = face_normal(v0, v1, v2);

            Vec3f to_v0 = dr::normalize(v0 - viewpoint);
            Vec3f to_v1 = dr::normalize(v1 - viewpoint);
            Vec3f to_v2 = dr::normalize(v2 - viewpoint);

            auto check_edge = [&](const ScalarIndex e,
                                  const Vec3f &dir1,
                                  const Vec3f &dir2) -> void {
                ScalarIndex e_oppo = dr::load<ScalarIndex>(E2E_data + e);
                bool valid = false;

                if (e_oppo == DirectedEdge::Invalid) {
                    valid = true;
                } else if (e_oppo > e) {
                    ScalarPoint3u v_idx_oppo = dr::load<ScalarPoint3u>(
                        face_data + (size_t) DirectedEdge::face(e_oppo) * MeshFaceStride);

                    Pt3f v0_oppo = dr::load<Pt3f>(V + vstride * v_idx_oppo.x());
                    Pt3f v1_oppo = dr::load<Pt3f>(V + vstride * v_idx_oppo.y());
                    Pt3f v2_oppo = dr::load<Pt3f>(V + vstride * v_idx_oppo.z());
                    Vec3f n_oppo = face_normal(v0_oppo, v1_oppo, v2_oppo);

                    if (dr::dot(dir1, n) * dr::dot(dir1, n_oppo) <= 0.f &&
                        dr::abs(dr::dot(n, n_oppo)) < 1.f) {
                        valid = true;
                    }
                }

                if (valid) {
                    indices[prim_count] = e;

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

        IndexBuffer out_indices = dr::load<UInt32>(indices.data(), indices.size());
        DynamicBuffer<Float> out_weights= dr::load<Float>(weight.data(), weight.size());

        return std::make_tuple(out_indices, out_weights);
    } else {
        UInt32 e = dr::arange<UInt32>(m_face_count * 3);
        Vector3u vi = dedge_indices(e);
        Point3f p0 = vertex_position(vi[0]),
                p1 = vertex_position(vi[1]);

        Normal3f n = face_normal(DirectedEdge::face(e));
        Vector3f to_p0 = dr::normalize(p0 - viewpoint);
        Vector3f to_p1 = dr::normalize(p1 - viewpoint);

        // The arclength weight is not perfect for perspective
        // cameras. But it is a close approximation.
        Float weight = unit_angle(to_p0, to_p1);

        UInt32 e_oppo = dedge_opposite(e);
        Mask has_opposite = (e_oppo != DirectedEdge::Invalid);

        Normal3f n_oppo = face_normal(DirectedEdge::face(e_oppo), has_opposite);

        Mask not_flat = dr::abs(dr::dot(n, n_oppo)) < 1.f;
        Mask only_one_visible_face =
            dr::dot(to_p0, n) * dr::dot(to_p0, n_oppo) <= 0.f;

        Mask valid = !has_opposite || ((e_oppo > e) &&
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

    UInt32 corner = DirectedEdge::corner(sample1);
    Vector3u vi = dedge_indices(sample1, active);
    Point3f p0 = vertex_position(vi[0], active),
            p1 = vertex_position(vi[1], active),
            p2 = vertex_position(vi[2], active);

    SilhouetteSample3f ss = dr::zeros<SilhouetteSample3f>();
    ss.p = dr::lerp(p0, p1, sample2);
    ss.d = dr::normalize(ss.p - viewpoint);
    ss.silhouette_d = dr::normalize(p1 - p0);
    ss.pdf = dr::rsqrt(dr::squared_norm(p0 - p1));
    ss.offset = 0.f;
    ss.prim_index = DirectedEdge::face(sample1);
    ss.shape = this;
    ss.discontinuity_type = (uint32_t) DiscontinuityFlags::PerimeterType;

    Vector3f inward_dir = p2 - ss.p;
    ss.n = dr::normalize(dr::cross(ss.d, ss.silhouette_d));
    dr::masked(ss.n, dr::dot(ss.n, inward_dir) > 0.f) *= -1.f;

    // Face local barycentric UV coordinates used by `differential_motion`
    ss.uv = dr::select(corner == 0u,
                       Point2f(sample2, 0.f),
                       Point2f(1 - sample2, sample2));
    ss.uv = dr::select(corner == 2u,
                       Point2f(0.f, 1 - sample2),
                       ss.uv);

    return ss;
}

// =============================================================

// =============================================================
// Ray tracing routines
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

    // Solve a least squares problem to determine
    // the UV coordinates within the current triangle
    Float b1  = dr::dot(du, rel), b2 = dr::dot(dv, rel),
          a11 = dr::dot(du, du), a12 = dr::dot(du, dv),
          a22 = dr::dot(dv, dv),
          inv_det = dr::rcp(dr::fmsub(a11, a22, a12 * a12));

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

    SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

    // Early exit when tracing isn't necessary
    if (!m_is_instance && recursion_depth > 0)
        return si;

    constexpr bool IsDiff = dr::is_diff_v<Float>;
    bool detach  = IsDiff && has_flag(ray_flags, RayFlags::DetachShape),
         shading = has_flag(ray_flags, RayFlags::Shading);

    // Computing a smoothly interpolated shading tangent has a cost.
    // Only do this when the BSDF actually needs it.
    bool need_tangents = shading && packs_tangent();

    PackedFace<> frec = packed_face(pi.prim_index, active);

    PackedVertex rec0 = packed_vertex(frec[0], active, detach),
                 rec1 = packed_vertex(frec[1], active, detach),
                 rec2 = packed_vertex(frec[2], active, detach);

    auto position = [](const PackedVertex &r) {
        return Point3f(r[PackedPositionOffset],
                       r[PackedPositionOffset + 1],
                       r[PackedPositionOffset + 2]);
    };

    Point3f p0 = position(rec0),
            p1 = position(rec1),
            p2 = position(rec2);

    Float b1 = pi.prim_uv.x(),
          b2 = pi.prim_uv.y(),
          b0 = 1.f - b1 - b2;

    // Edge vectors
    Vector3f e1 = p1 - p0, e2 = p2 - p0;

    // Surface position at the detached barycentric coordinates
    Point3f p_att = dr::fmadd(p0, b0, dr::fmadd(p1, b1, p2 * b2));

    Normal3f n_geo(face_normal(p0, p1, p2));

    si.t = pi.t;
    si.p = dr::detach(p_att);
    si.n = dr::detach(n_geo);

    si.attach_motion(ray, p_att, ray_flags);

    if constexpr (IsDiff) {
        // Propagate the derivative of the intersection point onto the barycentric coords.
        if (!has_flag(ray_flags, RayFlags::FollowShape) &&
            dr::grad_enabled(p0, p1, p2, ray.o, ray.d)) {
            Vector3f rel = si.p - p_att;

            Float a11 = dr::dot(e1, e1), a12 = dr::dot(e1, e2),
                  a22 = dr::dot(e2, e2),
                  inv_det = dr::rcp(dr::fmsub(a11, a22, a12 * a12)),
                  r1 = dr::dot(e1, rel), r2 = dr::dot(e2, rel);

            b1 = dr::replace_grad(b1, b1 + dr::fmsub (a22, r1, a12 * r2) * inv_det);
            b2 = dr::replace_grad(b2, b2 + dr::fnmadd(a12, r1, a11 * r2) * inv_det);
        }
    }

    si.n = n_geo;

    if (likely(shading)) {
        bool need_dn = has_normals() &&
                       has_flag(ray_flags, RayFlags::NormalPartials);

        // Shading normal, and its partials wrt. the barycentric coordinates
        Vector3f dn_db1 = 0.f, dn_db2 = 0.f;

        // Decoded in storage precision. With a packed tangent frame, a
        // single decode per vertex yields the normal and the tangent;
        // without one the record stores the normal outright and the
        // decodes disappear.
        auto frame = [tangent = packs_tangent()](const PackedVertex &r) {
            Vector<Float32, 3> f(r[PackedFrameOffset],
                                 r[PackedFrameOffset + 1],
                                 r[PackedFrameOffset + 2]);
            if (tangent)
                return frame_decode(f);
            else
                return std::pair(Normal<Float32, 3>(f),
                                 Vector<Float32, 3>(0.f));
        };

        auto [sn0, st0] = frame(rec0);
        auto [sn1, st1] = frame(rec1);
        auto [sn2, st2] = frame(rec2);

        if (has_normals()) {
            Vector3f n0(sn0),
                     dn1 = Vector3f(sn1) - n0,
                     dn2 = Vector3f(sn2) - n0;

            Vector3f n = dr::fmadd(dn1, b1, dr::fmadd(dn2, b2, n0));
            Float il = dr::rsqrt(dr::squared_norm(n));

            // Revert to the geometric normal if interpolation produces a
            // zero-valued shading normal
            Mask valid = dr::isfinite(il);
            n  = dr::select(valid, n * il, Vector3f(si.n));
            il = dr::select(valid, il, 0.f);

            si.sh_frame.n = Normal3f(n);

            if (need_dn) {
                // Derivative of ``normalize(b0*n0 + b1*n1 + b2*n2)``. Since
                // d/du [f(u)/|f(u)|] = [d/du f(u)]/|f(u)|
                // - f(u)/|f(u)|^3 <f(u), d/du f(u)>, this results in
                dn1 *= il;
                dn2 *= il;
                dn_db1 = dr::fnmadd(n, dr::dot(n, dn1), dn1);
                dn_db2 = dr::fnmadd(n, dr::dot(n, dn2), dn2);
            }
        } else {
            si.sh_frame.n = si.n;
        }

        // Texture coordinates and the associated partials
        if (has_texcoords()) {
            auto texcoord = [](const PackedVertex &r) {
                return Point2f(r[PackedTexcoordOffset],
                               r[PackedTexcoordOffset + 1]);
            };
            Point2f uv0 = texcoord(rec0);
            Vector2f duv0 = texcoord(rec1) - uv0,
                     duv1 = texcoord(rec2) - uv0;

            si.uv = Point2f(dr::fmadd(duv0, b1, dr::fmadd(duv1, b2, Vector2f(uv0))));

            Float det = dr::fmsub(duv0.x(), duv1.y(), duv0.y() * duv1.x());

            // Faces that occupy no UV area have no parameterization to invert.
            // Substituting the identity map keeps the barycentric one.
            Mask degenerate = det == 0.f;
            duv0 = dr::select(degenerate, Vector2f(1.f, 0.f), duv0);
            duv1 = dr::select(degenerate, Vector2f(0.f, 1.f), duv1);
            det  = dr::select(degenerate, 1.f, det);

            Float inv_det = dr::rcp(det);

            // Change of variables from the barycentric coordinates to the
            // texture parameterization
            auto to_uv_basis = [&](const Vector3f &d1, const Vector3f &d2) {
                return std::make_pair(
                    Vector3f(dr::fmsub( duv1.y(), d1, duv0.y() * d2) * inv_det),
                    Vector3f(dr::fnmadd(duv1.x(), d1, duv0.x() * d2) * inv_det));
            };

            std::tie(si.dp_du, si.dp_dv) = to_uv_basis(e1, e2);

            if (need_dn)
                std::tie(si.dn_du, si.dn_dv) = to_uv_basis(dn_db1, dn_db2);
        } else {
            // Without texture coordinates the parameterization is barycentric
            si.uv    = Point2f(b1, b2);
            si.dp_du = e1;
            si.dp_dv = e2;

            if (need_dn) {
                si.dn_du = dn_db1;
                si.dn_dv = dn_db2;
            }
        }

        if (need_tangents) {
            // Compute an interpolated shading tangent if used by the BSDF
            Vector3f t0(st0),
                     dt1 = Vector3f(st1) - t0,
                     dt2 = Vector3f(st2) - t0;

            si.sh_frame.s = dr::fmadd(dt1, b1, dr::fmadd(dt2, b2, t0));

            // Is the parameterization (dp_du, dp_dv) flipped? Instead of
            // computing this from dp_du/dp_dv, use a cached bit which often
            // allows Dr.Jit to optimize away unused position partials.
            si.frame_flipped = (frec[3] & FaceUVFlipped) != 0u;
        }
    }

    si.prim_index = pi.prim_index;
    si.shape    = this;
    si.instance = nullptr;

    return si;
}

// =============================================================

// =============================================================
// Mesh attributes
// =============================================================

MI_VARIANT bool
Mesh<Float, Spectrum>::holds_rgb2spec_coeffs(std::string_view name,
                                            size_t dim) {
    return is_spectral_v<Spectrum> && dim == 3 &&
           name.find("color") != std::string_view::npos;
}

MI_VARIANT void
Mesh<Float, Spectrum>::to_rgb2spec_coeffs(InputFloat *data, size_t rows) {
    DRJIT_MARK_USED(data);
    DRJIT_MARK_USED(rows);
    if constexpr (is_spectral_v<Spectrum>) {
        for (size_t i = 0; i < rows; ++i, data += 3)
            dr::store(data, srgb_model_fetch(
                                dr::load<Color<InputFloat, 3>>(data)));
    }
}

MI_VARIANT const typename Mesh<Float, Spectrum>::MeshAttribute *
Mesh<Float, Spectrum>::find_attribute(std::string_view name) const {
    auto it = m_mesh_attributes.find(name);
    return it == m_mesh_attributes.end() ? nullptr : &it->second;
}

MI_VARIANT template <uint32_t Size, bool Raw>
auto Mesh<Float, Spectrum>::interpolate_attribute(
        const MeshAttribute &attr, std::string_view name,
        const SurfaceInteraction3f &si, Mask active) const {
    using StoredType =
        std::conditional_t<Size == 1,
                           dr::replace_scalar_t<Float, InputFloat>,
                           dr::replace_scalar_t<Color3f, InputFloat>>;
    using ReturnType = std::conditional_t<Size == 1, Float, Color3f>;

    const FloatBuffer &buf = attr.data.array();

    StoredType v0, v1, v2;
    Point3f b(1.f, 0.f, 0.f);

    if (is_vertex_attribute(name)) {
        Vector3u fi = face_indices(si.prim_index, active);
        b  = barycentric_coordinates(si, active);
        v0 = dr::gather<StoredType>(buf, fi[0], active);
        v1 = dr::gather<StoredType>(buf, fi[1], active);
        v2 = dr::gather<StoredType>(buf, fi[2], active);
    } else {
        v0 = v1 = v2 = dr::gather<StoredType>(buf, si.prim_index, active);
    }

    auto blend = [&](const auto &a0, const auto &a1, const auto &a2) {
        return dr::fmadd(a0, b[0], dr::fmadd(a1, b[1], a2 * b[2]));
    };

    if constexpr (Size == 3 && !Raw && is_spectral_v<Spectrum>) {
        // The expansion is nonlinear and therefore precedes the blend
        if (holds_rgb2spec_coeffs(name, Size))
            return blend(
                srgb_model_eval<UnpolarizedSpectrum>(v0, si.wavelengths),
                srgb_model_eval<UnpolarizedSpectrum>(v1, si.wavelengths),
                srgb_model_eval<UnpolarizedSpectrum>(v2, si.wavelengths));
        else
            return UnpolarizedSpectrum(
                luminance((Color3f) blend(v0, v1, v2)));
    } else if constexpr (Size == 3 && !Raw &&
                         is_monochromatic_v<Spectrum>) {
        return UnpolarizedSpectrum(luminance((Color3f) blend(v0, v1, v2)));
    } else {
        return (ReturnType) blend(v0, v1, v2);
    }
}

MI_VARIANT template <uint32_t Size>
auto Mesh<Float, Spectrum>::eval_attribute_n(std::string_view name,
                                             const SurfaceInteraction3f &si,
                                             Mask active) const {
    static_assert(Size == 1 || Size == 3);
    using Result = std::conditional_t<Size == 1, Float, Color3f>;

    const MeshAttribute *attr = find_attribute(name);
    if (!attr) {
        if constexpr (Size == 1)
            return Base::eval_attribute_1(name, si, active);
        else
            return Base::eval_attribute_3(name, si, active);
    }

    if (attr->dim != Size)
        return Result(0.f);

    return interpolate_attribute<Size, true>(*attr, name, si, active);
}

MI_VARIANT const typename Mesh<Float, Spectrum>::TensorXf32 &
Mesh<Float, Spectrum>::attribute(std::string_view name) const {
    auto it = m_mesh_attributes.find(name);
    if (it == m_mesh_attributes.end())
        Throw("attribute(): attribute \"%s\" doesn't exist.", name);
    return it->second.data;
}

MI_VARIANT void Mesh<Float, Spectrum>::add_attribute(std::string_view name,
                                                     const TensorXf32 &values) {
    if (m_mesh_attributes.find(name) != m_mesh_attributes.end())
        Throw("add_attribute(): attribute \"%s\" already exists.", name);

    bool vertex = is_vertex_attribute(name);
    if (!vertex && !string::starts_with(name, "face_"))
        Throw("add_attribute(): attribute name must start with either "
              "\"vertex_\" or \"face_\".");

    size_t rows = vertex ? m_vertex_count : m_face_count;
    if (values.ndim() != 2 || values.shape(0) != rows)
        Throw("add_attribute(): attribute \"%s\": expected a (%zu, dim) "
              "tensor with one row per %s.", name, rows,
              vertex ? "vertex" : "face");

    size_t dim = values.shape(1);
    if (dim == 0 || dim > 4)
        Throw("add_attribute(): attribute \"%s\": 1 to 4 channels are "
              "supported, got %zu.", name, dim);

    TensorXf32 data = values;

    // Color records convert on the way in, using a host-side table lookup
    if (holds_rgb2spec_coeffs(name, dim)) {
        const FloatBuffer &host = dr::migrate(values.array(), JitBackend::None);
        if constexpr (dr::is_jit_v<Float>)
            dr::sync_thread();

        std::vector<InputFloat> buf(host.data(), host.data() + rows * 3);
        to_rgb2spec_coeffs(buf.data(), rows);

        data = TensorXf32(dr::load<FloatBuffer>(buf.data(), rows * 3),
                          { rows, dim });
    }

    m_mesh_attributes.insert(
        { std::string(name), { (uint32_t) dim, std::move(data) } });
}

MI_VARIANT void
Mesh<Float, Spectrum>::remove_attribute(std::string_view name) {
    auto it = m_mesh_attributes.find(name);
    if (it == m_mesh_attributes.end()) {
        // Maybe it exists as a texture attribute, try that.
        return Base::remove_attribute(name);
    }
    m_mesh_attributes.erase(it);
}

MI_VARIANT typename Mesh<Float, Spectrum>::Mask
Mesh<Float, Spectrum>::has_attribute(std::string_view name, Mask active) const {
    if (!find_attribute(name))
        return Base::has_attribute(name, active);
    return true;
}

MI_VARIANT typename Mesh<Float, Spectrum>::UnpolarizedSpectrum
Mesh<Float, Spectrum>::eval_attribute(std::string_view name,
                                      const SurfaceInteraction3f &si,
                                      Mask active) const {
    const MeshAttribute *attr = find_attribute(name);
    if (!attr)
        return Base::eval_attribute(name, si, active);

    if (attr->dim == 1)
        return interpolate_attribute<1, false>(*attr, name, si, active);
    else if (attr->dim == 3)
        return interpolate_attribute<3, false>(*attr, name, si, active);
    else
        return UnpolarizedSpectrum(0.f);
}

MI_VARIANT Float
Mesh<Float, Spectrum>::eval_attribute_1(std::string_view name,
                                        const SurfaceInteraction3f &si,
                                        Mask active) const {
    return eval_attribute_n<1>(name, si, active);
}

MI_VARIANT typename Mesh<Float, Spectrum>::Color3f
Mesh<Float, Spectrum>::eval_attribute_3(std::string_view name,
                                        const SurfaceInteraction3f &si,
                                        Mask active) const {
    return eval_attribute_n<3>(name, si, active);
}

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
            // Both this and the next vertex are inside, add to the list
            Assert(out_count + 1 < max_vertices);
            output[out_count++] = next;
        } else if (cur_is_inside && !next_is_inside) {
            // Going outside -- add the intersection
            double t = (split_pos - cur[axis]) / (next[axis] - cur[axis]);
            Assert(out_count + 1 < max_vertices);
            Point3d p = cur + (next - cur) * t;
            p[axis] = split_pos; // Avoid roundoff errors
            output[out_count++] = p;
        } else if (!cur_is_inside && next_is_inside) {
            // Coming back inside -- add the intersection + next vertex
            double t = (split_pos - cur[axis]) / (next[axis] - cur[axis]);
            Assert(out_count + 2 < max_vertices);
            Point3d p = cur + (next - cur) * t;
            p[axis] = split_pos; // Avoid roundoff errors
            output[out_count++] = p;
            output[out_count++] = next;
        } else {
            // Entirely outside - do not add anything
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

    // The kd-tree code will frequently call this function with
    // almost-collapsed bounding boxes. It's extremely important not to
    // introduce errors in such cases, otherwise the resulting tree will
    // incorrectly remove triangles from the associated nodes. Hence, do
    // the following computation in double precision!

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
    oss << class_name() << "[" << std::endl
        << "  filename = \"" << m_filename << "\"," << std::endl
        << "  bbox = " << string::indent(m_bbox) << "," << std::endl
        << "  position_count = " << position_count() << "," << std::endl
        << "  normal_count = " << normal_count() << "," << std::endl
        << "  vertex_count = " << m_vertex_count << "," << std::endl
        << "  vertices = [" << util::mem_string(vertex_data_bytes() * m_vertex_count) << " of vertex data]," << std::endl
        << "  face_count = " << m_face_count << "," << std::endl
        << "  faces = [" << util::mem_string(face_data_bytes() * m_face_count) << " of face data]," << std::endl;

    if (!m_area_pmf.empty())
        oss << "  surface_area = " << m_area_pmf.sum() << "," << std::endl;

    oss << "  face_normals = " << has_face_normals();

    if (!m_mesh_attributes.empty()) {
        oss << "," << std::endl << "  mesh attributes = [" << std::endl;
        size_t i = 0;
        for(const auto &[name, attribute]: m_mesh_attributes)
            oss << "    " << name << ": " << attribute.dim
                << (attribute.dim == 1 ? " float" : " floats")
                << (++i == m_mesh_attributes.size() ? "" : ",") << std::endl;
        oss << "  ]";
    }

    oss << "," << std::endl;
    oss << "  " << string::indent(get_children_string()) << std::endl;

    oss << "]";
    return oss.str();
}

MI_VARIANT size_t Mesh<Float, Spectrum>::vertex_data_bytes() const {
    size_t vertex_data_bytes = 3 * sizeof(InputFloat);

    if (has_normals())
        vertex_data_bytes += 3 * sizeof(InputFloat);
    if (has_texcoords())
        vertex_data_bytes += 2 * sizeof(InputFloat);

    for (const auto&[name, attribute]: m_mesh_attributes)
        if (is_vertex_attribute(name))
            vertex_data_bytes += attribute.dim * sizeof(InputFloat);

    return vertex_data_bytes;
}

MI_VARIANT size_t Mesh<Float, Spectrum>::face_data_bytes() const {
    size_t face_data_bytes = 4 * sizeof(ScalarIndex);

    for (const auto&[name, attribute]: m_mesh_attributes)
        if (!is_vertex_attribute(name))
            face_data_bytes += attribute.dim * sizeof(InputFloat);

    return face_data_bytes;
}

MI_VARIANT void
Mesh<Float, Spectrum>::describe(ShapeIR &g) const {
    g.kind = ShapeIR::Kind::Triangles;
    g.type = m_shape_type;
    g.ctx = this;
    g.vertex_count = m_vertex_count;
    g.face_count = m_face_count;
    g.vertex_ptr = m_packed_vertices.data();
    g.vertex_stride = MeshVertexStride * sizeof(InputFloat);
    g.index_ptr  = m_packed_faces.data();
    g.index_stride = 4 * sizeof(ScalarIndex);
    if constexpr (dr::is_metal_v<Float>) {
        // Metal has no index stride; hand it the (F, 3) faces view, whose
        // data() call materializes it
        g.index_ptr = faces().array().data();
        g.index_stride = 3 * sizeof(ScalarIndex);
    }
}

MI_VARIANT bool Mesh<Float, Spectrum>::parameters_grad_enabled() const {
    return dr::grad_enabled(m_packed_vertices);
}

MI_IMPLEMENT_TRAVERSE_CB(Mesh, Base)
MI_INSTANTIATE_CLASS(Mesh)
NAMESPACE_END(mitsuba)
