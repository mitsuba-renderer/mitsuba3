#include <mitsuba/core/logger.h>
#include <mitsuba/render/dedge.h>
#include <drjit/while_loop.h>
#include <algorithm>
#include <array>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT
DirectedEdge<Float, Spectrum>::DirectedEdge(const IndexBuffer &F,
                                            uint32_t vertex_count,
                                            std::string_view name,
                                            bool warn_defects)
    : m_vertex_count(vertex_count),
      m_half_edge_count((uint32_t) F.size()), m_name(name) {
    if (m_half_edge_count % 3 != 0)
        Throw("DirectedEdge(): 'F' holds %u entries, which is not a multiple "
              "of 3!", m_half_edge_count);

    if (m_half_edge_count == 0 || m_vertex_count == 0)
        Throw("DirectedEdge(): the mesh must have at least one face and one "
              "vertex!");

    if constexpr (dr::is_jit_v<Float>)
        build_jit(F);
    else
        build_host(F);

    if (!warn_defects)
        return;

    count_flags();

    if (m_flag_counts[1] || m_flag_counts[2] || m_flag_counts[3])
        Log(Warn,
            "DirectedEdge(%s): mesh has %u vertices on non-manifold edges, %u "
            "non-manifold vertices, and %u vertices on inconsistently wound "
            "edges. Edges with non-manifold incidence or inconsistent face "
            "winding are left unpaired and treated as boundaries.",
            m_name.empty() ? std::string() : "\"" + m_name + "\"",
            m_flag_counts[1], m_flag_counts[2], m_flag_counts[3]);
}

MI_VARIANT void DirectedEdge<Float, Spectrum>::count_flags() const {
    if (m_flag_counts_ready)
        return;

    if constexpr (dr::is_jit_v<Float>) {
        UInt32 counts = dr::zeros<UInt32>(4);
        for (uint32_t i = 0; i < 4; ++i)
            dr::scatter_reduce(ReduceOp::Add, counts, UInt32(1), UInt32(i),
                               has_flag(m_flags, (VertexFlags) (1u << i)));
        IndexBuffer host_counts = dr::migrate(counts, JitBackend::None);
        dr::sync_thread();
        for (uint32_t i = 0; i < 4; ++i)
            m_flag_counts[i] = host_counts.data()[i];
    } else {
        for (uint32_t i = 0; i < 4; ++i)
            m_flag_counts[i] = dr::count(has_flag(m_flags, (VertexFlags) (1u << i)))[0];
    }

    m_flag_counts_ready = true;
}

MI_VARIANT uint32_t
DirectedEdge<Float, Spectrum>::flag_count(VertexFlags flag) const {
    count_flags();
    return m_flag_counts[dr::log2i((uint32_t) flag)];
}

MI_VARIANT void DirectedEdge<Float, Spectrum>::build_host(const IndexBuffer &buffer) {
    if constexpr (dr::is_jit_v<Float>) {
        DRJIT_MARK_USED(buffer);
    } else {
        const uint32_t *F = buffer.data();
        uint32_t D = m_half_edge_count,
                 V = m_vertex_count;

        std::vector<uint32_t> E2E(D, Invalid),
                              V2E(V, Invalid),
                              V2E_any(V, Invalid),
                              valence(V, 0),
                              flags(V, 0);

        // Collect the half-edges of non-degenerate faces as {lower endpoint,
        // upper endpoint, half-edge} and sort, which groups those sharing an
        // undirected edge
        std::vector<std::array<uint32_t, 3>> edges;
        edges.reserve(D);
        for (uint32_t f = 0; f < D / 3; ++f) {
            const uint32_t *v = F + 3 * f;
            if (v[0] == v[1] || v[1] == v[2] || v[0] == v[2])
                continue; // A face with a repeated index has no area
            for (uint32_t i = 0; i < 3; ++i)
                edges.push_back({ std::min(v[i], v[(i + 1) % 3]),
                                  std::max(v[i], v[(i + 1) % 3]), 3 * f + i });
        }
        std::sort(edges.begin(), edges.end());

        // Pair each run of equal keys, but only where the answer is unambiguous
        for (size_t i = 0, n = edges.size(); i < n; ) {
            auto [a, b, e] = edges[i];

            size_t j = i + 1;
            while (j < n && edges[j][0] == a && edges[j][1] == b)
                ++j;

            if (j - i == 2) {
                uint32_t g = edges[i + 1][2];
                if (F[e] != F[g]) {
                    E2E[e] = g;
                    E2E[g] = e;
                } else {
                    flags[a] |= (uint32_t) VertexFlags::InconsistentOrientation;
                    flags[b] |= (uint32_t) VertexFlags::InconsistentOrientation;
                }
            } else if (j - i > 2) {
                flags[a] |= (uint32_t) VertexFlags::NonManifoldEdge;
                flags[b] |= (uint32_t) VertexFlags::NonManifoldEdge;
            }

            i = j;
        }

        // A half-edge starts a ring exactly when its predecessor is unpaired,
        // which turns the canonical start into a minimum over the whole star
        for (const auto &edge : edges) {
            uint32_t e = edge[2], a = F[e], b = F[next(e)];

            valence[a]++;
            V2E_any[a] = std::min(V2E_any[a], e);

            if (E2E[e] == Invalid) {
                flags[a] |= (uint32_t) VertexFlags::Boundary;
                flags[b] |= (uint32_t) VertexFlags::Boundary;
            }

            if (E2E[prev(e)] == Invalid)
                V2E[a] = std::min(V2E[a], e);
        }

        for (uint32_t v = 0; v < V; ++v)
            if (V2E[v] == Invalid)
                V2E[v] = V2E_any[v];

        // Walk each ring forward. It cannot cycle without returning to its
        // start (see build_jit()), so no iteration cap is needed.
        for (uint32_t v = 0; v < V; ++v) {
            uint32_t start = V2E[v];
            if (start == Invalid)
                continue;

            uint32_t e = start, count = 1;
            while (true) {
                uint32_t opp = E2E[e];
                if (opp == Invalid || next(opp) == start)
                    break;
                e = next(opp);
                count++;
            }

            if (count != valence[v])
                flags[v] |= (uint32_t) VertexFlags::NonManifoldVertex;
        }

        m_E2E     = dr::load<IndexBuffer>(E2E.data(), D);
        m_V2E     = dr::load<IndexBuffer>(V2E.data(), V);
        m_valence = dr::load<IndexBuffer>(valence.data(), V);
        m_flags   = dr::load<IndexBuffer>(flags.data(), V);
    }
}

MI_VARIANT void DirectedEdge<Float, Spectrum>::build_jit(const IndexBuffer &F) {
    if constexpr (!dr::is_jit_v<Float>) {
        DRJIT_MARK_USED(F);
    } else {
        uint32_t D = m_half_edge_count,
                 V = m_vertex_count;

        // Endpoints of every half-edge, plus the opposite corner of its face
        UInt32 e     = dr::arange<UInt32>(D),
               src   = dr::gather<UInt32>(F, e),
               dst   = dr::gather<UInt32>(F, next(e)),
               third = dr::gather<UInt32>(F, prev(e));

        // Identify non-degenerate faces and drop others
        Mask alive = (src != dst) && (dst != third) && (src != third);

        m_valence = dr::zeros<UInt32>(V);
        dr::scatter_reduce(ReduceOp::Add, m_valence, UInt32(1), src, alive);

        // Construction begins with a pairing step that finds all half-edges
        // sharing an undirected edge. For this, each half-edge inserts itself
        // into a linked list at its lower-valence endpoint (ties broken by
        // index). Both halves of an undirected edge pick the same one, so that
        // walking a single list reveals all half-edges of that undirected
        // edge. Preferring low valence keeps the lists short and avoids
        // quadratic runtime cost in meshes with high-valence vertices.
        UInt32 valence_src = dr::gather<UInt32>(m_valence, src),
               valence_dst = dr::gather<UInt32>(m_valence, dst);

        Mask src_first = (valence_src < valence_dst) ||
                         ((valence_src == valence_dst) && (src < dst));

        UInt32 owner = dr::select(src_first, src, dst),
               other = dr::select(src_first, dst, src);

        // The list at one vertex may contain half-edges from multiple
        // undirected edges. The list traversal done later tells them apart via
        // 'key', which stores the 'other' endpoint plus a bit recording
        // whether the half-edge points toward 'owner' or away from it.
        UInt32 key = (other << 1) | UInt32(src_first);

        // Build the lists. Following these lines, 'head[v]' holds the most
        // recently inserted half-edge of vertex 'v', and 'link[e]' points to
        // the successor of half-edge 'e'.
        UInt32 head = dr::full<UInt32>(Invalid, V),
               link = dr::scatter_exch(head, e, owner, alive);

        dr::eval(key, link, head);

        // Every half-edge now walks its owner's list and classifies itself by
        // counting the entries sharing its undirected edge and recording an
        // opposed partner when it sees one. Both halves of a pair walk the
        // same list and each writes only its own slot. As a consequence,
        // E2E[E2E[e]] == e holds by construction.
        struct ScanState {
            UInt32 it;      // Current position in the list, Invalid at the end
            UInt32 matches; // Entries seen on the same undirected edge
            UInt32 partner; // Opposed half-edge, or Invalid if none seen yet
            DRJIT_STRUCT(ScanState, it, matches, partner)
        };

        ScanState s{ dr::select(alive, dr::gather<UInt32>(head, owner, alive),
                                UInt32(Invalid)),
                     dr::zeros<UInt32>(D), dr::full<UInt32>(Invalid, D) };

        dr::tie(s) = dr::while_loop(
            dr::make_tuple(s),
            [](const ScanState &s) { return s.it != Invalid; },
            [&](ScanState &s) {
                Mask valid = s.it != Invalid;
                UInt32 k   = dr::gather<UInt32>(key, s.it, valid);

                // Two half-edges share an undirected edge when they agree on
                // the other endpoint. They are opposed when the top bit differs.
                Mask match   = (k >> 1) == (key >> 1),
                     opposed = k == (key ^ 1u);

                s.matches += UInt32(match);
                s.partner  = dr::select(opposed, s.it, s.partner);

                // Advance and stop early when there are more than 2 matches
                s.it = dr::select(s.matches > 2u, UInt32(Invalid),
                                  dr::gather<UInt32>(link, s.it, valid));
            });

        // A half-edge always matches itself, and does so in its own direction,
        // so a group of two holds at most one opposed half-edge
        Mask paired       = (s.matches == 2u) && (s.partner != Invalid),
             inconsistent = (s.matches == 2u) && (s.partner == Invalid),
             non_manifold = s.matches > 2u;

        m_E2E = dr::select(paired, s.partner, UInt32(Invalid));

        auto bit = [](VertexFlags f, const Mask &mask) {
            return dr::select(mask, UInt32((uint32_t) f), UInt32(0));
        };

        Mask unpaired = alive && !paired;

        // Both endpoints of a half-edge inherit its defects
        UInt32 defects = bit(VertexFlags::NonManifoldEdge, non_manifold) |
                         bit(VertexFlags::InconsistentOrientation, inconsistent) |
                         bit(VertexFlags::Boundary, unpaired);

        m_flags = dr::zeros<UInt32>(V);
        dr::scatter_reduce(ReduceOp::Or, m_flags, defects, src, unpaired);
        dr::scatter_reduce(ReduceOp::Or, m_flags, defects, dst, unpaired);

        // Now that E2E and the flags are ready, the next step computes V2E,
        // the half-edge at which the walk around each vertex begins. The walk
        // covers an open fan fully only when it begins at a half-edge with an
        // unpaired predecessor, and 'is_start' marks these candidates. 'Min'
        // reductions then find, per vertex, the smallest marked half-edge
        // (V2E_start) and the smallest one overall (V2E_any), the latter
        // serving as a fallback for closed fans. Vertices reached by neither
        // reduction keep the Invalid sentinel.
        Mask is_start = alive && (dr::gather<UInt32>(m_E2E, prev(e)) == Invalid);

        UInt32 V2E_start = dr::full<UInt32>(Invalid, V),
               V2E_any   = dr::full<UInt32>(Invalid, V);

        dr::scatter_reduce(ReduceOp::Min, V2E_start, e, src, is_start);
        dr::scatter_reduce(ReduceOp::Min, V2E_any, e, src, alive);

        m_V2E = dr::select(V2E_start != Invalid, V2E_start, V2E_any);

        // The last remaining output is the 'NonManifoldVertex' flag, which
        // marks vertices whose faces form more than one fan. The loop below
        // walks around each vertex and counts the number of faces, which
        // matches the valence in the manifold case. Each half-edge has a
        // unique successor next(E2E[e]) and a unique predecessor
        // E2E[prev(e)], so the half-edges of a mesh link up into disjoint
        // chains and rings. A walk of such a structure either ends at an
        // unpaired edge or returns to its starting point, so no iteration
        // cap is needed.
        struct WalkState {
            UInt32 e;     // Current half-edge of the walk
            UInt32 count; // Number of faces visited so far
            Mask active;  // Whether the walk is still in progress
            DRJIT_STRUCT(WalkState, e, count, active)
        };

        Mask visits = m_V2E != Invalid;
        WalkState w{ m_V2E, UInt32(visits), visits };

        dr::tie(w) = dr::while_loop(
            dr::make_tuple(w),
            [](const WalkState &w) { return w.active; },
            [this](WalkState &w) {
                UInt32 opp = dr::gather<UInt32>(m_E2E, w.e, w.active),
                       nxt = next(opp);

                Mask step = w.active && (opp != Invalid) && (nxt != m_V2E);

                w.count += UInt32(step);
                w.e      = dr::select(step, nxt, w.e);
                w.active = step;
            });

        m_flags |= bit(VertexFlags::NonManifoldVertex,
                       w.count != m_valence);

        dr::eval(m_E2E, m_V2E, m_valence, m_flags);
    }
}

MI_VARIANT std::string DirectedEdge<Float, Spectrum>::to_string() const {
    count_flags();
    return tfm::format(
        "DirectedEdge[%sfaces=%u, vertices=%u, boundary=%u, "
        "non_manifold_edge=%u, non_manifold_vertex=%u, "
        "inconsistent_orientation=%u]",
        m_name.empty() ? std::string() : tfm::format("name=\"%s\", ", m_name),
        m_half_edge_count / 3, m_vertex_count, m_flag_counts[0],
        m_flag_counts[1], m_flag_counts[2], m_flag_counts[3]);
}

MI_INSTANTIATE_CLASS(DirectedEdge)
NAMESPACE_END(mitsuba)
