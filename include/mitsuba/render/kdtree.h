#pragma once

#include <mitsuba/core/logger.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/tls.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/mesh.h>
#include <unordered_set>
#include <tbb/tbb.h>

/// Compile-time KD-tree depth limit to enable traversal with stack memory
#define MTS_KD_MAXDEPTH 48u

/// OrderedChunkAllocator: don't create chunks smaller than 5MiB
#define MTS_KD_MIN_ALLOC 5*1024u*1024u

/// Grain size for TBB parallelization
#define MTS_KD_GRAIN_SIZE 10240u

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Optimized KD-tree acceleration data structure for n-dimensional
 * (n<=4) shapes and various queries involving them.
 *
 * Note that this class mainly concerns itself with primitives that cover <em>a
 * region</em> of space. For point data, other implementations will be more
 * suitable. The most important application in Mitsuba is the fast construction
 * of high-quality trees for ray tracing. See the class \ref ShapeKDTree for
 * this specialization.
 *
 * The code in this class is a fully generic kd-tree implementation, which can
 * theoretically support any kind of shape. However, subclasses still need to
 * provide the following signatures for a functional implementation:
 *
 * \code
 * /// Return the total number of primitives
 * Size primitive_count() const;
 *
 * /// Return the axis-aligned bounding box of a certain primitive
 * BoundingBox bbox(Index primIdx) const;
 *
 * /// Return the bounding box of a primitive when clipped to another bounding box
 * BoundingBox bbox(Index primIdx, const BoundingBox &aabb) const;
 * \endcode
 *
 * This class follows the "Curiously recurring template" design pattern so that
 * the above functions can be inlined (in particular, no virtual calls will be
 * necessary!).
 *
 * When the kd-tree is initially built, this class optimizes a cost heuristic
 * every time a split plane has to be chosen. For ray tracing, the heuristic is
 * usually the surface area heuristic (SAH), but other choices are possible as
 * well. The tree cost model must be passed as a template argument, which can
 * use a supplied bounding box and split candidate to compute approximate
 * probabilities of recursing into the left and right subrees during a typical
 * kd-tree query operation. See \ref SurfaceAreaHeuristic3f for an example of
 * the interface that must be implemented.
 *
 * The kd-tree construction algorithm creates 'perfect split' trees as outlined
 * in the paper "On Building fast kd-Trees for Ray Tracing, and on doing that
 * in O(N log N)" by Ingo Wald and Vlastimil Havran. This works even when the
 * tree is not meant to be used for ray tracing. For polygonal meshes, the
 * involved Sutherland-Hodgman iterations can be quite expensive in terms of
 * the overall construction time. The \ref set_clip_primitives() method can be
 * used to deactivate perfect splits at the cost of a lower-quality tree.
 *
 * Because the O(N log N) construction algorithm tends to cause many incoherent
 * memory accesses and does not parallelize particularly well, a different
 * method known as <em>Min-Max Binning</em> is used for the top levels of the
 * tree. Min-Max-binning is an approximation to the O(N log N) approach, which
 * works extremely well at the top of the tree (i.e. when there are many
 * elements). This algorithm realized as a series of efficient parallel sweeps
 * that harness the available cores at all levels (even at the root node). Each
 * iteration splits the list of primitives into independent subtrees which can
 * also be processed in parallel. Eventually, the input data is reduced into
 * sufficiently small chunks, at which point the implementation switches over
 * to the more accurate O(N log N) builder. The various thresholds and
 * parameters for these different methods can be accessed and configured via
 * getters and setters of this class.
 */

template <typename BoundingBox_, typename Index_, typename CostModel_,
          typename Derived_> class TShapeKDTree : public Object {
public:
    using BoundingBox = BoundingBox_;
    using CostModel   = CostModel_;
    using Index       = Index_;
    using Size        = Index;
    using Derived     = Derived_;
    using Point       = typename BoundingBox::Point;
    using Vector      = typename BoundingBox::Vector;
    using Scalar      = typename Vector::Scalar;
    using IndexVector = std::vector<Index>;

    static constexpr size_t Dimension = Vector::Size;

    /* ==================================================================== */
    /*                     Public kd-tree interface                         */
    /* ==================================================================== */

    TShapeKDTree(const CostModel &model) : m_cost_model(model) { }

    /// Return the cost model used by the tree construction algorithm
    CostModel cost_model() const { return m_cost_model; }

    /// Return the maximum tree depth (0 == use heuristic)
    Size max_depth() const { return m_max_depth; }

    /// Set the maximum tree depth (0 == use heuristic)
    void set_max_depth(Size max_depth) { m_max_depth = max_depth; }

    /// Return the number of bins used for Min-Max binning
    Size min_max_bins() const { return m_min_max_bins; }

    /// Set the number of bins used for Min-Max binning
    void set_min_max_bins(Size value) { m_min_max_bins = value; }

    /// Return whether primitive clipping is used during tree construction
    bool clip_primitives() const { return m_clip_primitives; }

    /// Set whether primitive clipping is used during tree construction
    void set_clip_primitives(bool clip) { m_clip_primitives = clip; }

    /// Return whether or not bad splits can be "retracted".
    bool retract_bad_splits() const { return m_retract_bad_splits; }

    /// Specify whether or not bad splits can be "retracted".
    void set_retract_bad_splits(bool retract) { m_retract_bad_splits = retract; }

    /**
     * \brief Return the number of bad refines allowed to happen
     * in succession before a leaf node will be created.
     */
    Size max_bad_refines() const { return m_max_bad_refines; }

    /**
     * \brief Set the number of bad refines allowed to happen
     * in succession before a leaf node will be created.
     */
    void set_max_bad_refines(Size value) { m_max_bad_refines = value; }

    /**
     * \brief Return the number of primitives, at which recursion will
     * stop when building the tree.
     */
    Size stop_primitives() const { return m_stop_primitives; }

    /**
     * \brief Set the number of primitives, at which recursion will
     * stop when building the tree.
     */
    void set_stop_primitives(Size value) { m_stop_primitives = value; }

    /**
     * \brief Return the number of primitives, at which the builder will switch
     * from (approximate) Min-Max binning to the accurate O(n log n)
     * optimization method.
     */
    Size exact_primitive_threshold() const { return m_exact_prim_threshold; }

    /**
     * \brief Specify the number of primitives, at which the builder will
     * switch from (approximate) Min-Max binning to the accurate O(n log n)
     * optimization method.
     */
    void set_exact_primitive_threshold(Size value) {
        m_exact_prim_threshold = value;
    }

    /// Return the log level of kd-tree status messages
    ELogLevel log_level() const { return m_log_level; }

    /// Return the log level of kd-tree status messages
    void set_log_level(ELogLevel level) { m_log_level = level; }

    bool ready() const { return (bool) m_nodes; }

    /// Return the bounding box of the entire kd-tree
    const BoundingBox bbox() const { return m_bbox; }

    const Derived& derived() const { return (Derived&) *this; }
    Derived& derived() { return (Derived&) *this; }

    MTS_DECLARE_CLASS()

protected:
    /* ==================================================================== */
    /*                  Essential internal data structures                  */
    /* ==================================================================== */

    /// kd-tree node in 8 bytes.
    struct alignas(8) KDNode {
        union {
            /// Inner node
            struct {
                /// Split plane coordinate
                Scalar split;

                /// X/Y/Z axis specifier
                unsigned int axis : 3;

                /// Offset to left child (max 512M nodes in between in 32 bit)
                unsigned int left_offset : sizeof(Index) * 8 - 3;
            } inner;

            /// Leaf node
            struct {
                #if defined(LITTLE_ENDIAN)
                    /// How many primitives does this leaf reference?
                    unsigned int prim_count : 23;

                    /// Mask bits (all 1s for leaf nodes)
                    unsigned int mask : 9;
                #else /* Swap for big endian machines */
                    /// Mask bits (all 1s for leaf nodes)
                    unsigned int mask : 9;

                    /// How many primitives does this leaf reference?
                    unsigned int prim_count : 23;
                #endif

                /// Start offset of the primitive list
                Index prim_offset;
            } leaf;
        } data;

        /**
         * \brief Initialize a leaf kd-tree node.
         *
         * Returns \c false if the offset or number of primitives is so large
         * that it can't be represented
         */
        bool set_leaf_node(size_t prim_offset, size_t prim_count) {
            data.leaf.prim_count = (unsigned int) prim_count;
            data.leaf.mask      = 0b111111111u;
            data.leaf.prim_offset = (Index) prim_offset;
            return (size_t) data.leaf.prim_offset == prim_offset &&
                   (size_t) data.leaf.prim_count == prim_count;
        }

        /**
         * \brief Initialize an interior kd-tree node.
         *
         * Returns \c false if the offset or number of primitives is so large
         * that it can't be represented
         */
        bool set_inner_node(Index axis, Scalar split, size_t left_offset) {
            data.inner.split = split;
            data.inner.axis = (unsigned int) axis;
            data.inner.left_offset = (Index) left_offset;
            return (size_t) data.inner.left_offset == left_offset;
        }

        /// Is this a leaf node?
        bool leaf() const { return data.leaf.mask == 0b111111111u; }

        /// Assuming this is a leaf node, return the first primitive index
        Index primitive_offset() const { return data.leaf.prim_offset; }

        /// Assuming this is a leaf node, return the number of primitives
        Index primitive_count() const { return data.leaf.prim_count; }

        /// Assuming that this is an inner node, return the relative offset to the left child
        Index left_offset() const { return data.inner.left_offset; }

        /// Return the left child (for interior nodes)
        const KDNode *left() const { return this + data.inner.left_offset; }

        /// Return the left child (for interior nodes)
        const KDNode *right() const { return this + data.inner.left_offset + 1; }

        /// Return the split plane location (for interior nodes)
        Scalar split() const { return data.inner.split; }

        /// Return the split axis (for interior nodes)
        Index axis() const { return (Index) data.inner.axis; }

        /// Return a string representation
        friend std::ostream& operator<<(std::ostream &os, const KDNode &n) {
            if (n.leaf()) {
                os << "KDNode[leaf, primitive_offset=" << n.primitive_offset()
                   << ", primitive_count=" << n.primitive_count() << "]";
            } else {
                os << "KDNode[interior, axis=" << n.axis()
                    << ", split=" << n.split()
                    << ", left_offset=" << n.left_offset() << "]";
            }
            return os;
        }
    };

#if defined(SINGLE_PRECISION)
    static_assert(sizeof(KDNode) == sizeof(Size) + sizeof(Scalar),
                  "kd-tree node has unexpected size. Padding issue?");
#endif

protected:
    /// Enumeration representing the state of a classified primitive in the O(N log N) builder
    enum EPrimClassification : uint8_t {
        EIgnore = 0, ///< Primitive was handled already, ignore from now on
        ELeft   = 1, ///< Primitive is left of the split plane
        ERight  = 2, ///< Primitive is right of the split plane
        EBoth   = 3  ///< Primitive straddles the split plane
    };

    /* ==================================================================== */
    /*                    Specialized memory allocators                     */
    /* ==================================================================== */

    /**
     * \brief Compact storage for primitive classification
     *
     * When classifying primitives with respect to a split plane, a data structure
     * is needed to hold the tertiary result of this operation. This class
     * implements a compact storage (2 bits per entry) in the spirit of the
     * std::vector<bool> specialization.
     */
    class ClassificationStorage {
    public:
        void resize(Size count) {
            if (count != m_count) {
                m_buffer.reset(new uint8_t[(count + 3) / 4]);
                m_count = count;
            }
        }

        void set(Index index, EPrimClassification value) {
            Assert(index < m_count);
            uint8_t *ptr = m_buffer.get() + (index >> 2);
            uint8_t shift = (index & 3) << 1;
            *ptr = (*ptr & ~(3 << shift)) | (value << shift);
        }

        EPrimClassification get(Index index) const {
            Assert(index < m_count);
            uint8_t *ptr = m_buffer.get() + (index >> 2);
            uint8_t shift = (index & 3) << 1;
            return EPrimClassification((*ptr >> shift) & 3);
        }

        /// Return the size (in bytes)
        size_t size() const { return (m_count + 3) / 4; }

    private:
        std::unique_ptr<uint8_t[]> m_buffer;
        Size m_count = 0;
    };

    /**
     * During kd-tree construction, large amounts of memory are required to
     * temporarily hold index and edge event lists. When not implemented
     * properly, these allocations can become a critical bottleneck. The class
     * \ref OrderedChunkAllocator provides a specialized memory allocator,
     * which reserves memory in chunks of at least 512KiB (this number is
     * configurable). An important assumption made by the allocator is that
     * memory will be released in the exact same order in which it was
     * previously allocated. This makes it possible to create an implementation
     * with a very low memory overhead. Note that no locking is done, hence
     * each thread will need its own allocator.
     */
    class OrderedChunkAllocator {
    public:
        OrderedChunkAllocator(size_t min_allocation = MTS_KD_MIN_ALLOC)
                : m_min_allocation(min_allocation) {
            m_chunks.reserve(4);
        }

        ~OrderedChunkAllocator() {
            m_chunks.clear();
        }

        /**
         * \brief Request a block of memory from the allocator
         *
         * Walks through the list of chunks to find one with enough
         * free memory. If no chunk could be found, a new one is created.
         */
        template <typename T> ENOKI_MALLOC T * allocate(size_t size) {
            size *= sizeof(T);

            for (auto &chunk : m_chunks) {
                if (chunk.remainder() >= size) {
                    T* result = reinterpret_cast<T *>(chunk.cur);
                    chunk.cur += size;
                    return result;
                }
            }

            /* No chunk had enough free memory */
            size_t alloc_size = std::max(size, m_min_allocation);

            std::unique_ptr<uint8_t[]> data(new uint8_t[alloc_size]);
            uint8_t *start = data.get(), *cur = start + size;
            m_chunks.emplace_back(std::move(data), cur, alloc_size);

            return reinterpret_cast<T *>(start);
        }

        template <typename T> void release(T *ptr_) {
            auto ptr = reinterpret_cast<uint8_t *>(ptr_);

            for (auto &chunk: m_chunks) {
                if (chunk.contains(ptr)) {
                    chunk.cur = ptr;
                    return;
                }
            }

            #if !defined(NDEBUG)
                for (auto const &chunk : m_chunks) {
                    if (ptr == chunk.start.get() + chunk.size)
                        return; /* Potentially 0-sized buffer, don't be too stringent */
                }

                Throw("OrderedChunkAllocator: Internal error while releasing memory");
            #endif
        }

        /**
         * \brief Shrink the size of the last allocated chunk
         */
        template <typename T> void shrink_allocation(T *ptr_, size_t new_size) {
            auto ptr = reinterpret_cast<uint8_t *>(ptr_);
            new_size *= sizeof(T);

            for (auto &chunk: m_chunks) {
                if (chunk.contains(ptr)) {
                    chunk.cur = ptr + new_size;
                    return;
                }
            }

            #if !defined(NDEBUG)
                if (new_size == 0) {
                    for (auto const &chunk : m_chunks) {
                        if (ptr == chunk.start.get() + chunk.size)
                            return; /* Potentially 0-sized buffer, don't be too stringent */
                    }
                }

                Throw("OrderedChunkAllocator: Internal error while releasing memory");
            #endif
        }

        /// Return the currently allocated number of chunks
        size_t chunk_count() const { return m_chunks.size(); }

        /// Return the total amount of chunk memory in bytes
        size_t size() const {
            size_t result = 0;
            for (auto const &chunk : m_chunks)
                result += chunk.size;
            return result;
        }

        /// Return the total amount of used memory in bytes
        size_t used() const {
            size_t result = 0;
            for (auto const &chunk : m_chunks)
                result += chunk.used();
            return result;
        }

        /// Return a string representation of the chunks
        friend std::ostream& operator<<(std::ostream &os, const OrderedChunkAllocator &o) {
            os << "OrderedChunkAllocator[" << std::endl;
            for (size_t i = 0; i < o.m_chunks.size(); ++i)
                os << "    Chunk " << i << ": " << o.m_chunks[i] << std::endl;
            os << "]";
            return os;
        }

    private:
        struct Chunk {
            std::unique_ptr<uint8_t[]> start;
            uint8_t *cur;
            size_t size;

            Chunk(std::unique_ptr<uint8_t[]> &&start, uint8_t *cur, size_t size)
                : start(std::move(start)), cur(cur), size(size) { }

            size_t used() const { return (size_t) (cur - start.get()); }
            size_t remainder() const { return size - used(); }

            bool contains(uint8_t *ptr) const { return ptr >= start.get() && ptr < start.get() + size; }

            friend std::ostream& operator<<(std::ostream &os, const Chunk &ch) {
                os << (const void *) ch.start.get() << "-" << (const void *) (ch.start.get() + ch.size)
                   << " (size = " << ch.size << ", remainder = " << ch.remainder() << ")";
                return os;
            }
        };

        size_t m_min_allocation;
        std::vector<Chunk> m_chunks;
    };

    /* ==================================================================== */
    /*                    Build-related data structures                     */
    /* ==================================================================== */

    struct BuildContext;

    /// Helper data structure used during tree construction (used by a single thread)
    struct LocalBuildContext {
        ClassificationStorage classification_storage;
        OrderedChunkAllocator left_alloc;
        OrderedChunkAllocator right_alloc;
        BuildContext *ctx = nullptr;

        ~LocalBuildContext() {
            Assert(left_alloc.used() == 0);
            Assert(right_alloc.used() == 0);
            if (ctx)
                ctx->temp_storage += left_alloc.size() + right_alloc.size() +
                                     classification_storage.size();
        }
    };

    /// Helper data structure used during tree construction (shared by all threads)
    struct BuildContext {
        const Derived &derived;
        Thread *thread;
        tbb::concurrent_vector<KDNode> node_storage;
        tbb::concurrent_vector<Index> index_storage;
        ThreadLocal<LocalBuildContext> local;

        /* Keep some statistics about the build process */
        std::atomic<size_t> bad_refines {0};
        std::atomic<size_t> retracted_splits {0};
        std::atomic<size_t> pruned {0};
        std::atomic<size_t> temp_storage {0};
        std::atomic<size_t> work_units {0};
        double exp_traversal_steps = 0;
        double exp_leaves_visited = 0;
        double exp_primitives_queried = 0;
        Size max_prims_in_leaf = 0;
        Size nonempty_leaf_count = 0;
        Size max_depth = 0;
        Size prim_buckets[16] { };

        BuildContext(const Derived &derived)
            : derived(derived), thread(Thread::thread()) { }
    };


    /// Data type for split candidates suggested by the tree cost model
    struct SplitCandidate {
        Scalar cost = std::numeric_limits<Scalar>::infinity();
        Scalar split = 0;
        int axis = 0;
        Size left_count = 0, right_count = 0;
        Size right_bin = 0;       /* used by min-max binning only */
        bool planar_left = false; /* used by the O(n log n) builder only */

        friend std::ostream& operator<<(std::ostream &os, const SplitCandidate &c) {
            os << "SplitCandidate[" << std::endl
               << "  cost = " << c.cost << "," << std::endl
               << "  split = " << c.split << "," << std::endl
               << "  axis = " << c.axis << "," << std::endl
               << "  left_count = " << c.left_count << "," << std::endl
               << "  right_count = " << c.right_count << "," << std::endl
               << "  right_bin = " << c.right_bin << "," << std::endl
               << "  planar_left = " << (c.planar_left ? "yes" : "no") << std::endl
               << "]";
            return os;
        }
    };

    /**
     * \brief Describes the beginning or end of a primitive under orthogonal
     * projection onto different axes
     */
    struct EdgeEvent {
        /// Possible event types
        enum EEventType {
            EEdgeEnd = 0,
            EEdgePlanar = 1,
            EEdgeStart = 2
        };

        /// Dummy constructor
        EdgeEvent() { }

        /// Create a new edge event
        EdgeEvent(EEventType type, int axis, Scalar pos, Index index)
         : pos(pos), index(index), type(type), axis(axis) { }

        /// Return a string representation
        friend std::ostream& operator<<(std::ostream &os, const EdgeEvent &e) {
            os << "EdgeEvent[" << std::endl
                << "  pos = " << e.pos << "," << std::endl
                << "  index = " << e.index << "," << std::endl
                << "  type = ";
            switch (e.type) {
                case EEdgeEnd: os << "end"; break;
                case EEdgeStart: os << "start"; break;
                case EEdgePlanar: os << "planar"; break;
                default: os << "unknown!"; break;
            }
            os << "," << std::endl
               << "  axis = " << e.axis << std::endl
               << "]";
            return os;
        }

        bool operator<(const EdgeEvent &other) const {
            return std::tie(axis, pos, type, index) <
                   std::tie(other.axis, other.pos, other.type, other.index);
        }

        void set_invalid() { pos = 0; index = 0; type = 0; axis = 7; }
        bool valid() const { return axis != 7; }

        /// Plane position
        Scalar pos;
        /// Primitive index
        Index index;
        /// Event type: end/planar/start
        uint16_t type;
        /// Event axis
        uint16_t axis;
    };

    using EdgeEventVector = std::vector<EdgeEvent>;

    static_assert(sizeof(Scalar) + sizeof(Index) + sizeof(uint32_t) ==
                  sizeof(EdgeEvent), "EdgeEvent has an unexpected size!");

    /**
     * \brief Min-max binning data structure with parallel binning & partitioning steps
     *
     * See
     *   "Highly Parallel Fast KD-tree Construction for Interactive Ray Tracing of
     *   Dynamic Scenes" by M. Shevtsov, A. Soupikov and A. Kapustin
     */
    class MinMaxBins {
    public:
        MinMaxBins(Size bin_count, const BoundingBox &bbox)
            : m_bins(bin_count * Dimension * 2, Size(0)), m_bin_count(bin_count),
              m_inv_bin_size(1 / (bbox.extents() / (Scalar) bin_count)),
              m_max_bin(bin_count - 1), m_bbox(bbox) {
            Assert(bbox.valid());
        }

        MinMaxBins(const MinMaxBins &other)
            : m_bins(other.m_bins), m_bin_count(other.m_bin_count),
              m_inv_bin_size(other.m_inv_bin_size), m_max_bin(other.m_max_bin),
              m_bbox(other.m_bbox) { }

        MinMaxBins(MinMaxBins &&other)
            : m_bins(std::move(other.m_bins)), m_bin_count(other.m_bin_count),
              m_inv_bin_size(other.m_inv_bin_size), m_max_bin(other.m_max_bin),
              m_bbox(other.m_bbox) { }

        void operator=(const MinMaxBins &other) {
            m_bins = other.m_bins;
            m_bin_count = other.m_bin_count;
            m_inv_bin_size = other.m_inv_bin_size;
            m_max_bin = other.m_max_bin;
            m_bbox = other.m_bbox;
        }

        void operator=(MinMaxBins &&other) {
            m_bins = std::move(other.m_bins);
            m_bin_count = other.m_bin_count;
            m_inv_bin_size = other.m_inv_bin_size;
            m_max_bin = other.m_max_bin;
            m_bbox = other.m_bbox;
        }

        MinMaxBins& operator+=(const MinMaxBins &other) {
            Assert(m_bins.size() == other.m_bins.size());
            for (Size i = 0; i < m_bins.size(); ++i)
                m_bins[i] += other.m_bins[i];
            return *this;
        }

        MTS_INLINE void put(BoundingBox bbox) {
            using IndexArray = Array<Index, 3>;
            const IndexArray offset_min = IndexArray(0, 2 * m_bin_count, 4 * m_bin_count);
            const IndexArray offset_max = offset_min + 1;
            Index *ptr = m_bins.data();

            Assert(bbox.valid());
            Vector rel_min = (bbox.min - m_bbox.min) * m_inv_bin_size;
            Vector rel_max = (bbox.max - m_bbox.min) * m_inv_bin_size;

            rel_min = min(max(rel_min, zero<Vector>()), m_max_bin);
            rel_max = min(max(rel_max, zero<Vector>()), m_max_bin);

            IndexArray index_min = IndexArray(rel_min);
            IndexArray index_max = IndexArray(rel_max);

            Assert(all(index_min <= index_max));
            index_min = index_min + index_min + offset_min;
            index_max = index_max + index_max + offset_max;

            IndexArray minCounts = gather<IndexArray>(ptr, index_min),
                       maxCounts = gather<IndexArray>(ptr, index_max);

            scatter(ptr, minCounts + 1, index_min);
            scatter(ptr, maxCounts + 1, index_max);
        }

        SplitCandidate best_candidate(Size prim_count, const CostModel &model) const {
            const Index *bin = m_bins.data();
            SplitCandidate best;

            Vector step = m_bbox.extents() / Scalar(m_bin_count);

            for (Index axis = 0; axis < Dimension; ++axis) {
                SplitCandidate candidate;

                /* Initially: all primitives to the right, none on the left */
                candidate.left_count = 0;
                candidate.right_count = prim_count;
                candidate.right_bin = 0;
                candidate.axis = axis;
                candidate.split = m_bbox.min[axis];

                for (Index i = 0; i < m_bin_count; ++i) {
                    /* Evaluate the cost model and keep the best candidate */
                    candidate.cost = model.inner_cost(
                        axis, candidate.split,
                        model.leaf_cost(candidate.left_count),
                        model.leaf_cost(candidate.right_count));

                    if (candidate.cost < best.cost)
                        best = candidate;

                    /* Move one bin to the right and

                       1. Increase left_count by the number of primitives which
                          started in the bin (thus they at least overlap with
                          the left interval). This information is stored in the MIN
                          bin.

                       2. Reduce right_count by the number of primitives which
                          ended (thus they are entirely on the left). This
                          information is stored in the MAX bin.
                    */
                    candidate.left_count  += *bin++; /* MIN-bin */
                    candidate.right_count -= *bin++; /* MAX-bin */
                    candidate.right_bin++;
                    candidate.split += step[axis];
                }

                /* Evaluate the cost model and keep the best candidate */
                candidate.cost = model.inner_cost(
                    axis, candidate.split,
                    model.leaf_cost(candidate.left_count),
                    model.leaf_cost(candidate.right_count));

                if (candidate.cost < best.cost)
                    best = candidate;

                Assert(candidate.left_count == prim_count);
                Assert(candidate.right_count == 0);
            }

            Assert(bin == m_bins.data() + m_bins.size());
            Assert(best.left_count + best.right_count >= prim_count);

            if (best.right_bin == 0) {
                best.split = m_bbox.min[best.axis];
            } else if (best.right_bin == m_bin_count) {
                best.split = m_bbox.max[best.axis];
            } else {
                auto predicate = [
                    inv_bin_size = m_inv_bin_size[best.axis],
                    offset = m_bbox.min[best.axis],
                    right_bin = best.right_bin
                ](Scalar value) {
                    /* Predicate which says whether a value falls on the left
                       of the chosen split plane. This function is meant to
                       behave exactly the same way as put() above. */
                    return Index((value - offset) * inv_bin_size) < right_bin;
                };

                /* Find the last floating point value which is classified as
                   falling into the left subtree. Due the various rounding
                   errors that are involved, it's tricky to compute this
                   variable using an explicit floating point expression. The
                   code below bisects the interval to find this value, which is
                   guaranteed to work (this takes ~ 20-30 iterations) */
                best.split = math::bisect<Scalar>(
                    m_bbox.min[best.axis],
                    m_bbox.max[best.axis],
                    predicate
                );

                /* Double-check that it worked */
                Assert(predicate(best.split));
                Assert(!predicate(std::nextafter(
                    best.split, std::numeric_limits<float>::infinity())));
            }

            return best;
        }

        struct Partition {
            IndexVector left_indices;
            IndexVector right_indices;
            BoundingBox left_bounds;
            BoundingBox right_bounds;
        };

        /**
         * \brief Given a suitable split candidate, compute tight bounding
         * boxes for the left and right subtrees and return associated
         * primitive lists.
         */
        Partition partition(const Derived &derived,
                            const IndexVector &indices,
                            const SplitCandidate &split) {
            const int axis = split.axis;
            const Scalar offset = m_bbox.min[axis],
                         max_bin = m_max_bin[axis],
                         inv_bin_size = m_inv_bin_size[axis];

            tbb::spin_mutex mutex;
            IndexVector left_indices(split.left_count);
            IndexVector right_indices(split.right_count);
            Size left_count = 0, right_count = 0;
            BoundingBox left_bounds, right_bounds;

            tbb::parallel_for(
                tbb::blocked_range<Size>(0u, Size(indices.size()), MTS_KD_GRAIN_SIZE),
                [&](const tbb::blocked_range<Index> &range) {
                    IndexVector left_indices_local, right_indices_local;
                    BoundingBox left_bounds_local, right_bounds_local;

                    left_indices_local.reserve(range.size());
                    right_indices_local.reserve(range.size());

                    for (Size i = range.begin(); i != range.end(); ++i) {
                        const Index prim_index = indices[i];
                        const BoundingBox prim_bbox = derived.bbox(prim_index);

                        Scalar rel_min = (prim_bbox.min[axis] - offset) * inv_bin_size;
                        Scalar rel_max = (prim_bbox.max[axis] - offset) * inv_bin_size;

                        rel_min = std::min(std::max(rel_min, Scalar(0)), max_bin);
                        rel_max = std::min(std::max(rel_max, Scalar(0)), max_bin);

                        const Size index_min = (Size) rel_min,
                                   index_max = (Size) rel_max;

                        if (index_max < split.right_bin) {
                            left_indices_local.push_back(prim_index);
                            left_bounds_local.expand(prim_bbox);
                        } else if (index_min >= split.right_bin) {
                            right_indices_local.push_back(prim_index);
                            right_bounds_local.expand(prim_bbox);
                        } else {
                            left_indices_local.push_back(prim_index);
                            right_indices_local.push_back(prim_index);
                            left_bounds_local.expand(prim_bbox);
                            right_bounds_local.expand(prim_bbox);
                        }
                    }

                    /* Merge into global results */
                    Index *target_left, *target_right;

                    /* critical section */ {
                        tbb::spin_mutex::scoped_lock lock(mutex);
                        target_left = &left_indices[left_count];
                        target_right = &right_indices[right_count];
                        left_count += (Index) left_indices_local.size();
                        right_count += (Index) right_indices_local.size();
                        left_bounds.expand(left_bounds_local);
                        right_bounds.expand(right_bounds_local);
                    }

                    memcpy(target_left, left_indices_local.data(),
                           left_indices_local.size() * sizeof(Size));
                    memcpy(target_right, right_indices_local.data(),
                           right_indices_local.size() * sizeof(Size));
                }
            );


            Assert(left_count == split.left_count);
            Assert(right_count == split.right_count);

            return { std::move(left_indices), std::move(right_indices),
                     left_bounds, right_bounds };
        }

    protected:
        std::vector<Size> m_bins;
        Size m_bin_count;
        Vector m_inv_bin_size;
        Vector m_max_bin;
        BoundingBox m_bbox;
    };


    /**
     * \brief TBB task for building subtrees in parallel
     *
     * This class is responsible for building a subtree of the final kd-tree.
     * It recursively spawns new tasks for its respective subtrees to enable
     * parallel construction.
     *
     * At the top of the tree, it uses min-max-binning and parallel reductions
     * to create sufficient parallelism. When the number of elements is
     * sufficiently small, it switches to a more accurate O(N log N) builder
     * which uses normal recursion on the stack (i.e. it does not spawn further
     * parallel pieces of work).
     */
    class BuildTask : public tbb::task {
    public:
        using NodeIterator  = typename tbb::concurrent_vector<KDNode>::iterator;
        using IndexIterator = typename tbb::concurrent_vector<Index>::iterator;

        /// Context with build-specific variables (shared by all threads/tasks)
        BuildContext &m_ctx;

        /// Local context with thread local variables
        LocalBuildContext *m_local = nullptr;

        /// Node to be initialized by this task
        NodeIterator m_node;

        /// Index list of primitives to be organized
        IndexVector m_indices;

        /// Bounding box of the node
        BoundingBox m_bbox;

        /// Tighter bounding box of the contained primitives
        BoundingBox m_tight_bbox;

        /// Depth of the node within the tree
        Size m_depth;

        /// Number of "bad refines" so far
        Size m_bad_refines;

        /// This scalar should be set to the final cost when done
        Scalar *m_cost;

        BuildTask(BuildContext &ctx, const NodeIterator &node,
                  IndexVector &&indices, const BoundingBox bbox,
                  const BoundingBox &tight_bbox, Index depth, Size bad_refines,
                  Scalar *cost)
            : m_ctx(ctx), m_node(node), m_indices(std::move(indices)),
              m_bbox(bbox), m_tight_bbox(tight_bbox), m_depth(depth),
              m_bad_refines(bad_refines), m_cost(cost) {
            Assert(m_bbox.contains(tight_bbox));
        }

        /// Run one iteration of min-max binning and spawn recursive tasks
        task *execute() {
            ThreadEnvironment env(m_ctx.thread);
            Size prim_count = Size(m_indices.size());
            const Derived &derived = m_ctx.derived;

            m_ctx.work_units++;

            /* ==================================================================== */
            /*                           Stopping criteria                          */
            /* ==================================================================== */

            if (prim_count <= derived.stop_primitives() ||
                m_depth >= derived.max_depth() || m_tight_bbox.collapsed()) {
                make_leaf(std::move(m_indices));
                return nullptr;
            }

            if (prim_count <= derived.exact_primitive_threshold()) {
                *m_cost = transition_to_nlogn();
                return nullptr;
            }

            /* ==================================================================== */
            /*                              Binning                                 */
            /* ==================================================================== */

            /* Accumulate all shapes into bins */
            MinMaxBins bins = tbb::parallel_reduce(
                tbb::blocked_range<Size>(0u, prim_count, MTS_KD_GRAIN_SIZE),
                MinMaxBins(derived.min_max_bins(), m_tight_bbox),

                /* MAP: Bin a number of shapes and return the resulting 'MinMaxBins' data structure */
                [&](const tbb::blocked_range<Index> &range, MinMaxBins bins) {
                    for (Index i = range.begin(); i != range.end(); ++i)
                        bins.put(derived.bbox(m_indices[i]));
                    return bins;
                },

                /* REDUCE: Combine two 'MinMaxBins' data structures */
                [](MinMaxBins b1, const MinMaxBins &b2) {
                    b1 += b2;
                    return b1;
                }
            );

            /* ==================================================================== */
            /*                        Split candidate search                        */
            /* ==================================================================== */

            CostModel model(derived.cost_model());
            model.set_bounding_box(m_bbox);
            auto best = bins.best_candidate(prim_count, model);

            Assert(std::isfinite(best.cost));
            Assert(best.split >= m_bbox.min[best.axis]);
            Assert(best.split <= m_bbox.max[best.axis]);

            /* Allow a few bad refines in sequence before giving up */
            Scalar leaf_cost = model.leaf_cost(prim_count);
            if (best.cost >= leaf_cost) {
                if ((best.cost > 4 * leaf_cost && prim_count < 16)
                    || m_bad_refines >= derived.max_bad_refines()) {
                    make_leaf(std::move(m_indices));
                    return nullptr;
                }
                ++m_bad_refines;
                m_ctx.bad_refines++;
            }

            /* ==================================================================== */
            /*                            Partitioning                              */
            /* ==================================================================== */

            auto partition = bins.partition(derived, m_indices, best);

            /* Release index list */
            IndexVector().swap(m_indices);

            /* ==================================================================== */
            /*                              Recursion                               */
            /* ==================================================================== */

            NodeIterator children = m_ctx.node_storage.grow_by(2);
            Size left_offset(std::distance(m_node, children));

            if (!m_node->set_inner_node(best.axis, best.split, left_offset))
                Throw("Internal error during kd-tree construction: unable to store "
                      "overly large offset to left child node (%i)", left_offset);

            BoundingBox left_bounds(m_bbox), right_bounds(m_bbox);

            left_bounds.max[best.axis] = Scalar(best.split);
            right_bounds.min[best.axis] = Scalar(best.split);

            partition.left_bounds.clip(left_bounds);
            partition.right_bounds.clip(right_bounds);

            Scalar left_cost = 0, right_cost = 0;

            BuildTask &left_task = *new (allocate_child()) BuildTask(
                m_ctx, children, std::move(partition.left_indices),
                left_bounds, partition.left_bounds, m_depth+1,
                m_bad_refines, &left_cost);

            BuildTask &right_task = *new (allocate_child()) BuildTask(
                m_ctx, std::next(children), std::move(partition.right_indices),
                right_bounds, partition.right_bounds, m_depth + 1,
                m_bad_refines, &right_cost);

            set_ref_count(3);
            spawn(left_task);
            spawn(right_task);
            wait_for_all();

            /* ==================================================================== */
            /*                           Final decision                             */
            /* ==================================================================== */

            *m_cost = model.inner_cost(
                best.axis,
                best.split,
                left_cost, right_cost
            );

            /* Tear up bad (i.e. costly) subtrees and replace them with leaf nodes */
            if (unlikely(*m_cost > leaf_cost && derived.retract_bad_splits())) {
                std::unordered_set<Index> temp;
                traverse(m_node, temp);
                m_ctx.retracted_splits++;
                make_leaf(std::move(temp));
            }

            return nullptr;
        }

        /// Recursively run the O(N log N builder)
        Scalar build_nlogn(NodeIterator node, Size prim_count,
                           EdgeEvent *events_start, EdgeEvent *events_end,
                           const BoundingBox3f &bbox, Size depth,
                           Size bad_refines, bool left_child = true) {
            const Derived &derived = m_ctx.derived;

            /* Initialize the tree cost model */
            CostModel model(derived.cost_model());
            model.set_bounding_box(bbox);
            Scalar leaf_cost = model.leaf_cost(prim_count);

            /* ==================================================================== */
            /*                           Stopping criteria                          */
            /* ==================================================================== */

            if (prim_count <= derived.stop_primitives() || depth >= derived.max_depth()) {
                make_leaf(node, prim_count, events_start, events_end);
                return leaf_cost;
            }

            /* ==================================================================== */
            /*                        Split candidate search                        */
            /* ==================================================================== */

            /* First, find the optimal splitting plane according to the
               tree construction heuristic. To do this in O(n), the search is
               implemented as a sweep over the edge events */

            /* Initially, the split plane is placed left of the scene
               and thus all geometry is on its right side */
            Size left_count[Dimension], right_count[Dimension];
            for (size_t i = 0; i < Dimension; ++i) {
                left_count[i] = 0;
                right_count[i] = prim_count;
            }

            /* Keep track of where events for different axes start */
            EdgeEvent* events_by_dimension[Dimension + 1] { };
            events_by_dimension[0] = events_start;
            events_by_dimension[Dimension] = events_end;

            /* Iterate over all events and find the best split plane */
            SplitCandidate best;
            for (auto event = events_start; event != events_end; ) {
                /* Record the current position and count the number
                   and type of remaining events that are also here. */
                Size num_start = 0, num_end = 0, num_planar = 0;
                int axis = event->axis;
                Scalar pos = event->pos;

                while (event < events_end && event->pos == pos && event->axis == axis) {
                    switch (event->type) {
                        case EdgeEvent::EEdgeStart:  ++num_start;  break;
                        case EdgeEvent::EEdgePlanar: ++num_planar; break;
                        case EdgeEvent::EEdgeEnd:    ++num_end;    break;
                    }
                    ++event;
                }

                /* Keep track of the beginning of each dimension */
                if (event < events_end && event->axis != axis)
                    events_by_dimension[event->axis] = event;

                /* The split plane can now be moved onto 't'. Accordingly, all planar
                   and ending primitives are removed from the right side */
                right_count[axis] -= num_planar + num_end;

                /* Check if the edge event is out of bounds -- when primitive
                   clipping is active, this should never happen! */
                Assert(!(derived.clip_primitives() &&
                         (pos < bbox.min[axis] || pos > bbox.max[axis])));

                /* Calculate a score using the tree construction heuristic */
                if (likely(pos > bbox.min[axis] && pos < bbox.max[axis])) {
                    Size num_left = left_count[axis] + num_planar,
                         num_right = right_count[axis];

                    Scalar cost = model.inner_cost(
                        axis, pos, model.leaf_cost(num_left),
                        model.leaf_cost(num_right));

                    if (cost < best.cost) {
                        best.cost = cost;
                        best.split = pos;
                        best.axis = axis;
                        best.left_count = num_left;
                        best.right_count = num_right;
                        best.planar_left = true;
                    }

                    if (num_planar != 0) {
                        /* There are planar events here -- also consider
                           placing them on the right side */
                        num_left = left_count[axis];
                        num_right = right_count[axis] + num_planar;

                        cost = model.inner_cost(
                            axis, pos, model.leaf_cost(num_left),
                            model.leaf_cost(num_right));

                        if (cost < best.cost) {
                            best.cost = cost;
                            best.split = pos;
                            best.axis = axis;
                            best.left_count = num_left;
                            best.right_count = num_right;
                            best.planar_left = false;
                        }
                    }
                }

                /* The split plane is moved past 't'. All prims,
                    which were planar on 't', are moved to the left
                    side. Also, starting prims are now also left of
                    the split plane. */
                left_count[axis] += num_start + num_planar;
            }

            /* Sanity checks. Everything should now be left of the split plane */
            for (size_t i = 0; i < Dimension; ++i) {
                Assert(right_count[i] == 0 && left_count[i] == prim_count);
                Assert(events_by_dimension[i] != events_end && events_by_dimension[i]->axis == i);
                Assert((i == 0) || ((events_by_dimension[i]-1)->axis == i - 1));
            }

            /* Allow a few bad refines in sequence before giving up */
            if (best.cost >= leaf_cost) {
                if ((best.cost > 4 * leaf_cost && prim_count < 16)
                    || bad_refines >= derived.max_bad_refines()
                    || !std::isfinite(best.cost)) {
                    make_leaf(node, prim_count, events_start, events_end);
                    return leaf_cost;
                }
                ++bad_refines;
                m_ctx.bad_refines++;
            }

            /* ==================================================================== */
            /*                      Primitive Classification                        */
            /* ==================================================================== */

            auto &classification = m_local->classification_storage;

            /* Initially mark all prims as being located on both sides */
            for (auto event = events_by_dimension[best.axis];
                 event != events_by_dimension[best.axis + 1]; ++event)
                classification.set(event->index, EPrimClassification::EBoth);

            Size prims_left = 0, prims_right = 0;
            for (auto event = events_by_dimension[best.axis];
                 event != events_by_dimension[best.axis + 1]; ++event) {

                if (event->type == EdgeEvent::EEdgeEnd &&
                    event->pos <= best.split) {
                    /* Fully on the left side (the primitive's interval ends
                       before (or on) the split plane) */
                    Assert(classification.get(event->index) == EPrimClassification::EBoth);
                    classification.set(event->index, EPrimClassification::ELeft);
                    prims_left++;
                } else if (event->type == EdgeEvent::EEdgeStart &&
                           event->pos >= best.split) {
                    /* Fully on the right side (the primitive's interval
                       starts after (or on) the split plane) */
                    Assert(classification.get(event->index) == EPrimClassification::EBoth);
                    classification.set(event->index, EPrimClassification::ERight);
                    prims_right++;
                } else if (event->type == EdgeEvent::EEdgePlanar) {
                    /* If the planar primitive is not on the split plane,
                       the classification is easy. Otherwise, place it on
                       the side with the lower cost */
                    Assert(classification.get(event->index) == EPrimClassification::EBoth);
                    if (event->pos < best.split ||
                        (event->pos == best.split && best.planar_left)) {
                        classification.set(event->index, EPrimClassification::ELeft);
                        prims_left++;
                    } else if (event->pos > best.split ||
                               (event->pos == best.split && !best.planar_left)) {
                        classification.set(event->index, EPrimClassification::ERight);
                        prims_right++;
                    }
                }
            }

            Size prims_both = prim_count - prims_left - prims_right;

            /* Some sanity checks */
            Assert(prims_left + prims_both == best.left_count);
            Assert(prims_right + prims_both == best.right_count);

            /* ==================================================================== */
            /*                            Partitioning                              */
            /* ==================================================================== */

            BoundingBox left_bbox = bbox, right_bbox = bbox;
            left_bbox.max[best.axis] = best.split;
            right_bbox.min[best.axis] = best.split;

            Size pruned_left = 0, pruned_right = 0;

            auto &left_alloc = m_local->left_alloc;
            auto &right_alloc = m_local->right_alloc;

            EdgeEvent *left_events_start, *right_events_start,
                      *left_events_end, *right_events_end;

            /* First, allocate a conservative amount of scratch space for
               the final event lists and then resize it to the actual used
               amount */
            if (left_child) {
                left_events_start = events_start;
                right_events_start = right_alloc.template allocate<EdgeEvent>(
                    best.right_count * 2 * Dimension);
            } else {
                left_events_start = left_alloc.template allocate<EdgeEvent>(
                    best.left_count * 2 * Dimension);
                right_events_start = events_start;
            }
            left_events_end = left_events_start;
            right_events_end = right_events_start;

            if (prims_both == 0 || !derived.clip_primitives()) {
                /* Fast path: no clipping needed. */
                for (auto it = events_start; it != events_end; ++it) {
                    auto event = *it;

                    /* Fetch the classification of the current event */
                    switch (classification.get(event.index)) {
                        case EPrimClassification::ELeft:
                            *left_events_end++ = event;
                            break;

                        case EPrimClassification::ERight:
                            *right_events_end++ = event;
                            break;

                        case EPrimClassification::EBoth:
                            *left_events_end++ = event;
                            *right_events_end++ = event;
                            break;

                        default:
                            Assert(false);
                    }
                }

                Assert((Size) (left_events_end - left_events_start) <= best.left_count* 2 * Dimension);
                Assert((Size) (right_events_end - right_events_start) <= best.right_count * 2 * Dimension);
            } else {
                /* Slow path: some primitives are straddling the split plane
                   and primitive clipping is enabled. They will generate new
                   events that have to be sorted and merged into the current
                   sorted event lists. Start by allocating some more scratch
                   space for this.. */
                EdgeEvent *temp_left_events_start, *temp_left_events_end,
                    *temp_right_events_start, *temp_right_events_end,
                    *new_left_events_start, *new_left_events_end,
                    *new_right_events_start, *new_right_events_end;

                temp_left_events_start = temp_left_events_end =
                    left_alloc.template allocate<EdgeEvent>(prims_left * 2 * Dimension);
                temp_right_events_start = temp_right_events_end =
                    right_alloc.template allocate<EdgeEvent>(prims_right * 2 * Dimension);
                new_left_events_start = new_left_events_end =
                    left_alloc.template allocate<EdgeEvent>(prims_both * 2 * Dimension);
                new_right_events_start = new_right_events_end =
                    right_alloc.template allocate<EdgeEvent>(prims_both * 2 * Dimension);

                for (auto it = events_start; it != events_end; ++it) {
                    auto event = *it;

                    /* Fetch the classification of the current event */
                    switch (classification.get(event.index)) {
                        case EPrimClassification::ELeft:
                            *temp_left_events_end++ = event;
                            break;

                        case EPrimClassification::ERight:
                            *temp_right_events_end++ = event;
                            break;

                        case EPrimClassification::EIgnore:
                            break;

                        case EPrimClassification::EBoth: {
                                BoundingBox clippedLeft  = derived.bbox(event.index, left_bbox);
                                BoundingBox clippedRight = derived.bbox(event.index, right_bbox);

                                Assert(left_bbox.contains(clippedLeft) || !clippedLeft.valid());
                                Assert(right_bbox.contains(clippedRight) || !clippedRight.valid());

                                if (clippedLeft.valid() &&
                                    clippedLeft.surface_area() > 0) {
                                    for (Index axis = 0; axis < Dimension; ++axis) {
                                        Scalar min = clippedLeft.min[axis],
                                               max = clippedLeft.max[axis];

                                        if (min != max) {
                                            *new_left_events_end++ = EdgeEvent(
                                                EdgeEvent::EEdgeStart, axis, min, event.index);
                                            *new_left_events_end++ = EdgeEvent(
                                                EdgeEvent::EEdgeEnd, axis, max, event.index);
                                        } else {
                                            *new_left_events_end++ = EdgeEvent(
                                                EdgeEvent::EEdgePlanar, axis, min, event.index);
                                        }
                                    }
                                } else {
                                    pruned_left++;
                                }

                                if (clippedRight.valid() &&
                                    clippedRight.surface_area() > 0) {
                                    for (Index axis = 0; axis < Dimension; ++axis) {
                                        Scalar min = clippedRight.min[axis],
                                               max = clippedRight.max[axis];

                                        if (min != max) {
                                            *new_right_events_end++ = EdgeEvent(
                                                EdgeEvent::EEdgeStart, axis, min, event.index);
                                            *new_right_events_end++ = EdgeEvent(
                                                EdgeEvent::EEdgeEnd, axis, max, event.index);
                                        } else {
                                            *new_right_events_end++ = EdgeEvent(
                                                EdgeEvent::EEdgePlanar, axis, min, event.index);
                                        }
                                    }
                                } else {
                                    pruned_right++;
                                }

                                /* Set classification to 'EIgnore' to ensure that
                                   clipping occurs only once */
                                classification.set(
                                    event.index, EPrimClassification::EIgnore);
                            }
                            break;

                        default:
                            Assert(false);
                    }
                }

                Assert((Size) (temp_left_events_end - temp_left_events_start) <= prims_left * 2 * Dimension);
                Assert((Size) (temp_right_events_end - temp_right_events_start) <= prims_right * 2 * Dimension);
                Assert((Size) (new_left_events_end - new_left_events_start) <= prims_both * 2 * Dimension);
                Assert((Size) (new_right_events_end - new_right_events_start) <= prims_both * 2 * Dimension);

                m_ctx.pruned += pruned_left + pruned_right;

                /* Sort the events due to primitives which overlap the split plane */
                std::sort(new_left_events_start, new_left_events_end);
                std::sort(new_right_events_start, new_right_events_end);

                /* Merge the left list */
                left_events_end = std::merge(temp_left_events_start,
                    temp_left_events_end, new_left_events_start,
                    new_left_events_end, left_events_start);

                /* Merge the right list */
                right_events_end = std::merge(temp_right_events_start,
                    temp_right_events_end, new_right_events_start,
                    new_right_events_end, right_events_start);

                /* Release temporary memory */
                left_alloc.release(new_left_events_start);
                right_alloc.release(new_right_events_start);
                left_alloc.release(temp_left_events_start);
                right_alloc.release(temp_right_events_start);
            }

            /* Shrink the edge event storage now that we know exactly how
               many events are on each side */
            left_alloc.shrink_allocation(left_events_start,
                                        left_events_end - left_events_start);
            right_alloc.shrink_allocation(right_events_start,
                                         right_events_end - right_events_start);

            /* ==================================================================== */
            /*                              Recursion                               */
            /* ==================================================================== */

            NodeIterator children = m_ctx.node_storage.grow_by(2);

            Size left_offset(std::distance(node, children));

            if (!node->set_inner_node(best.axis, best.split, left_offset))
                Throw("Internal error during kd-tree construction: unable "
                      "to store overly large offset to left child node (%i)",
                      left_offset);
            if (node->left() == &*node || node->right() == &*node)
                Throw("Internal error..");

            Scalar left_cost =
                build_nlogn(children, best.left_count - pruned_left,
                            left_events_start, left_events_end, left_bbox,
                            depth + 1, bad_refines, true);

            Scalar right_cost =
                build_nlogn(std::next(children), best.right_count - pruned_right,
                            right_events_start, right_events_end, right_bbox,
                            depth + 1, bad_refines, false);

            /* Release the index lists not needed by the children anymore */
            if (left_child)
                right_alloc.release(right_events_start);
            else
                left_alloc.release(left_events_start);

            /* ==================================================================== */
            /*                           Final decision                             */
            /* ==================================================================== */

            Scalar final_cost =
                model.inner_cost(best.axis, best.split, left_cost, right_cost);

            /* Tear up bad (i.e. costly) subtrees and replace them with leaf nodes */
            if (unlikely(final_cost > leaf_cost && derived.retract_bad_splits())) {
                std::unordered_set<Index> temp;
                traverse(node, temp);

                auto it = m_ctx.index_storage.grow_by(temp.begin(), temp.end());
                Size offset(std::distance(m_ctx.index_storage.begin(), it));

                if (!node->set_leaf_node(offset, temp.size()))
                    Throw("Internal error: could not create leaf node with %i "
                          "primitives -- too much geometry?", m_indices.size());

                m_ctx.retracted_splits++;
                return leaf_cost;
            }

            return final_cost;
        }

        /// Create an initial sorted edge event list and start the O(N log N) builder
        Scalar transition_to_nlogn() {
            const auto &derived = m_ctx.derived;
            m_local = &((LocalBuildContext &) m_ctx.local);

            Size prim_count = Size(m_indices.size()), final_prim_count = prim_count;

            /* We don't yet know how many edge events there will be. Allocate a
               conservative amount and shrink the buffer later on. */
            Size initial_size = prim_count * 2 * Dimension;

            EdgeEvent *events_start =
                m_local->left_alloc.template allocate<EdgeEvent>(initial_size),
                *events_end = events_start + initial_size;

            for (Size i = 0; i<prim_count; ++i) {
                Index prim_index = m_indices[i];
                BoundingBox prim_bbox = derived.bbox(prim_index, m_bbox);
                bool valid = prim_bbox.valid() && prim_bbox.surface_area() > 0;

                if (unlikely(!valid)) {
                    --final_prim_count;
                    m_ctx.pruned++;
                }

                for (Index axis = 0; axis < Dimension; ++axis) {
                    Scalar min = prim_bbox.min[axis], max = prim_bbox.max[axis];
                    Index offset = (Index) (axis * prim_count + i) * 2;

                    if (unlikely(!valid)) {
                        events_start[offset  ].set_invalid();
                        events_start[offset+1].set_invalid();
                    } else if (min == max) {
                        events_start[offset  ] = EdgeEvent(EdgeEvent::EEdgePlanar, axis, min, prim_index);
                        events_start[offset+1].set_invalid();
                    } else {
                        events_start[offset  ] = EdgeEvent(EdgeEvent::EEdgeStart, axis, min, prim_index);
                        events_start[offset+1] = EdgeEvent(EdgeEvent::EEdgeEnd,   axis, max, prim_index);
                    }
                }
            }

            /* Release index list */
            IndexVector().swap(m_indices);

            /* Sort the events list and remove invalid ones from the end */
            std::sort(events_start, events_end);
            while (events_start != events_end && !(events_end-1)->valid())
                --events_end;

            m_local->left_alloc.template shrink_allocation<EdgeEvent>(
                events_start, events_end - events_start);
            m_local->classification_storage.resize(derived.primitive_count());
            m_local->ctx = &m_ctx;

            Float cost = build_nlogn(m_node, final_prim_count, events_start,
                                     events_end, m_bbox, m_depth, 0);

            m_local->left_alloc.release(events_start);

            return cost;
        }

        /// Create a leaf node using the given set of indices (called by min-max binning)
        template <typename T> void make_leaf(T &&indices) {
            auto it = m_ctx.index_storage.grow_by(indices.begin(), indices.end());
            Size offset(std::distance(m_ctx.index_storage.begin(), it));

            if (!m_node->set_leaf_node(offset, indices.size()))
                Throw("Internal error: could not create leaf node with %i "
                      "primitives -- too much geometry?", m_indices.size());

            *m_cost = m_ctx.derived.cost_model().leaf_cost(Size(indices.size()));
        }

        /// Create a leaf node using the given edge event list (called by the O(N log N) builder)
        void make_leaf(NodeIterator node, Size prim_count, EdgeEvent *events_start,
                      EdgeEvent *events_end) const {
            auto it = m_ctx.index_storage.grow_by(prim_count);
            Size offset(std::distance(m_ctx.index_storage.begin(), it));

            if (!node->set_leaf_node(offset, prim_count))
                Throw("Internal error: could not create leaf node with %i "
                      "primitives -- too much geometry?", prim_count);

            for (auto event = events_start; event != events_end; ++event) {
                if (event->axis != 0)
                    break;
                if (event->type == EdgeEvent::EEdgeStart ||
                    event->type == EdgeEvent::EEdgePlanar) {
                    Assert(--prim_count >= 0);
                    *it++ = event->index;
                }
            }

            Assert(prim_count == 0);
        }

        /// Traverse a subtree and collect all encountered primitive references in a set
        void traverse(NodeIterator node, std::unordered_set<Index> &result) {
            if (node->leaf()) {
                for (Size i = 0; i < node->primitive_count(); ++i)
                    result.insert(m_ctx.index_storage[node->primitive_offset() + i]);
            } else {
                NodeIterator left_child = node + node->left_offset(),
                             right_child = left_child + 1;
                traverse(left_child, result);
                traverse(right_child, result);
            }
        }
    };

    void compute_statistics(BuildContext &ctx, const KDNode *node,
                            const BoundingBox &bbox, Size depth) {
        if (depth > ctx.max_depth)
            ctx.max_depth = depth;

        if (node->leaf()) {
            auto prim_count = node->primitive_count();
            double value = (double) CostModel::eval(bbox);

            ctx.exp_leaves_visited += value;
            ctx.exp_primitives_queried += value * double(prim_count);
            if (prim_count < sizeof(ctx.prim_buckets) / sizeof(Size))
                ctx.prim_buckets[prim_count]++;
            if (prim_count > ctx.max_prims_in_leaf)
                ctx.max_prims_in_leaf = prim_count;
            if (prim_count > 0)
                ctx.nonempty_leaf_count++;
        } else {
            ctx.exp_traversal_steps += (double) CostModel::eval(bbox);

            Index axis = node->axis();
            Scalar split = Scalar(node->split());
            BoundingBox left_bbox(bbox), right_bbox(bbox);
            left_bbox.max[axis] = split;
            right_bbox.min[axis] = split;
            compute_statistics(ctx, node->left(), left_bbox, depth + 1);
            compute_statistics(ctx, node->right(), right_bbox, depth + 1);
        }
    }

    void build() {
        /* Some sanity checks */
        if (ready())
            Throw("The kd-tree has already been built!");
        if (m_min_max_bins <= 1)
            Throw("The number of min-max bins must be > 2");
        if (m_stop_primitives <= 0)
            Throw("The stopping primitive count must be greater than zero");
        if (m_exact_prim_threshold <= m_stop_primitives)
            Throw("The exact primitive threshold must be bigger than the "
                  "stopping primitive count");

        Size prim_count = derived().primitive_count();
        if (m_max_depth == 0)
            m_max_depth = (int) (8 + 1.3f * log2i(prim_count));
        m_max_depth = std::min(m_max_depth, (Size) MTS_KD_MAXDEPTH);

        Log(m_log_level, "kd-tree configuration:");
        Log(m_log_level, "   Cost model               : %s",
            string::indent(m_cost_model, 30));
        Log(m_log_level, "   Max. tree depth          : %i", m_max_depth);
        Log(m_log_level, "   Scene bounding box (min) : %s", m_bbox.min);
        Log(m_log_level, "   Scene bounding box (max) : %s", m_bbox.max);
        Log(m_log_level, "   Min-max bins             : %i", m_min_max_bins);
        Log(m_log_level, "   O(n log n) method        : use for <= %i primitives",
            m_exact_prim_threshold);
        Log(m_log_level, "   Stopping primitive count : %i", m_stop_primitives);
        Log(m_log_level, "   Perfect splits           : %s",
            m_clip_primitives ? "yes" : "no");
        Log(m_log_level, "   Retract bad splits       : %s",
            m_retract_bad_splits ? "yes" : "no");
        Log(m_log_level, "");

        /* ==================================================================== */
        /*              Create build context and preallocate memory             */
        /* ==================================================================== */

        BuildContext ctx(derived());

        ctx.node_storage.reserve(prim_count);
        ctx.index_storage.reserve(prim_count);

        ctx.node_storage.grow_by(1);

        /* ==================================================================== */
        /*                      Build the tree in parallel                      */
        /* ==================================================================== */

        Scalar final_cost = 0;
        if (prim_count == 0) {
            Log(EWarn, "kd-tree contains no geometry!");
            ctx.node_storage[0].set_leaf_node(0, 0);
        } else {

            Log(m_log_level, "Creating a preliminary index list (%s)",
                util::mem_string(prim_count * sizeof(Index)).c_str());

            IndexVector indices(prim_count);
            for (size_t i = 0; i < prim_count; ++i)
                indices[i] = (Index) i;

            BuildTask &task = *new (tbb::task::allocate_root()) BuildTask(
                ctx, ctx.node_storage.begin(), std::move(indices),
                m_bbox, m_bbox, 0, 0, &final_cost);

            tbb::task::spawn_root_and_wait(task);
        }

        Log(m_log_level, "Structural kd-tree statistics:");

        /* ==================================================================== */
        /*     Store the node and index lists in a compact contiguous format    */
        /* ==================================================================== */

        m_node_count = Size(ctx.node_storage.size());
        m_index_count = Size(ctx.index_storage.size());

        m_indices.reset(enoki::alloc<Index>(m_index_count));
        tbb::parallel_for(
            tbb::blocked_range<Size>(0u, m_index_count, MTS_KD_GRAIN_SIZE),
            [&](const tbb::blocked_range<Size> &range) {
                for (Size i = range.begin(); i != range.end(); ++i)
                    m_indices[i] = ctx.index_storage[i];
            }
        );

        tbb::concurrent_vector<Index>().swap(ctx.index_storage);

        m_nodes.reset(enoki::alloc<KDNode>(m_node_count));
        tbb::parallel_for(
            tbb::blocked_range<Size>(0u, m_node_count, MTS_KD_GRAIN_SIZE),
            [&](const tbb::blocked_range<Size> &range) {
                for (Size i = range.begin(); i != range.end(); ++i)
                    m_nodes[i] = ctx.node_storage[i];
            }
        );
        tbb::concurrent_vector<KDNode>().swap(ctx.node_storage);

        /* ==================================================================== */
        /*         Print various tree statistics if requested by the user       */
        /* ==================================================================== */

        if (Thread::thread()->logger()->log_level() <= m_log_level) {
            compute_statistics(ctx, m_nodes.get(), m_bbox, 0);

            // Trigger per-thread data release
            ctx.local.clear();

            ctx.exp_traversal_steps /= (double) CostModel::eval(m_bbox);
            ctx.exp_leaves_visited /= (double) CostModel::eval(m_bbox);
            ctx.exp_primitives_queried /= (double) CostModel::eval(m_bbox);
            ctx.temp_storage += ctx.node_storage.size() * sizeof(KDNode);
            ctx.temp_storage += ctx.index_storage.size() * sizeof(Index);

            Log(m_log_level, "   Primitive references        : %i (%s)",
                m_index_count, util::mem_string(m_index_count * sizeof(Index)));

            Log(m_log_level, "   kd-tree nodes               : %i (%s)",
                m_node_count, util::mem_string(m_node_count * sizeof(KDNode)));

            Log(m_log_level, "   kd-tree depth               : %i",
                ctx.max_depth);

            Log(m_log_level, "   Temporary storage used      : %s",
                util::mem_string(ctx.temp_storage));

            Log(m_log_level, "   Parallel work units         : %i",
                ctx.work_units);

            std::ostringstream oss;
            Size prim_bucket_count = sizeof(ctx.prim_buckets) / sizeof(Size);
            oss << "   Leaf node histogram         : ";
            for (Size i = 0; i < prim_bucket_count; i++) {
                oss << i << "(" << ctx.prim_buckets[i] << ") ";
                if ((i + 1) % 4 == 0 && i + 1 < prim_bucket_count) {
                    Log(m_log_level, "%s", oss.str());
                    oss.str("");
                    oss << "                                 ";
                }
            }
            Log(m_log_level, "%s", oss.str().c_str());
            Log(m_log_level, "");

            Log(m_log_level, "Qualitative kd-tree statistics:");
            Log(m_log_level, "   Retracted splits            : %i",
                ctx.retracted_splits);
            Log(m_log_level, "   Bad refines                 : %i",
                ctx.bad_refines);
            Log(m_log_level, "   Pruned                      : %i",
                ctx.pruned);
            Log(m_log_level, "   Largest leaf node           : %i primitives",
                ctx.max_prims_in_leaf);
            Log(m_log_level, "   Avg. prims/nonempty leaf    : %.2f",
                m_index_count / (Scalar) ctx.nonempty_leaf_count);
            Log(m_log_level, "   Expected traversals/query   : %.2f",
                ctx.exp_traversal_steps);
            Log(m_log_level, "   Expected leaf visits/query  : %.2f",
                ctx.exp_leaves_visited);
            Log(m_log_level, "   Expected prim. visits/query : %.2f",
                ctx.exp_primitives_queried);
            Log(m_log_level, "   Final cost                  : %.2f",
                final_cost);
            Log(m_log_level, "");
        }

        #if defined(__LINUX__)
            /* Forcefully release Heap memory back to the OS */
            malloc_trim(0);
        #endif
    }

protected:
    std::unique_ptr<KDNode[], enoki::aligned_deleter> m_nodes;
    std::unique_ptr<Index[],  enoki::aligned_deleter> m_indices;
    Size m_node_count = 0;
    Size m_index_count = 0;

    CostModel m_cost_model;
    bool m_clip_primitives = true;
    bool m_retract_bad_splits = true;
    Size m_max_depth = 0;
    Size m_stop_primitives = 3;
    Size m_max_bad_refines = 0;
    Size m_exact_prim_threshold = 65536;
    Size m_min_max_bins = 128;
    ELogLevel m_log_level = EDebug;
    BoundingBox m_bbox;
};

template <typename BoundingBox, typename Index, typename CostModel, typename Derived>
Class * TShapeKDTree<BoundingBox, Index, CostModel, Derived>::m_class =
    new Class("TShapeKDTree", "Object", true, nullptr, nullptr);

template <typename BoundingBox, typename Index, typename CostModel, typename Derived>
const Class *TShapeKDTree<BoundingBox, Index, CostModel, Derived>::class_() const {
    return m_class;
}

class SurfaceAreaHeuristic3f {
public:
    using Size = uint32_t;
    using Index = uint32_t;

    SurfaceAreaHeuristic3f(Float query_cost, Float traversal_cost,
                           Float empty_space_bonus)
        : m_query_cost(query_cost), m_traversal_cost(traversal_cost),
          m_empty_space_bonus(empty_space_bonus) {
        if (m_query_cost <= 0)
            Throw("The query cost must be > 0");
        if (m_traversal_cost <= 0)
            Throw("The traveral cost must be > 0");
        if (m_empty_space_bonus <= 0 || m_empty_space_bonus > 1)
            Throw("The empty space bonus must be in [0, 1]");
    }

    /**
     * \brief Return the query cost used by the tree construction heuristic
     *
     * (This is the average cost for testing a shape against a kd-tree query)
     */
    Float query_cost() const { return m_query_cost; }

    /// Get the cost of a traversal operation used by the tree construction heuristic
    Float traversal_cost() const { return m_traversal_cost; }

    /**
     * \brief Return the bonus factor for empty space used by the
     * tree construction heuristic
     */
    Float empty_space_bonus() const { return m_empty_space_bonus; }

    /**
     * \brief Initialize the surface area heuristic with the bounds of
     * a parent node
     *
     * Precomputes some information so that traversal probabilities
     * of potential split planes can be evaluated efficiently
     */
    void set_bounding_box(const BoundingBox3f &bbox) {
        auto extents = bbox.extents();
        Float temp = 2.f / bbox.surface_area();
        auto a = shuffle<1, 2, 0>(extents);
        auto b = shuffle<2, 0, 1>(extents);
        m_temp0 = m_temp1 = (a * b) * temp;
        m_temp2 = (a + b) * temp;
        m_temp0 -= m_temp2 * Vector3f(bbox.min);
        m_temp1 += m_temp2 * Vector3f(bbox.max);
    }

    /// \brief Evaluate the cost of a leaf node
    Float leaf_cost(Size nelem) const {
        return m_query_cost * nelem;
    }

    /**
     * \brief Evaluate the surface area heuristic
     *
     * Given a split on axis \a axis at position \a split, compute the
     * probability of traversing the left and right child during a typical
     * query operation. In the case of the surface area heuristic, this is
     * simply the ratio of surface areas.
     */
    Float inner_cost(Index axis, Float split, Float left_cost, Float right_cost) const {
        Float left_prob  = m_temp0[axis] + m_temp2[axis] * split;
        Float right_prob = m_temp1[axis] - m_temp2[axis] * split;

        Float cost = m_traversal_cost +
            (left_prob * left_cost + right_prob * right_cost);

        if (unlikely(left_cost == 0 || right_cost == 0))
            cost *= m_empty_space_bonus;

        return cost;
    }

    static Float eval(const BoundingBox3f &bbox) {
        return bbox.surface_area();

    }

    friend std::ostream &operator<<(std::ostream &os, const SurfaceAreaHeuristic3f &sa) {
        os << "SurfaceAreaHeuristic3f[" << std::endl
           << "  query_cost = " << sa.query_cost() << "," << std::endl
           << "  traversal_cost = " << sa.traversal_cost() << "," << std::endl
           << "  empty_space_bonus = " << sa.empty_space_bonus() << std::endl
           << "]";
        return os;
    }
private:
    Vector3f m_temp0, m_temp1, m_temp2;
    Float m_query_cost;
    Float m_traversal_cost;
    Float m_empty_space_bonus;
};

class MTS_EXPORT_RENDER ShapeKDTree
    : public TShapeKDTree<BoundingBox3f, uint32_t, SurfaceAreaHeuristic3f, ShapeKDTree> {
public:
    using Base = TShapeKDTree<BoundingBox3f, uint32_t, SurfaceAreaHeuristic3f, ShapeKDTree>;
    using Base::Scalar;

    ShapeKDTree(const Properties &props);
    void add_shape(Shape *shape);

    Size primitive_count() const { return m_primitive_map.back(); }
    Size shape_count() const { return Size(m_shapes.size()); }

    void build() {
        Timer timer;
        Log(EInfo, "Building a SAH kd-tree (%i primitives) ..",
            primitive_count());

        Base::build();

        Log(EInfo, "Finished. (%s of storage, took %s)",
            util::mem_string(m_index_count * sizeof(Index) +
                            m_node_count * sizeof(KDNode)),
            util::time_string(timer.value())
        );
    }

    /// Return the i-th shape
    const Shape *shape(size_t i) const { Assert(i < m_shapes.size()); return m_shapes[i]; }

    /// Return the i-th shape
    Shape *shape(size_t i) { Assert(i < m_shapes.size()); return m_shapes[i]; }

    using Base::bbox;

    /// Return the bounding box of the i-th primitive
    BoundingBox3f bbox(Index i) const {
        Index shape_index = find_shape(i);
        return m_shapes[shape_index]->bbox(i);
    }

    /// Return the (clipped) bounding box of the i-th primitive
    BoundingBox3f bbox(Index i, const BoundingBox3f &clip) const {
        Index shape_index = find_shape(i);
        return m_shapes[shape_index]->bbox(i, clip);
    }

    /**
    * \brief Ray tracing kd-tree traversal loop (Havran variant)
    *
    * This is generally the most robust and fastest traversal routine
    * of the methods implemented in this class. However, this method is only
    * implemented for scalar rays.
    */
    template<bool IsShadowRay = false>
    std::pair<bool, Float> ray_intersect_havran(const Ray3f &ray, Float mint, Float maxt) const {
        /// Ray traversal stack entry
        struct KDStackEntry {
            // Pointer to the far child
            const KDNode *node;
            // Distance traveled along the ray (entry or exit)
            Float t;
            // Previous stack item
            uint32_t prev;
            // Associated point
            Point3f p;
        };

        // Allocate the node stack
        KDStackEntry stack[MTS_KD_MAXDEPTH];

        // True if an intersection has been found along the ray
        bool its_found(false);

        // Set up the entry point
        uint32_t en_pt = 0;
        stack[en_pt].t = mint;
        stack[en_pt].p = ray(mint);

        // Set up the exit point
        uint32_t ex_pt = 1;
        stack[ex_pt].t = maxt;
        stack[ex_pt].p = ray(maxt);
        stack[ex_pt].node = nullptr;

        const KDNode *current_node = m_nodes.get();

        while (current_node != nullptr) {
            while (!current_node->leaf()) {
                const Float split_val = current_node->split();
                const uint32_t axis   = current_node->axis();
                const KDNode *far_child;

                bool entry_before_sp = stack[en_pt].p[axis] <= split_val;
                bool exit_before_sp  = stack[ex_pt].p[axis] <= split_val;
                bool entry_on_sp     = stack[en_pt].p[axis] == split_val;
                bool exit_after_sp   = stack[ex_pt].p[axis] >  split_val;

                // N4 and P4 cases from Havran's thesis
                bool n4 = entry_before_sp  && !exit_before_sp && !entry_on_sp;
                bool p4 = !entry_before_sp && !exit_after_sp;

                bool explore_both = n4 || p4;

                bool left_first  = (entry_before_sp && exit_before_sp) || n4;
                bool right_first = !left_first;

                if (likely(!explore_both)) {
                    int visit_right = right_first ? 1 : 0;
                    current_node = current_node->left() + visit_right;
                    continue;
                }

                // At this point, we will visit both nodes.
                int far_child_offset = left_first ? 1 : 0;

                far_child    = current_node->left() + far_child_offset;
                current_node = current_node->left() + (1 - far_child_offset);

                // Calculate the distance to the split plane
                Float dist_to_split = (split_val - ray.o[axis]) * ray.d_rcp[axis];

                // Set up a new exit point
                const uint32_t tmp = ex_pt++;
                if (ex_pt == en_pt) /* Do not overwrite the entry point */
                    ++ex_pt;

                stack[ex_pt].prev    = tmp;
                stack[ex_pt].t       = dist_to_split;
                stack[ex_pt].node    = far_child;
                stack[ex_pt].p       = ray(dist_to_split);
                stack[ex_pt].p[axis] = split_val;
            }

            // Arrived at a leaf node
            const Index prim_start = current_node->primitive_offset();
            const Index prim_end = prim_start + current_node->primitive_count();
            for (Index i = prim_start; i < prim_end; i++) {
                Index prim_index   = m_indices[i];
                Index shape_index  = find_shape(prim_index);
                const Shape *shape = this->shape(shape_index);
                const Mesh *mesh   = dynamic_cast<const Mesh *>(shape);

                auto result = mesh->ray_intersect(prim_index, ray);

                bool its_found_result  = std::get<0>(result);
                Scalar t_result        = std::get<3>(result);

                if (its_found_result) {
                    if (IsShadowRay)
                        return std::make_pair(true, t_result);
                    maxt = min(t_result, maxt);
                    its_found = true;
                }
            }

            if (stack[ex_pt].t > maxt)
                break;

            // Pop from the stack and advance to the next node on the interval
            en_pt = ex_pt;
            current_node = stack[ex_pt].node;
            ex_pt = stack[en_pt].prev;
        }
        return std::make_pair(its_found, maxt);
    }

    /**
    * \brief Scalar implementation of the ray tracing kd-tree traversal loop (PBRT variant).
    */
    template<bool IsShadowRay = false,
        typename Ray,
        typename Point = typename Ray::Point,
        typename Scalar = value_t<Point>,
        enable_if_not_array_t<Scalar> = 0>
        std::pair<bool, Scalar> ray_intersect_pbrt(const Ray &ray, Scalar mint_, Scalar maxt_) const {

        /// Ray traversal stack entry
        struct KDStackEntry {
            // Distance traveled along the ray to the entry and exit of this node
            Float mint, maxt;
            // Pointer to the far child
            const KDNode *node;
        };

        // Allocate the node stack
        KDStackEntry stack[MTS_KD_MAXDEPTH];
        int32_t stack_index = 0;

        // True if the earliest intersection has been found
        bool its_found(false);

        const KDNode *current_node = m_nodes.get();
        Scalar mint = Scalar(mint_), maxt = Scalar(maxt_);

        if (!(maxt_ >= mint))
            return std::make_pair(its_found, maxt_);

        while (current_node != nullptr) {
            if (mint > maxt_)
                break;

            if (likely(!current_node->leaf())) {
                const Float split   = current_node->split();
                const uint32_t axis = current_node->axis();

                // Compute parametric distance along the rays to the split plane
                Scalar t_plane = (split - ray.o[axis]) * ray.d_rcp[axis];

                bool left_first  = (ray.o[axis] < split) || (ray.o[axis] == split && ray.d[axis] <= 0.f);
                bool right_first = !left_first;

                bool start_after = t_plane < mint;
                bool end_before  = t_plane > maxt || !(t_plane > 0.f); // Also handles t_plane == NaN

                bool visit_single_node = (start_after || end_before);

                // if we only need to visit one node, just pick the correct one and continue
                if (likely(visit_single_node)) {
                    int visit_left = ((end_before && left_first) || (!end_before && start_after && right_first)) ? 0 : 1;
                    current_node   = current_node->left() + visit_left;
                    continue;
                }

                // At this point, we will visit both nodes.
                int second_node_offset = left_first ? 1 : 0;
                const KDNode *second_node = current_node->left() + second_node_offset;
                current_node = current_node->left() + (1 - second_node_offset);

                KDStackEntry& entry = stack[stack_index];
                entry.node = second_node;
                entry.mint = t_plane;
                entry.maxt = maxt;
                maxt = t_plane;

                ++stack_index;
            } else { // Arrived at a leaf node
                const Index prim_start = current_node->primitive_offset();
                const Index prim_end = prim_start + current_node->primitive_count();
                for (Index i = prim_start; i < prim_end; i++) {
                    Index prim_index   = m_indices[i];
                    Index shape_index  = find_shape(prim_index);
                    const Shape *shape = this->shape(shape_index);
                    const Mesh *mesh   = dynamic_cast<const Mesh *>(shape);

                    auto result = mesh->ray_intersect(prim_index, ray);

                    bool its_found_result  = std::get<0>(result);
                    Scalar t_result        = std::get<3>(result);

                    if (its_found_result) {
                        if (IsShadowRay)
                            return std::make_pair(true, t_result);
                        maxt_ = min(t_result, maxt_);
                        its_found = true;
                    }
                }

                if (stack_index > 0) {
                    --stack_index;
                    KDStackEntry& entry = stack[stack_index];
                    current_node = entry.node;
                    mint = entry.mint;
                    maxt = entry.maxt;
                } else {
                    break;
                }
            }
        }
        return std::make_pair(its_found, maxt_);
    }

    /**
    * \brief Vectorized implementation of the ray tracing kd-tree traversal loop (PBRT variant).
    */
    template<bool IsShadowRay = false,
        typename Ray,
        typename Point = typename Ray::Point,
        typename Scalar = value_t<Point>,
        typename Mask = mask_t<Scalar>,
        enable_if_static_array_t<Scalar> = 0>
        std::pair<Mask, Scalar> ray_intersect_pbrt(const Ray &ray, const Scalar &mint_, Scalar maxt_) const {
        /// Ray traversal stack entry
        struct KDStackEntry {
            // Distance traveled along the rays to the entry and exit of this node
            Scalar mint, maxt;
            // Pointer to the far child
            const KDNode *node;
            // Track rays that should not be traversing the node.
            Mask currently_inactive;
        };

        // Allocate the node stack
        KDStackEntry stack[MTS_KD_MAXDEPTH];
        uint32_t stack_index = 0;

        // True if the earliest intersection has been found
        Mask its_found(false);

        // True is a ray is inactive
        Mask inactive(false);

        // Track rays that should not be traversing the current node. "inactive" is always a subset of currently_inactive.
        Mask currently_inactive(false);

        // Records the intersection time for the earliest intersection along the ray
        Scalar t(std::numeric_limits<scalar_t<Scalar>>::infinity());

        const KDNode *current_node = m_nodes.get();
        Scalar mint = Scalar(mint_), maxt = Scalar(maxt_);
        inactive |= !(maxt_ >= mint);
        currently_inactive |= inactive;

        while (current_node != nullptr && !all(inactive)) {
            if (likely(!current_node->leaf())) {
                const Float split   = current_node->split();
                const uint32_t axis = current_node->axis();

                // Compute parametric distance along the rays to the split plane
                Scalar t_plane = (split - ray.o[axis]) * ray.d_rcp[axis];

                Mask left_of_split  = (ray.o[axis] < split) | (eq(ray.o[axis], split) & ray.d[axis] <= 0.f);
                Mask right_of_split = !left_of_split;

                Mask start_after = t_plane < mint;
                Mask end_before  = t_plane > maxt | !(t_plane > 0.f); // Also handles t_plane == NaN

                // Track if we are in N4/P4 case
                Mask need_visit_both = !start_after & !end_before;

                // For any cases, this mask tracks if the ray needs to traverse the left node first.
                Mask left_first = (need_visit_both & left_of_split) |
                                  (!need_visit_both & ((end_before & left_of_split) | (!end_before & start_after & right_of_split)));

                Mask right_first = !left_first | currently_inactive;
                left_first |= currently_inactive;

                bool all_left_first  = all(left_first);
                bool all_right_first = all(right_first);
                bool none_visit_both = all(!need_visit_both | currently_inactive);

                bool visit_single_node = none_visit_both && (all_left_first || all_right_first);

                // if we only need to visit one node, just pick the correct one and continue
                if (likely(visit_single_node)) {
                    int visit_left = all_left_first ? 0 : 1;
                    current_node   = current_node->left() + visit_left;
                    continue;
                }

                // Vote to choose which node to traverse first
                bool visit_left_first = count(left_first) >= count(right_first);

                int second_node_offset = visit_left_first ? 1 : 0;

                const KDNode *second_node = current_node->left() + second_node_offset;
                current_node = current_node->left() + (1 - second_node_offset);

                // Records which ray will traverse the nodes in the right order
                Mask correct_order = left_first ^ Mask(!visit_left_first);

                KDStackEntry& entry = stack[stack_index++];
                entry.node = second_node;
                entry.mint = select(correct_order & need_visit_both, t_plane, mint);
                entry.maxt = select(!correct_order & need_visit_both, t_plane, maxt);
                // Record nodes which don't want to traverse the second_node
                entry.currently_inactive = (currently_inactive | (correct_order & !need_visit_both));

                maxt = select(correct_order & need_visit_both, t_plane, maxt);

                // Record nodes that didn't want to traverse the current_node
                currently_inactive |= (!correct_order & !need_visit_both);
            } else { // Arrived at a leaf node
                const Index prim_start = current_node->primitive_offset();
                const Index prim_end = prim_start + current_node->primitive_count();
                for (Index i = prim_start; i < prim_end; i++) {
                    Index prim_index   = m_indices[i];
                    Index shape_index  = find_shape(prim_index);
                    const Shape *shape = this->shape(shape_index);
                    const Mesh *mesh   = dynamic_cast<const Mesh *>(shape);

                    auto result = mesh->ray_intersect(prim_index, ray);

                    Mask its_found_result  = std::get<0>(result);
                    Scalar t_result        = std::get<3>(result);

                    its_found |= its_found_result;
                    t     = select(its_found_result, min(t, t_result), t);
                    maxt_ = select(its_found_result, min(maxt_, t), maxt_);

                    if (IsShadowRay && (all(its_found | inactive)))
                        return std::make_pair(its_found, t);
                }

                // Ensure that there is still active rays that want to visit the poped node
                do {
                    if (stack_index > 0) {
                        --stack_index;
                        KDStackEntry& entry = stack[stack_index];
                        current_node = entry.node;
                        mint = entry.mint;
                        maxt = entry.maxt;
                        currently_inactive = entry.currently_inactive;
                        inactive |= (mint > maxt_) & !currently_inactive;
                        currently_inactive |= inactive;
                    } else {
                        return std::make_pair(its_found, t);
                    }
                } while (all(currently_inactive));
            }
        }
        return std::make_pair(its_found, t);
    }

    /// Intersection routine which doesn't rely on the kd-tree. Should only be used for debug/test purpuses
    template<bool IsShadowRay = false,
             typename Ray,
             typename Point = typename Ray::Point,
             typename Scalar = value_t<Point>,
             typename Mask = mask_t<Scalar>>
    std::pair<Mask, Scalar> ray_intersect_dummy(const Ray &ray, const Scalar &mint_, Scalar maxt_) const {
        Mask its_found(false);
        Scalar t(maxt_);

        for (size_t s = 0; s < m_shapes.size(); s++) {
            const Shape *shape = this->shape(s);
            if (shape->class_()->derives_from(MTS_CLASS(Mesh))) {
                const Mesh *mesh = dynamic_cast<const Mesh *>(shape);

                for (size_t i = 0; i < mesh->face_count(); i++) {
                    auto result = mesh->ray_intersect(i, ray);

                    Mask its_found_result  = std::get<0>(result);
                    Scalar its_time_result = std::get<3>(result);

                    its_found_result &= (its_time_result > mint_ & its_time_result < maxt_);

                    its_found |= its_found_result;
                    t = select(its_found_result, min(t, its_time_result), t);

                    if (IsShadowRay) {
                        if (all(its_found))
                            return std::make_pair(its_found, t);
                    }
                }
            }
        }
        return std::make_pair(its_found, t);
    }

    /// Return a human-readable string representation of the scene contents.
    virtual std::string to_string() const override;

    MTS_DECLARE_CLASS()
protected:
    /**
     * \brief Map an abstract \ref TShapeKDTree primitive index to a specific
     * shape managed by the \ref ShapeKDTree.
     *
     * The function returns the shape index and updates the \a idx parameter to
     * point to the primitive index (e.g. triangle ID) within the shape.
     */
    Index find_shape(Index &i) const {
        Assert(i < primitive_count());

        if (i >= primitive_count()) std::cout << "Assert(i < primitive_count()) == false :" << i << std::endl;

        Index shape_index = (Index) math::find_interval(
            Size(0),
            Size(m_primitive_map.size()),
            [&](Index k) {
                return m_primitive_map[k] <= i;
            }
        );

        Assert(shape_index < shape_count() &&
               m_primitive_map.size() == shape_count() + 1);

        Assert(i >= m_primitive_map[shape_index]);
        Assert(i <  m_primitive_map[shape_index + 1]);
        i -= m_primitive_map[shape_index];

        return shape_index;
    }

protected:
    std::vector<ref<Shape>> m_shapes;
    std::vector<Size> m_primitive_map;
};

NAMESPACE_END(mitsuba)
