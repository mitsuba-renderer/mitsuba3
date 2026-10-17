#include <mitsuba/render/emtree.h>
#include <numeric>

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(detail)

/// Bound on direction and length errors of a unit vector rounded to snorm8
constexpr float EmitterTreeAxisError = 0.00682f; // >= asin(sqrt(3) * 0.5 / 127)

/// Decode a half-angle sine byte q as cos(theta) = 1 - q*q*(2/255^2)
constexpr float EmitterTreeCosScale = 2.f / (255.f * 255.f);

/// Layout of the child word of a node (see EmitterTree::Node)
constexpr uint32_t EmitterTreeLeafFlag  = 0x80000000u, ///< The node is a leaf
                   EmitterTreeDeltaFlag = 0x40000000u, ///< The subtree holds a delta-position emitter
                   EmitterTreeIndexMask = 0x3fffffffu; ///< First child slot, or emitter index of a leaf

/// Children per inner node, and the number of binary levels that a group collapses
constexpr uint32_t EmitterTreeArity = 8, EmitterTreeBits = 3;

/**
 * \brief Bounds of a set of emitter primitives
 *
 * The builder describes every primitive and every cluster of primitives by
 * its bounding box, the cone of Conty Estevez and Kulla (2018) that contains
 * their orientations, the widest emission angle about those orientations,
 * and the summed energy. The traversal reads a quantized 32-byte record of
 * these bounds, which pack() produces and EmitterTree::importance() decodes.
 */
template <typename Value> struct EmitterTreeCluster {
    using Vector3 = Vector<Value, 3>;
    using Point3 = Point<Value, 3>;
    using BoundingBox3 = BoundingBox<Point3>;

    BoundingBox3 bbox;   ///< Union of the primitives' spatial bounds
    Vector3 axis;        ///< Unit axis of the cone enclosing the primitives' orientations
    Value theta_o,       ///< Half-angle of the orientation cone, in radians
          theta_e,       ///< Largest emission half-angle about a primitive's orientation
          energy;        ///< Sum of primitive energies, including sampling weights
    bool delta = false;  ///< Contains at least one delta-position emitter

    /// Union of two clusters, with the cone union of Conty Estevez and Kulla
    static EmitterTreeCluster merge(const EmitterTreeCluster &a_,
                                  const EmitterTreeCluster &b_) {
        const EmitterTreeCluster *a = &a_, *b = &b_;
        if (b->theta_o > a->theta_o)
            std::swap(a, b);

        EmitterTreeCluster r;
        r.bbox = a->bbox;
        r.bbox.expand(b->bbox);
        r.energy = a->energy + b->energy;
        r.theta_e = dr::maximum(a->theta_e, b->theta_e);
        r.delta = a->delta || b->delta;

        Value cos_d = dr::clip(dr::dot(a->axis, b->axis), -1.f, 1.f),
              theta_d = dr::acos(cos_d);

        // The wider cone already contains the other one
        if (theta_d + b->theta_o <= a->theta_o) {
            r.axis = a->axis;
            r.theta_o = a->theta_o;
            return r;
        }

        Value theta_o = 0.5f * (a->theta_o + theta_d + b->theta_o);
        if (theta_o >= dr::Pi<Value>) {
            r.axis = a->axis;
            r.theta_o = dr::Pi<Value>;
            return r;
        }

        // Rotate the wider cone's axis toward the other one
        Vector3 perp = b->axis - cos_d * a->axis;
        Value len = dr::norm(perp);
        if (len > 0.f)
            perp /= len;
        else
            perp = coordinate_system(a->axis).first;
        Value theta_r = theta_o - a->theta_o;
        r.axis = dr::normalize(a->axis * dr::cos(theta_r) + perp * dr::sin(theta_r));
        r.theta_o = theta_o;
        return r;
    }

    /**
     * \brief Cluster measure of the surface area orientation heuristic
     *
     * This is the energy times the spatial and angular measures of Eq. 2 of
     * Conty Estevez and Kulla (2018); the angular measure M_Omega is their
     * Eq. 1. The spatial measure uses the area of the bounding sphere
     * rather than the box area M_A, so that collinear or coplanar point
     * emitters still have a nonzero measure. Eq. 2 further divides by the
     * measures of the parent, which does not affect the choice of a split.
     */
    Value cost() const {
        Value theta_w = dr::minimum(theta_o + theta_e, dr::Pi<Value>),
              sin_o = dr::sin(theta_o), cos_o = dr::cos(theta_o);
        Value m_omega =
            2.f * dr::Pi<Value> * (1.f - cos_o) +
            0.5f * dr::Pi<Value> *
                (2.f * theta_w * sin_o - dr::cos(theta_o - 2.f * theta_w) -
                 2.f * theta_o * sin_o + cos_o);
        Value r2 = 0.25f * dr::squared_norm(bbox.extents());
        return 4.f * dr::Pi<Value> * r2 * m_omega * energy;
    }

    static uint32_t float_bits(Value f) {
        return dr::memcpy_cast<uint32_t>((float) f);
    }

    /**
     * \brief Write the cluster as a 32-byte node record
     *
     * - Words 0-2: bounding-box center, three float32 coordinates.
     * - Word 3: bounding-sphere radius, float32, at least Epsilon.
     * - Word 4: energy, float32.
     * - Word 5, bits 0-23: orientation axis xyz, signed bytes scaled by 1/127.
     * - Word 5, bits 24-31: sin(theta_e/2), unsigned byte scaled by 1/255.
     * - Word 6, bits 0-15: 1 - cos(theta_o), float16.
     * - Word 6, bits 16-31: sin(theta_o), float16.
     * - Word 7: the child word (see EmitterTree::Node) plus the delta flag.
     *
     * The angles round outwards, which widens the cones. The axis rounds to
     * nearest and theta_o grows by the resulting direction error.
     */
    void pack(uint32_t *n, uint32_t word) const {
        auto unorm8 = [](Value f) {
            return (uint32_t) dr::ceil(dr::clip(f, 0.f, 1.f) * 255.f);
        };
        auto snorm8 = [](Value f) {
            return (uint32_t) (uint8_t) (int8_t) dr::round(dr::clip(f, -1.f, 1.f) * 127.f);
        };
        auto fp16_up = [](Value f) {
            dr::half h((float) f);
            if ((float) h < f)
                h = dr::half::from_binary(h.value + 1);
            return (uint32_t) h.value;
        };

        if ((word & EmitterTreeDeltaFlag) != 0)
            Throw("Emitter tree: too many primitives or nodes for the 30-bit index");

        Point3 center = bbox.center();
        // A positive radius keeps the clamped distance of importance() finite
        Value radius = dr::maximum(0.5f * dr::norm(bbox.extents()),
                                   dr::Epsilon<Value>);

        // Keep the emission cone within a hemisphere by moving any excess
        // angle into the orientation cone
        Value theta_e_ = dr::minimum(theta_e, 0.5f * dr::Pi<Value>),
              theta_o_ = dr::minimum(theta_o + (theta_e - theta_e_) +
                                     EmitterTreeAxisError, dr::Pi<Value>);
        // The versine keeps precision near zero. Derive the sine from the
        // decoded cosine so that the stored pair stays consistent.
        uint32_t vers_o = fp16_up(2.f * dr::square(dr::sin(0.5f * theta_o_)));
        Value cos_o = 1.f - (float) dr::half::from_binary((uint16_t) vers_o);
        uint32_t sin_o = fp16_up(dr::safe_sqrt(1.f - dr::square(cos_o)));

        n[0] = float_bits(center.x());
        n[1] = float_bits(center.y());
        n[2] = float_bits(center.z());
        n[3] = float_bits(radius);
        n[4] = float_bits(energy);
        n[5] =  snorm8(axis.x()) |
               (snorm8(axis.y()) << 8) |
               (snorm8(axis.z()) << 16) |
               (unorm8(dr::sin(0.5f * theta_e_)) << 24);
        n[6] = vers_o | (sin_o << 16);
        n[7] = word | (delta ? EmitterTreeDeltaFlag : 0u);
    }
};

/**
 * Build a binary hierarchy with the surface area orientation heuristic, then
 * collapse it into groups of EmitterTreeArity children for traversal. Each binary
 * split sorts the primitives along every axis and evaluates every split position.
 */
template <typename Value> struct EmitterTreeBuilder {
    using Cluster = EmitterTreeCluster<Value>;
    using Vector3 = typename Cluster::Vector3;
    using BoundingBox3 = typename Cluster::BoundingBox3;

    std::vector<Cluster> prims;   ///< Bounds of each primitive, in original order
    std::vector<uint32_t> order;  ///< Permutation of the primitives, partitioned by the splits
    std::vector<Cluster> suffix;  ///< Scratch bounds of the primitives after a split position
    std::vector<uint32_t> nodes;  ///< Output node records, eight words per slot
    std::vector<uint32_t> codes;  ///< Output root-to-leaf path code of each primitive

    /**
     * Deepest binary level: it collapses into 10 groups, whose path code and
     * marker bit fit into 32 bits (see Emitter::emitter_tree_code())
     */
    static constexpr uint32_t MaxDepth = 30;

    /// Node of the binary tree that the splits produce
    struct BinaryNode {
        Cluster cluster;     ///< Bounds and energy of the entire subtree
        uint32_t left = 0,   ///< Left child in 'binary'; zero identifies a leaf
                 right = 0;  ///< Right child in 'binary'; zero for a leaf
        uint32_t prim = 0;   ///< Original primitive index, only meaningful for leaves
    };
    std::vector<BinaryNode> binary;  ///< Temporary hierarchy, with its root at index zero

    /// Sort order[begin, end) by box center along an axis, breaking ties by primitive index
    void sort(uint32_t begin, uint32_t end, int axis) {
        std::sort(order.begin() + begin, order.begin() + end,
                  [&](uint32_t a, uint32_t b) {
                      Value ca = prims[a].bbox.center()[axis],
                            cb = prims[b].bbox.center()[axis];
                      return ca < cb || (ca == cb && a < b);
                  });
    }

    /// Sort the range and return the position of the best split
    uint32_t split(uint32_t begin, uint32_t end, uint32_t depth) {
        uint32_t n = end - begin;

        BoundingBox3 cb;
        for (uint32_t i = begin; i < end; ++i)
            cb.expand(prims[order[i]].bbox.center());
        Vector3 extents = cb.extents();
        Value max_extent = dr::max(extents);
        int longest = extents.x() >= extents.y()
                          ? (extents.x() >= extents.z() ? 0 : 2)
                          : (extents.y() >= extents.z() ? 1 : 2);

        // Median splits once the heuristic could exceed the depth bound.
        // The subtree of a median-split range of n primitives has depth ceil(log2 n).
        uint32_t log2_n = 32 - dr::lzcnt(n - 1);
        int best_axis = -1;
        uint32_t best_pos = 0;
        Value best_cost = dr::Infinity<Value>;

        if (depth + log2_n < MaxDepth && max_extent > 0.f) {
            for (int axis = 0; axis < 3; ++axis) {
                if (extents[axis] <= 0.f)
                    continue;
                sort(begin, end, axis);

                // suffix[i] bounds the primitives at positions [i, n)
                Cluster acc = prims[order[end - 1]];
                suffix[n - 1] = acc;
                for (uint32_t i = n - 2; i >= 1; --i) {
                    acc = Cluster::merge(acc, prims[order[begin + i]]);
                    suffix[i] = acc;
                }

                // Pair each prefix with its suffix. The factor K_r of Eq. 2
                // penalizes splits along short axes of the centroid box.
                Value reg = max_extent / extents[axis];
                acc = prims[order[begin]];
                for (uint32_t i = 1; i < n; ++i) {
                    Value cost = reg * (acc.cost() + suffix[i].cost());
                    if (cost < best_cost) {
                        best_cost = cost;
                        best_axis = axis;
                        best_pos = i;
                    }
                    acc = Cluster::merge(acc, prims[order[begin + i]]);
                }
            }
        }

        if (best_axis < 0) {
            best_axis = longest;
            best_pos = n / 2;
        }
        sort(begin, end, best_axis);
        return begin + best_pos;
    }

    /// Build the binary subtree of the primitives at positions [begin, end)
    uint32_t build_binary(uint32_t begin, uint32_t end, uint32_t depth) {
        uint32_t index = (uint32_t) binary.size();
        binary.emplace_back();
        if (end - begin == 1) {
            uint32_t prim = order[begin];
            binary[index].cluster = prims[prim];
            binary[index].prim = prim;
            return index;
        }
        uint32_t mid = split(begin, end, depth);
        uint32_t left = build_binary(begin, mid, depth + 1),
                 right = build_binary(mid, end, depth + 1);
        binary[index].left = left;
        binary[index].right = right;
        binary[index].cluster = Cluster::merge(binary[left].cluster,
                                               binary[right].cluster);
        return index;
    }

    /// Collect the cut of the binary tree ``depth`` levels below ``index``
    void collect(uint32_t index, uint32_t depth, std::vector<uint32_t> &out) {
        const BinaryNode &b = binary[index];
        if (b.left == 0 || depth == 0) {
            out.push_back(index);
        } else {
            collect(b.left, depth - 1, out);
            collect(b.right, depth - 1, out);
        }
    }

    /**
     * Emit the group of children of binary node ``index``, whose group is
     * ``depth`` levels below the root, and return its first slot. ``code``
     * holds the choices of the ancestors.
     */
    uint32_t emit(uint32_t index, uint32_t depth, uint32_t code) {
        std::vector<uint32_t> children;
        collect(index, EmitterTreeBits, children);

        uint32_t base = (uint32_t) (nodes.size() / 8);
        nodes.resize(nodes.size() + 8 * (size_t) EmitterTreeArity);
        for (uint32_t i = 0; i < EmitterTreeArity; ++i) {
            if (i >= children.size()) {
                // Empty slot: zero energy and a radius that keeps importance() finite
                nodes[8 * (size_t) (base + i) + 3] = Cluster::float_bits(dr::Epsilon<Value>);
                continue;
            }
            const BinaryNode &b = binary[children[i]];
            // Root choices occupy the low bits, so PDF replay consumes them first
            uint32_t child_code = code | (i << (depth * EmitterTreeBits)), word;
            if (b.left == 0) {
                // A marker above the last choice distinguishes a valid path from zero
                codes[b.prim] = child_code | (1u << ((depth + 1) * EmitterTreeBits));
                word = EmitterTreeLeafFlag | b.prim;
            } else {
                word = emit(children[i], depth + 1, child_code);
            }
            b.cluster.pack(nodes.data() + 8 * (size_t) (base + i), word);
        }
        return base;
    }
};

NAMESPACE_END(detail)

MI_VARIANT EmitterTree<Float, Spectrum>::EmitterTree(
    std::vector<ref<Emitter>> &emitters,
    const std::vector<ref<Emitter>> &portals) {
    using Cluster = detail::EmitterTreeCluster<ScalarFloat>;
    constexpr uint32_t Arity = detail::EmitterTreeArity;

    detail::EmitterTreeBuilder<ScalarFloat> builder;
    std::vector<ScalarFloat> records;

    // Emitters of finite extent, in leaf order
    std::vector<Emitter *> finite;

    for (Emitter *e : emitters) {
        ScalarFloat weight = e->sampling_weight();
        if (weight == 0.f)
            continue;

        if (has_flag(e->flags(), EmitterFlags::Infinite)) {
            auto [dir, irradiance] = e->irradiance_estimate();
            records.insert(records.end(), { dir.x(), dir.y(), dir.z(),
                                            irradiance * weight });
            m_infinite_emitters.push_back(e);
        } else {
            EmitterBounds eb = e->bounds();
            builder.prims.push_back({ eb.bbox, eb.axis, eb.theta_o, eb.theta_e,
                                      eb.energy * weight,
                                      has_flag(e->flags(), EmitterFlags::DeltaPosition) });
            finite.push_back(e);
        }
    }

    size_t n = finite.size();
    if (n < 2)
        Throw("Emitter tree: requires at least two finite emitters");

    builder.order.resize(n);
    std::iota(builder.order.begin(), builder.order.end(), 0);
    builder.codes.assign(n, 0);
    builder.suffix.resize(n);

    // The root takes slot 0. The rest of its group stays unused and
    // keeps the child groups aligned to their size.
    builder.nodes.resize(8 * (size_t) Arity, 0);
    uint32_t root = builder.build_binary(0, (uint32_t) n, 0);
    builder.binary[root].cluster.pack(builder.nodes.data(),
                                      builder.emit(root, 0, 0));

    for (size_t i = 0; i < n; ++i)
        finite[i]->m_emitter_tree_code = builder.codes[i];

    m_nodes = dr::load<UInt32Buffer>(builder.nodes.data(), builder.nodes.size());
    m_emitters = dr::load<DynamicBuffer<EmitterPtr>>(finite.data(), n);

    if (!m_infinite_emitters.empty())
        m_infinite_records = dr::load<DynamicBuffer<Float>>(records.data(),
                                                        records.size());

    // Portals act as one-sided area emitters of unit radiance
    if (!portals.empty() && !m_infinite_emitters.empty()) {
        std::vector<uint32_t> nodes(8 * portals.size());
        for (size_t i = 0; i < portals.size(); ++i) {
            const ScalarAffineTransform4f &t = portals[i]->world_transform_scalar();
            ScalarPoint3f c = t * ScalarPoint3f(0.f);
            ScalarVector3f a = t * ScalarVector3f(1.f, 0.f, 0.f),
                           b = t * ScalarVector3f(0.f, 1.f, 0.f);
            ScalarBoundingBox3f bbox;
            for (ScalarVector3f corner : { a + b, a - b, b - a, -a - b })
                bbox.expand(ScalarPoint3f(c + corner));
            Cluster cl { bbox, dr::normalize(t * ScalarNormal3f(0.f, 0.f, 1.f)),
                         0.f, 0.5f * dr::Pi<ScalarFloat>,
                         4.f * dr::norm(dr::cross(a, b)) };
            cl.pack(nodes.data() + 8 * i, 0);
        }
        m_portal_nodes = dr::load<UInt32Buffer>(nodes.data(), nodes.size());
    }

    Log(Debug, "Built an emitter tree over %zu emitters (%zu infinite emitters)",
        n, m_infinite_emitters.size());
}

MI_VARIANT Float EmitterTree<Float, Spectrum>::importance(
    const Node &node, const Point3f &p, const Normal3f &n,
    const Mask &two_sided) const {
    // Decode the record written by EmitterTreeCluster::pack(). The axis keeps
    // its byte scale, which the dot product correction below folds in.
    auto as_float = [](const UInt32 &w) {
        return Float(dr::reinterpret_array<Float32>(w));
    };
    auto fp16 = [](const UInt32 &w) {
        return Float(dr::reinterpret_array<Float16>(UInt16(w)));
    };
    auto int8 = [](const UInt32 &w) { return Float(Int8(w)); };

    Point3f center(as_float(node[0]), as_float(node[1]), as_float(node[2]));
    Float radius = as_float(node[3]), energy = as_float(node[4]),
          cos_o = 1.f - fp16(node[6]), sin_o = fp16(node[6] >> 16),
          cos_e = dr::fnmadd(dr::square(Float(node[5] >> 24)),
                             detail::EmitterTreeCosScale, 1.f);
    Vector3f axis(int8(node[5]), int8(node[5] >> 8), int8(node[5] >> 16));
    Mask delta = (node[7] & detail::EmitterTreeDeltaFlag) != 0u;

    // Eq. 3 of Conty Estevez and Kulla (2018) for a bounding sphere. The
    // distance is clamped to the radius, and its reciprocal also normalizes
    // the two dot products below.
    Vector3f v = center - p;
    Float d2 = dr::squared_norm(v),
          r2 = dr::square(radius);
    Mask inside = d2 <= r2;
    Float inv_d = dr::rsqrt(dr::fmax(d2, r2));

    // Apparent angular radius theta_u of the bounding sphere, capped at
    // pi/2 outside. Inside, theta_u = pi stands for every direction: the
    // spread theta_s = theta_o + pi reaches at least pi, so sin_s <= 0 or
    // cos_t >= cos_s = -1 sets cos_tp = 1 below, and cos_i >= cos_u = -1
    // sets cos_ip = 1.
    Float sin_u = dr::select(inside, 0.f, radius * inv_d),
          cos_u = dr::select(inside, -1.f,
                             dr::sqrt(dr::fmax(dr::fnmadd(sin_u, sin_u, 1.f), 0.f)));

    // cos_t bounds the cosine between the axis and the direction to the
    // receiver. One factor folds in the byte scale of the axis and its
    // 0.7% length error. theta_s = theta_o + theta_u is the total spread.
    constexpr float eps = detail::EmitterTreeAxisError;
    Float dot = dr::dot(axis, v),
          cos_t = dot * dr::select(dot < 0.f, Float((-1.f / 127.f) / (1.f - eps)),
                                  Float((-1.f / 127.f) / (1.f + eps))) * inv_d;
    Float sin_t = dr::sqrt(dr::fmax(dr::fnmadd(cos_t, cos_t, 1.f), 0.f)),
          cos_s = dr::fmsub(cos_o, cos_u, sin_o * sin_u),
          sin_s = dr::fmadd(sin_o, cos_u, cos_o * sin_u);

    // Emission cosine at the angle theta_t - theta_s, clamped at zero. A
    // spread of pi or more (sin_s <= 0) covers all directions. The fmax
    // keeps the importance non-negative, which the prefix walk relies on.
    Float cos_tp = dr::select(cos_t >= cos_s || sin_s <= 0.f, 1.f,
                              dr::fmadd(cos_t, cos_s, sin_t * sin_s));
    Mask emits = cos_tp >= cos_e;
    cos_tp = dr::fmax(cos_tp, 0.f);

    // Bound the receiver cosine over the same angular radius. A zero normal
    // denotes an interaction without a surface, where this factor is omitted.
    Mask has_n = dr::any(n != 0.f);
    Float cos_i = dr::dot(n, v) * inv_d;
    cos_i = dr::select(two_sided, dr::abs(cos_i), cos_i);
    Float sin_i = dr::sqrt(dr::fmax(dr::fnmadd(cos_i, cos_i, 1.f), 0.f)),
          cos_ip = dr::select(cos_i >= cos_u, 1.f,
                              dr::fmadd(cos_i, cos_u, sin_i * sin_u));
    cos_ip = dr::select(has_n, dr::fmax(cos_ip, 0.f), 1.f);

    // BSDF sampling cannot reach delta emitters. Keep only the sidedness
    // test for these, rather than a diffuse receiver cosine.
    cos_ip = dr::select(delta, dr::select(cos_ip > 0.f, 1.f, 0.f), cos_ip);

    return dr::select(emits, energy * cos_tp * cos_ip * dr::square(inv_d), 0.f);
}

MI_VARIANT Float EmitterTree<Float, Spectrum>::infinite_importance(
    size_t index, const Point3f &p, const Normal3f &n,
    const Mask &two_sided) const {
    Vector4f rec = dr::gather<Vector4f>(m_infinite_records, UInt32((uint32_t) index));

    if (uses_portals(index)) {
        Float result = 0.f;
        for (size_t i = 0; i < m_portal_nodes.size() / 8; ++i)
            result += importance(
                dr::gather<Node>(m_portal_nodes, UInt32((uint32_t) i)),
                p, n, two_sided);
        return result * rec.w() * dr::InvPi<Float>;
    }

    // Directional emitters are delta emitters: only their sidedness counts
    // (see importance())
    Vector3f dir(rec.x(), rec.y(), rec.z());
    Mask directional = dr::any(dir != 0.f), has_n = dr::any(n != 0.f);
    Float cos_i = dr::dot(n, dir);
    cos_i = dr::select(two_sided, dr::abs(cos_i), cos_i);
    Float f = dr::select(directional && has_n && cos_i <= 0.f, 0.f, 1.f);
    return rec.w() * f;
}

MI_VARIANT typename EmitterTree<Float, Spectrum>::Weights
EmitterTree<Float, Spectrum>::weights(const Point3f &p, const Normal3f &n,
                                    const Mask &two_sided) const {
    Weights w;
    size_t n_infinite = m_infinite_emitters.size();

    if (n_infinite == 0) {
        // The descent alone determines the selection probabilities
        w.tree = w.total = 1.f;
        return w;
    }

    // The constant indices of the root and portal gathers always exist.
    // Masking them would produce zero nodes, whose zero radius breaks
    // the distance clamp of importance()
    w.tree = importance(dr::gather<Node>(m_nodes, UInt32(0)), p, n, two_sided);
    w.total = w.tree;

    // Without portals, nothing tells how much of an infinite emitter the
    // receiver can see. Such emitters together never outweigh the tree, so
    // that an enclosed scene still samples its own emitters. A receiver that
    // faces away from every finite emitter leaves the tree without weight
    // and then draws only from the infinite emitters.
    w.infinite.resize(n_infinite);
    Float unbounded = 0.f;
    for (size_t k = 0; k < n_infinite; ++k) {
        w.infinite[k] = infinite_importance(k, p, n, two_sided);
        if (!uses_portals(k))
            unbounded += w.infinite[k];
    }
    Float scale = dr::select(unbounded > w.tree && w.tree > 0.f,
                             w.tree / unbounded, 1.f);
    for (size_t k = 0; k < n_infinite; ++k) {
        if (!uses_portals(k))
            w.infinite[k] *= scale;
        w.total += w.infinite[k];
    }
    return w;
}

MI_VARIANT typename EmitterTree<Float, Spectrum>::Selection
EmitterTree<Float, Spectrum>::select(const Interaction3f &ref, Float sample,
                                   const Mask &two_sided, Mask active) const {
    // Selection probabilities are detached; differentiate the sampled emitter's
    // contribution rather than the discrete choice of an emitter.
    Point3f p = dr::detach(ref.p);
    Normal3f n = dr::detach(ref.n);
    size_t n_infinite = m_infinite_emitters.size();

    Weights w = weights(p, n, two_sided);

    // Stratified samplers can round a sample up to one. Below one, a sample
    // scaled by a total stays below that total (for u <= 1 - 2^-24,
    // fl(u * s) <= prev_float(s)), so the strict comparisons below and in
    // the descent always leave a candidate.
    sample = dr::fmin(sample, dr::OneMinusEpsilon<Float>);

    // Choose between the tree and the infinite emitters
    Selection sel;
    Float u = sample * w.total, acc = w.tree;
    Mask chosen_tree = u < w.tree, chosen = chosen_tree;
    sel.weight = dr::select(chosen_tree, w.total / w.tree, 0.f);
    sel.sample = dr::select(chosen_tree, u / w.tree, sample);

    for (size_t k = 0; k < n_infinite; ++k) {
        Mask pick = !chosen && w.infinite[k] > 0.f &&
                    (k + 1 == n_infinite || u < acc + w.infinite[k]);
        sel.emitter = dr::select(pick, EmitterPtr(m_infinite_emitters[k].get()), sel.emitter);
        sel.weight = dr::select(pick, w.total / w.infinite[k], sel.weight);
        sel.sample = dr::select(pick, (u - acc) / w.infinite[k], sel.sample);
        chosen |= pick;
        acc += w.infinite[k];
    }

    // The leaf names the emitter. Lanes outside the tree keep their weight
    // and sample. A descent that ran out of candidates left a zero weight,
    // and the masked gather then yields a null emitter.
    Mask in_tree = chosen_tree && active;
    UInt32 leaf = descend<false>(p, n, two_sided, sel.weight, sel.sample,
                                 in_tree) &
                  detail::EmitterTreeIndexMask;
    in_tree &= sel.weight > 0.f;
    sel.emitter = dr::select(in_tree,
                             dr::gather<EmitterPtr>(m_emitters, leaf, in_tree),
                             sel.emitter);
    return sel;
}

MI_VARIANT Float EmitterTree<Float, Spectrum>::pdf(
    const Interaction3f &ref, const EmitterPtr &emitter,
    const Mask &two_sided, Mask active) const {
    if constexpr (!dr::is_jit_v<Float>) {
        if (!active || emitter == nullptr)
            return 0.f;
    }

    Point3f p = dr::detach(ref.p);
    Normal3f n = dr::detach(ref.n);
    size_t n_infinite = m_infinite_emitters.size();

    Weights w = weights(p, n, two_sided);
    Float inv_total = dr::select(w.total > 0.f, dr::rcp(w.total), 0.f);

    Float pdf = 0.f;
    for (size_t k = 0; k < n_infinite; ++k)
        pdf = dr::select(emitter == EmitterPtr(m_infinite_emitters[k].get()),
                         w.infinite[k] * inv_total, pdf);

    UInt32 code = emitter->emitter_tree_code();
    Mask in_tree = active && code != 0u;

    Float tree_pdf = w.tree * inv_total;
    descend<true>(p, n, two_sided, tree_pdf, code, in_tree);
    return dr::select(in_tree, tree_pdf, pdf);
}

MI_VARIANT template <bool Replay>
typename EmitterTree<Float, Spectrum>::UInt32 EmitterTree<Float, Spectrum>::descend(
    const Point3f &p, const Normal3f &n, const Mask &two_sided, Float &value,
    std::conditional_t<Replay, UInt32, Float> &aux, const Mask &active) const {
    constexpr uint32_t Arity = detail::EmitterTreeArity;

    struct LoopState {
        UInt32 word;  ///< Packed child/leaf word of the current node
        Float value;  ///< Inverse probability (sampling) or probability (replay) of the path
        std::conditional_t<Replay, UInt32, Float> aux;  ///< Remapped sample or remaining path choices
        Mask active;  ///< Lanes still participating in the descent
        DRJIT_STRUCT(LoopState, word, value, aux, active)
    // The root's children form the first group after its reserved slot
    } ls = { UInt32(Arity), value, aux, active };

    dr::tie(ls) = dr::while_loop(dr::make_tuple(ls),
        [](const LoopState &ls) { return ls.active; },
        [this, &p, &n, &two_sided](LoopState &ls) {
            // One masked 32-byte gather and evaluation per child. The mask
            // keeps terminated lanes from following stale child indices.
            UInt32 base = ls.word & detail::EmitterTreeIndexMask;
            Float importance[Arity], prefix[Arity], sum = 0.f;
            UInt32 words[Arity];
            for (uint32_t c = 0; c < Arity; ++c) {
                Node node = dr::gather<Node>(m_nodes, base + c, ls.active);
                importance[c] = this->importance(node, p, n, two_sided);
                words[c] = node[7];
                sum += importance[c];
                prefix[c] = sum;
            }

            UInt32 word;
            Float prob;
            if constexpr (Replay) {
                // Take the recorded child at each level
                UInt32 choice = ls.aux & (Arity - 1u);
                ls.aux = ls.aux >> detail::EmitterTreeBits;
                prob = importance[0];
                word = words[0];
                for (uint32_t c = 1; c < Arity; ++c) {
                    Mask pick = choice == c;
                    prob = dr::select(pick, importance[c], prob);
                    word = dr::select(pick, words[c], word);
                }
                ls.value *= dr::select(sum > 0.f, prob / sum, 0.f);
            } else {
                // The scaled sample picks the first child whose prefix
                // exceeds it, which has a nonzero importance. Walking
                // backwards, the lowest such child wins. The last slot
                // is the fallback, reached only when every importance
                // is zero; its zero probability then ends the lane
                // with a zero weight.
                Float x = ls.aux * sum;
                word = words[Arity - 1];
                prob = importance[Arity - 1];
                Float lo = prefix[Arity - 2];
                for (uint32_t c = Arity - 1; c-- > 0;) {
                    Mask pick = x < prefix[c];
                    word = dr::select(pick, words[c], word);
                    prob = dr::select(pick, importance[c], prob);
                    lo = dr::select(pick, c == 0 ? Float(0.f) : prefix[c - 1], lo);
                }

                // Normalize each level to avoid over/underflow of products
                // of raw importances. Tracking the inverse PMF shares the
                // reciprocal with the remapping of the sample.
                Float inv_prob = dr::select(prob > 0.f, dr::rcp(prob), 0.f);
                ls.aux = dr::fmin(dr::fmax((x - lo) * inv_prob, 0.f),
                                  dr::OneMinusEpsilon<Float>);
                ls.value *= sum * inv_prob;
                ls.active &= prob > 0.f;
            }
            ls.word = word;
            ls.active &= (word & detail::EmitterTreeLeafFlag) == 0u;
        });

    value = ls.value;
    aux = ls.aux;
    return ls.word;
}

MI_VARIANT std::pair<typename EmitterTree<Float, Spectrum>::DirectionSample3f, Spectrum>
EmitterTree<Float, Spectrum>::sample_direction(const Interaction3f &ref,
                                             Point2f sample, Mask two_sided,
                                             Mask active) const {
    Selection sel = select(ref, sample.x(), two_sided, active);
    active &= sel.emitter != nullptr;
    if (dr::none_or<false>(active))
        return { dr::zeros<DirectionSample3f>(), Spectrum(0.f) };

    sample.x() = sel.sample;
    auto [ds, spec] = sel.emitter->sample_direction(ref, sample, active);
    ds.pdf *= dr::select(sel.weight > 0.f, dr::rcp(sel.weight), 0.f);
    spec *= sel.weight;
    return { ds, spec };
}

MI_VARIANT Float EmitterTree<Float, Spectrum>::pdf_direction(
    const Interaction3f &ref, const DirectionSample3f &ds, Mask two_sided,
    Mask active) const {
    return ds.emitter->pdf_direction(ref, ds, active) *
           pdf(ref, ds.emitter, two_sided, active);
}

MI_IMPLEMENT_TRAVERSE_CB(EmitterTree, Object)

MI_INSTANTIATE_CLASS(EmitterTree)
NAMESPACE_END(mitsuba)
