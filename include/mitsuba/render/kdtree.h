#pragma once

#include <unordered_set>
#include <atomic>

#include <nanothread/nanothread.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/mesh.h>

/// Compile-time KD-tree depth limit to enable traversal with stack memory
#define MI_KD_MAXDEPTH 48u

/// OrderedChunkAllocator: don't create chunks smaller than 5MiB
#define MI_KD_MIN_ALLOC 5*1024u*1024u

/// Grain size for parallelization
#define MI_KD_GRAIN_SIZE 10240u

/**
 * Temporary scratch space that is used to cache intersection information
 * (# of floats)
 */
#define MI_KD_INTERSECTION_CACHE_SIZE 6

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(detail)
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
    OrderedChunkAllocator(size_t min_allocation = MI_KD_MIN_ALLOC)
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
    template <typename T> DRJIT_MALLOC T * allocate(size_t size) {
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

        bool contains(uint8_t *ptr) const {
            return ptr >= start.get() && ptr < start.get() + size;
        }

        friend std::ostream& operator<<(std::ostream &os, const Chunk &ch) {
            os << (const void *) ch.start.get() << "-" << (const void *) (ch.start.get() + ch.size)
            << " (size = " << ch.size << ", remainder = " << ch.remainder() << ")";
            return os;
        }
    };

    size_t m_min_allocation;
    std::vector<Chunk> m_chunks;
};

/* Append-only concurrent vector, whose storage is arranged into slices
   with increasing powers of two. Max 2^32 entries supported. */
template <typename Value>
struct ConcurrentVector {
    ConcurrentVector() : m_size_and_capacity(0) { }
    ~ConcurrentVector() { release(); }

    void reserve(uint32_t size) {
        if (size == 0)
            return;

        uint32_t slices     = log2i(size) + 1,
                 slice_size = 1,
                 capacity   = 0;

        bool changed = false;
        for (uint32_t i = 0; i < slices; ++i) {
            Value *cur = m_slices[i].load(std::memory_order_acquire);
            if (!cur) {
                Value *val = new Value[slice_size];
                if (m_slices[i].compare_exchange_strong(cur, val))
                    changed = true;
                else
                    delete[] val;
            }
            capacity += slice_size;
            slice_size *= 2;
        }

        if (changed) {
            uint64_t size_and_capacity =
                     m_size_and_capacity.load(std::memory_order_acquire);

            while (true) {
                uint32_t cur_size     = (uint32_t) size_and_capacity,
                         cur_capacity = (uint32_t) (size_and_capacity >> 32);

                if (cur_capacity >= capacity)
                    break;

                uint64_t size_and_capacity_new =
                    (uint64_t) cur_size + (((uint64_t) capacity) << 32);

                if (m_size_and_capacity.compare_exchange_weak(
                        size_and_capacity, size_and_capacity_new,
                        std::memory_order_release, std::memory_order_relaxed))
                    break;
            }
        }
    }

    Value &operator[](uint32_t index) {
        index += 1;
        uint32_t slice  = log2i((uint32_t) index),
                 offset = 1u << slice;
        return m_slices[slice][index - offset];
    }

    const Value &operator[](uint32_t index) const {
        index += 1;
        uint32_t slice  = log2i(index),
                 offset = 1u << slice;
        return m_slices[slice][index - offset];
    }

    uint32_t grow_by(uint32_t amount) {
        uint64_t size_and_capacity =
            m_size_and_capacity.load(std::memory_order_acquire);

        while (true) {
            uint32_t size     = (uint32_t) size_and_capacity,
                     capacity = (uint32_t) (size_and_capacity >> 32),
                     new_size = size + amount;

            if (new_size > capacity) {
                reserve(new_size);
                size_and_capacity =
                    m_size_and_capacity.load(std::memory_order_acquire);
                continue;
            }

            uint64_t size_and_capacity_new =
                (uint64_t) new_size + (((uint64_t) capacity) << 32);

            if (m_size_and_capacity.compare_exchange_weak(
                    size_and_capacity, size_and_capacity_new,
                    std::memory_order_release, std::memory_order_relaxed))
                return size;
        }
    }

    uint32_t log2i(uint32_t x) {
        #if defined(_MSC_VER)
            unsigned long y;
            _BitScanReverse(&y, (unsigned long)x);
            return (uint32_t) y;
        #else
            return 31u - __builtin_clz(x);
        #endif
    }

    uint64_t size() const {
        return m_size_and_capacity.load(std::memory_order_acquire);
    }

    void release() {
        for (int i = 0; i < 32; ++i) {
            if (m_slices[i].load()) {
                delete[] m_slices[i].load();
                m_slices[i].store(nullptr);
            }
        }
    }

private:
    std::atomic<uint64_t> m_size_and_capacity;
    std::atomic<Value *> m_slices[32] { };
};
NAMESPACE_END(detail)


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
 * kd-tree query operation. See \ref SurfaceAreaHeuristic3 for an example of
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
    using Scalar      = dr::value_t<Vector>;
    using SizedInt    = dr::uint_array_t<Scalar>;
    using IndexVector = std::vector<Index>;

    static constexpr size_t Dimension     = Vector::Size;
    static constexpr int MantissaBits     = sizeof(Scalar) == 4 ? 23: 52;
    static constexpr int ExponentSignBits = sizeof(Scalar) == 4 ? 9 : 12;

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
    LogLevel log_level() const { return m_log_level; }

    /// Return the log level of kd-tree status messages
    void set_log_level(LogLevel level) { m_log_level = level; }

    bool ready() const { return (bool) m_nodes; }

    /// Return the bounding box of the entire kd-tree
    const BoundingBox bbox() const { return m_bbox; }

    const Derived& derived() const { return (Derived&) *this; }
    Derived& derived() { return (Derived&) *this; }

    MI_DECLARE_CLASS()
protected:
    /* ==================================================================== */
    /*                  Essential internal data structures                  */
    /* ==================================================================== */

#if defined(_MSC_VER)
#  pragma pack(push)
#  pragma pack(1)
#elif defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wbitfield-width"
#endif

    /// kd-tree node in 8 bytes.
    struct KDNode {
        union {
            /// Inner node
            struct DRJIT_PACK {
                /// Split plane coordinate
                Scalar split;

                /// X/Y/Z axis specifier
                unsigned int axis : 2;

                /// Offset to left child (max 1B nodes in between)
                unsigned int left_offset : sizeof(Index) * 8 - 2;
            } inner;

            /// Leaf node
            struct DRJIT_PACK {
                #if defined(LITTLE_ENDIAN)
                    /// How many primitives does this leaf reference?
                    SizedInt prim_count : MantissaBits;

                    /// Mask bits (all 1s for leaf nodes)
                    SizedInt mask : ExponentSignBits;
                #else /* Swap for big endian machines */
                    /// Mask bits (all 1s for leaf nodes)
                    SizedInt mask : ExponentSignBits;

                    /// How many primitives does this leaf reference?
                    SizedInt prim_count : MantissaBits;
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
            if constexpr (sizeof(Scalar) == 4)
                data.leaf.mask = 0b111111111u;
            else
                data.leaf.mask = 0b111111111111u;
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
            return (size_t) data.inner.left_offset == left_offset &&
                   (Index) data.inner.axis == axis;
        }

        /// Is this a leaf node?
        bool leaf() const {
            if constexpr (sizeof(Scalar) == 4)
                return data.leaf.mask == 0b111111111u;
            else
                return data.leaf.mask == 0b111111111111u;
        }

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

#if defined(_MSC_VER)
#  pragma pack(pop)
#elif defined(__clang__)
#  pragma clang diagnostic pop
#endif

    static_assert(sizeof(KDNode) == sizeof(Size) + sizeof(Scalar),
                  "kd-tree node has unexpected size. Padding issue?");

protected:
    /// Enumeration representing the state of a classified primitive in the O(N log N) builder
    enum class PrimClassification : uint8_t {
        Ignore = 0, /// Primitive was handled already, ignore from now on
        Left   = 1, /// Primitive is left of the split plane
        Right  = 2, /// Primitive is right of the split plane
        Both   = 3  /// Primitive straddles the split plane
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

        void set(Index index, PrimClassification value) {
            Assert(index < m_count);
            uint8_t *ptr = m_buffer.get() + (index >> 2);
            uint8_t shift = (index & 3) << 1;
            *ptr = (*ptr & ~(3 << shift)) | ((uint8_t) value << shift);
        }

        PrimClassification get(Index index) const {
            Assert(index < m_count);
            uint8_t *ptr = m_buffer.get() + (index >> 2);
            uint8_t shift = (index & 3) << 1;
            return PrimClassification((*ptr >> shift) & 3);
        }

        /// Return the size (in bytes)
        size_t size() const { return (m_count + 3) / 4; }

    private:
        std::unique_ptr<uint8_t[]> m_buffer;
        Size m_count = 0;
    };

    /* ==================================================================== */
    /*                    Build-related data structures                     */
    /* ==================================================================== */

    struct BuildContext;

    /// Helper data structure used during tree construction (used by a single thread)
    struct LocalBuildContext {
        ClassificationStorage classification_storage;
        detail::OrderedChunkAllocator left_alloc;
        detail::OrderedChunkAllocator right_alloc;
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
        ThreadEnvironment env;
        detail::ConcurrentVector<KDNode> node_storage;
        detail::ConcurrentVector<Index> index_storage;
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

        BuildContext(const Derived &derived) : derived(derived) { }
    };

    /// Data type for split candidates suggested by the tree cost model
    struct SplitCandidate {
        Scalar cost = dr::Infinity<Scalar>;
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
        enum class Type : uint16_t {
            EdgeEnd = 0,
            EdgePlanar = 1,
            EdgeStart = 2
        };

        /// Dummy constructor
        EdgeEvent() { }

        /// Create a new edge event
        EdgeEvent(Type type, int axis, Scalar pos, Index index)
         : pos(pos), index(index), type(type), axis((uint16_t) axis) { }

        /// Return a string representation
        friend std::ostream& operator<<(std::ostream &os, const EdgeEvent &e) {
            os << "EdgeEvent[" << std::endl
                << "  pos = " << e.pos << "," << std::endl
                << "  index = " << e.index << "," << std::endl
                << "  type = ";
            switch (e.type) {
                case Type::EdgeEnd: os << "end"; break;
                case Type::EdgeStart: os << "start"; break;
                case Type::EdgePlanar: os << "planar"; break;
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

        void set_invalid() {
            pos   = 0;
            index = 0;
            type  = Type::EdgeEnd;
            axis  = 7;
        }
        bool valid() const { return axis != 7; }

        /// Plane position
        Scalar pos;
        /// Primitive index
        Index index;
        /// Event type: end/planar/start
        Type type;
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
              m_inv_bin_size(1.f / (bbox.extents() / (Scalar) bin_count)),
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

        MI_INLINE void put(BoundingBox bbox) {
            using IndexArray = dr::Array<Index, 3>;
            const IndexArray offset_min = IndexArray(0, 2 * m_bin_count, 4 * m_bin_count);
            const IndexArray offset_max = offset_min + 1;
            Index *ptr = m_bins.data();

            Assert(bbox.valid());
            Vector rel_min = (bbox.min - m_bbox.min) * m_inv_bin_size;
            Vector rel_max = (bbox.max - m_bbox.min) * m_inv_bin_size;

            rel_min = dr::minimum(dr::maximum(rel_min, dr::zeros<Vector>()), m_max_bin);
            rel_max = dr::minimum(dr::maximum(rel_max, dr::zeros<Vector>()), m_max_bin);

            IndexArray index_min = IndexArray(rel_min);
            IndexArray index_max = IndexArray(rel_max);

            Assert(dr::all(index_min <= index_max));
            index_min = index_min + index_min + offset_min;
            index_max = index_max + index_max + offset_max;

            IndexArray minCounts = dr::gather<IndexArray>(ptr, index_min),
                       maxCounts = dr::gather<IndexArray>(ptr, index_max);

            dr::scatter(ptr, minCounts + 1, index_min);
            dr::scatter(ptr, maxCounts + 1, index_max);
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
                    best.split, dr::Infinity<Scalar>)));
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

            std::mutex mutex;
            IndexVector left_indices(split.left_count);
            IndexVector right_indices(split.right_count);
            Size left_count = 0, right_count = 0;
            BoundingBox left_bounds, right_bounds;

            dr::parallel_for(
                dr::blocked_range<Size>(0u, Size(indices.size()), MI_KD_GRAIN_SIZE),
                [&](const dr::blocked_range<Index> &range) {
                    IndexVector left_indices_local, right_indices_local;
                    BoundingBox left_bounds_local, right_bounds_local;

                    Index range_size = Index(range.end()) - Index(range.begin());
                    left_indices_local.reserve(range_size);
                    right_indices_local.reserve(range_size);

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
                    Index *target_left = nullptr, *target_right = nullptr;

                    /* critical section */ {
                        std::lock_guard<std::mutex> lock(mutex);
                        if (!left_indices_local.empty()) {
                            target_left = &left_indices[left_count];
                            left_count += (Index)left_indices_local.size();
                            left_bounds.expand(left_bounds_local);
                        }
                        if (!right_indices_local.empty()) {
                            target_right = &right_indices[right_count];
                            right_count += (Index)right_indices_local.size();
                            right_bounds.expand(right_bounds_local);
                        }
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
     * \brief Build task for building subtrees in parallel
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
    class BuildTask {
    public:
        /// Context with build-specific variables (shared by all threads/tasks)
        BuildContext &m_ctx;

        /// Local context with thread local variables
        static thread_local LocalBuildContext m_local;

        /// Node to be initialized by this task
        Index m_node;

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

        BuildTask(BuildContext &ctx, Index node, IndexVector &&indices,
                  const BoundingBox bbox, const BoundingBox &tight_bbox,
                  Index depth, Size bad_refines, Scalar *cost)
            : m_ctx(ctx), m_node(node), m_indices(std::move(indices)),
              m_bbox(bbox), m_tight_bbox(tight_bbox), m_depth(depth),
              m_bad_refines(bad_refines), m_cost(cost) {
            Assert(m_bbox.contains(tight_bbox));
        }

        /// Run one iteration of min-max binning and spawn recursive tasks
        void execute() {
            ScopedSetThreadEnvironment env(m_ctx.env);
            Size prim_count = Size(m_indices.size());
            const Derived &derived = m_ctx.derived;

            m_ctx.work_units++;

            /* ==================================================================== */
            /*                           Stopping criteria                          */
            /* ==================================================================== */

            if (prim_count <= derived.stop_primitives() ||
                m_depth >= derived.max_depth() || m_tight_bbox.collapsed()) {
                make_leaf(std::move(m_indices));
                return;
            }

            if (prim_count <= derived.exact_primitive_threshold()) {
                *m_cost = transition_to_nlogn();
                return;
            }

            /* ==================================================================== */
            /*                              Binning                                 */
            /* ==================================================================== */

            /* Accumulate all shapes into bins */
            MinMaxBins bins(derived.min_max_bins(), m_tight_bbox);
            std::mutex bins_mutex;
            dr::parallel_for(
                dr::blocked_range<Size>(0u, prim_count, MI_KD_GRAIN_SIZE),
                [&](const dr::blocked_range<Index> &range) {
                    MinMaxBins bins_local(derived.min_max_bins(), m_tight_bbox);
                    for (Index i = range.begin(); i != range.end(); ++i)
                        bins_local.put(derived.bbox(m_indices[i]));
                    std::lock_guard<std::mutex> lock(bins_mutex);
                    bins += bins_local;
                }
            );

            /* ==================================================================== */
            /*                        Split candidate search                        */
            /* ==================================================================== */

            CostModel model(derived.cost_model());
            model.set_bounding_box(m_bbox);
            auto best = bins.best_candidate(prim_count, model);

            Assert(dr::isfinite(best.cost));
            Assert(best.split >= m_bbox.min[best.axis]);
            Assert(best.split <= m_bbox.max[best.axis]);

            /* Allow a few bad refines in sequence before giving up */
            Scalar leaf_cost = model.leaf_cost(prim_count);
            if (best.cost >= leaf_cost) {
                if ((best.cost > 4 * leaf_cost && prim_count < 16)
                    || m_bad_refines >= derived.max_bad_refines()) {
                    make_leaf(std::move(m_indices));
                    return;
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

            Size children = m_ctx.node_storage.grow_by(2);
            Size left_offset = children - m_node;

            if (!m_ctx.node_storage[m_node].set_inner_node(best.axis, best.split, left_offset))
                Throw("Internal error during kd-tree construction: unable to store "
                      "overly large offset to left child node (%i)", left_offset);

            BoundingBox left_bounds(m_bbox), right_bounds(m_bbox);

            left_bounds.max[best.axis] = Scalar(best.split);
            right_bounds.min[best.axis] = Scalar(best.split);

            partition.left_bounds.clip(left_bounds);
            partition.right_bounds.clip(right_bounds);

            Scalar left_cost = 0, right_cost = 0;

            BuildTask left_task = BuildTask(
                m_ctx, children, std::move(partition.left_indices),
                left_bounds, partition.left_bounds, m_depth+1,
                m_bad_refines, &left_cost);

            BuildTask right_task = BuildTask(
                m_ctx, children + 1, std::move(partition.right_indices),
                right_bounds, partition.right_bounds, m_depth + 1,
                m_bad_refines, &right_cost);

            Task *left_dr_task =
                dr::do_async([&]() { left_task.execute(); });
            right_task.execute();
            task_wait_and_release(left_dr_task);

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
        }

        /// Recursively run the O(N log N builder)
        Scalar build_nlogn(Index node, Size prim_count,
                           EdgeEvent *events_start, EdgeEvent *events_end,
                           const BoundingBox &bbox, Size depth,
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
                        case EdgeEvent::Type::EdgeStart:  ++num_start;  break;
                        case EdgeEvent::Type::EdgePlanar: ++num_planar; break;
                        case EdgeEvent::Type::EdgeEnd:    ++num_end;    break;
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
                    || !dr::isfinite(best.cost)) {
                    make_leaf(node, prim_count, events_start, events_end);
                    return leaf_cost;
                }
                ++bad_refines;
                m_ctx.bad_refines++;
            }

            /* ==================================================================== */
            /*                      Primitive Classification                        */
            /* ==================================================================== */

            auto &classification = m_local.classification_storage;

            /* Initially mark all prims as being located on both sides */
            for (auto event = events_by_dimension[best.axis];
                 event != events_by_dimension[best.axis + 1]; ++event)
                classification.set(event->index, PrimClassification::Both);

            Size prims_left = 0, prims_right = 0;
            for (auto event = events_by_dimension[best.axis];
                 event != events_by_dimension[best.axis + 1]; ++event) {

                if (event->type == EdgeEvent::Type::EdgeEnd &&
                    event->pos <= best.split) {
                    /* Fully on the left side (the primitive's interval ends
                       before (or on) the split plane) */
                    Assert(classification.get(event->index) == PrimClassification::Both);
                    classification.set(event->index, PrimClassification::Left);
                    prims_left++;
                } else if (event->type == EdgeEvent::Type::EdgeStart &&
                           event->pos >= best.split) {
                    /* Fully on the right side (the primitive's interval
                       starts after (or on) the split plane) */
                    Assert(classification.get(event->index) == PrimClassification::Both);
                    classification.set(event->index, PrimClassification::Right);
                    prims_right++;
                } else if (event->type == EdgeEvent::Type::EdgePlanar) {
                    /* If the planar primitive is not on the split plane,
                       the classification is easy. Otherwise, place it on
                       the side with the lower cost */
                    Assert(classification.get(event->index) == PrimClassification::Both);
                    if (event->pos < best.split ||
                        (event->pos == best.split && best.planar_left)) {
                        classification.set(event->index, PrimClassification::Left);
                        prims_left++;
                    } else if (event->pos > best.split ||
                               (event->pos == best.split && !best.planar_left)) {
                        classification.set(event->index, PrimClassification::Right);
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

            auto &left_alloc  = m_local.left_alloc;
            auto &right_alloc = m_local.right_alloc;

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
                        case PrimClassification::Left:
                            *left_events_end++ = event;
                            break;

                        case PrimClassification::Right:
                            *right_events_end++ = event;
                            break;

                        case PrimClassification::Both:
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
                        case PrimClassification::Left:
                            *temp_left_events_end++ = event;
                            break;

                        case PrimClassification::Right:
                            *temp_right_events_end++ = event;
                            break;

                        case PrimClassification::Ignore:
                            break;

                        case PrimClassification::Both: {
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
                                                EdgeEvent::Type::EdgeStart, axis, min, event.index);
                                            *new_left_events_end++ = EdgeEvent(
                                                EdgeEvent::Type::EdgeEnd, axis, max, event.index);
                                        } else {
                                            *new_left_events_end++ = EdgeEvent(
                                                EdgeEvent::Type::EdgePlanar, axis, min, event.index);
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
                                                EdgeEvent::Type::EdgeStart, axis, min, event.index);
                                            *new_right_events_end++ = EdgeEvent(
                                                EdgeEvent::Type::EdgeEnd, axis, max, event.index);
                                        } else {
                                            *new_right_events_end++ = EdgeEvent(
                                                EdgeEvent::Type::EdgePlanar, axis, min, event.index);
                                        }
                                    }
                                } else {
                                    pruned_right++;
                                }

                                /* Set classification to 'EIgnore' to ensure that
                                   clipping occurs only once */
                                classification.set(
                                    event.index, PrimClassification::Ignore);
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

            Size children = m_ctx.node_storage.grow_by(2);
            Size left_offset = children - node;

            if (!m_ctx.node_storage[node].set_inner_node(best.axis, best.split, left_offset))
                Throw("Internal error during kd-tree construction: unable "
                      "to store overly large offset to left child node (%i)",
                      left_offset);

            Scalar left_cost =
                build_nlogn(children, best.left_count - pruned_left,
                            left_events_start, left_events_end, left_bbox,
                            depth + 1, bad_refines, true);

            Scalar right_cost =
                build_nlogn(children + 1, best.right_count - pruned_right,
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

                Size offset = m_ctx.index_storage.grow_by((Size) temp.size());

                if (!m_ctx.node_storage[node].set_leaf_node(offset, temp.size()))
                    Throw("Internal error: could not create leaf node with %i "
                          "primitives -- too much geometry?", m_indices.size());

                for(auto it = temp.begin(); it != temp.end(); it++)
                    m_ctx.index_storage[offset++] = *it;

                m_ctx.retracted_splits++;
                return leaf_cost;
            }

            return final_cost;
        }

        /// Create an initial sorted edge event list and start the O(N log N) builder
        Scalar transition_to_nlogn() {
            const auto &derived = m_ctx.derived;
            // m_local = &m_ctx.local; // TODO remove this

            Size prim_count = Size(m_indices.size()), final_prim_count = prim_count;

            /* We don't yet know how many edge events there will be. Allocate a
               conservative amount and shrink the buffer later on. */
            Size initial_size = prim_count * 2 * Dimension;

            EdgeEvent *events_start =
                m_local.left_alloc.template allocate<EdgeEvent>(initial_size),
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
                        events_start[offset  ] = EdgeEvent(EdgeEvent::Type::EdgePlanar, axis, min, prim_index);
                        events_start[offset+1].set_invalid();
                    } else {
                        events_start[offset  ] = EdgeEvent(EdgeEvent::Type::EdgeStart, axis, min, prim_index);
                        events_start[offset+1] = EdgeEvent(EdgeEvent::Type::EdgeEnd,   axis, max, prim_index);
                    }
                }
            }

            /* Release index list */
            IndexVector().swap(m_indices);

            /* Sort the events list and remove invalid ones from the end */
            std::sort(events_start, events_end);
            while (events_start != events_end && !(events_end-1)->valid())
                --events_end;

            m_local.left_alloc.template shrink_allocation<EdgeEvent>(
                events_start, events_end - events_start);
            m_local.classification_storage.resize(derived.primitive_count());
            m_local.ctx = &m_ctx;

            Scalar cost = build_nlogn(m_node, final_prim_count, events_start,
                                      events_end, m_bbox, m_depth, 0);

            m_local.left_alloc.release(events_start);

            return cost;
        }

        /// Create a leaf node using the given set of indices (called by min-max binning)
        template <typename T> void make_leaf(T &&indices) {
            Size offset = m_ctx.index_storage.grow_by((Size) indices.size());

            if (!m_ctx.node_storage[m_node].set_leaf_node(offset, indices.size()))
                Throw("Internal error: could not create leaf node with %i "
                      "primitives -- too much geometry?", m_indices.size());

            for(auto it = indices.begin(); it != indices.end(); it++)
                m_ctx.index_storage[offset++] = *it;

            *m_cost = m_ctx.derived.cost_model().leaf_cost(Size(indices.size()));
        }

        /// Create a leaf node using the given edge event list (called by the O(N log N) builder)
        void make_leaf(Index node, Size prim_count, EdgeEvent *events_start,
                       EdgeEvent *events_end) const {
            auto offset = m_ctx.index_storage.grow_by(prim_count);

            if (!m_ctx.node_storage[node].set_leaf_node(offset, prim_count))
                Throw("Internal error: could not create leaf node with %i "
                      "primitives -- too much geometry?", prim_count);

            for (auto event = events_start; event != events_end; ++event) {
                if (event->axis != 0)
                    break;
                if (event->type == EdgeEvent::Type::EdgeStart ||
                    event->type == EdgeEvent::Type::EdgePlanar) {
                    Assert(--prim_count >= 0);
                    m_ctx.index_storage[offset++] = event->index;
                }
            }

            Assert(prim_count == 0);
        }

        /// Traverse a subtree and collect all encountered primitive references in a set
        void traverse(Index node_index, std::unordered_set<Index> &result) {
            auto& node = m_ctx.node_storage[node_index];
            if (node.leaf()) {
                for (Size i = 0; i < node.primitive_count(); ++i)
                    result.insert(m_ctx.index_storage[node.primitive_offset() + i]);
            } else {
                Index left_child  = node_index + node.left_offset(),
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
            m_max_depth = (int) (8 + 1.3f * dr::log2i(prim_count));
        m_max_depth = std::min(m_max_depth, (Size) MI_KD_MAXDEPTH);

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
            Log(Warn, "kd-tree contains no geometry!");
            ctx.node_storage[0].set_leaf_node(0, 0);
            m_bbox.min = 0.f;
            m_bbox.max = 0.f;
        } else {
            Log(m_log_level, "Creating a preliminary index list (%s)",
                util::mem_string(prim_count * sizeof(Index)).c_str());

            IndexVector indices(prim_count);
            for (size_t i = 0; i < prim_count; ++i)
                indices[i] = (Index) i;

            BuildTask task = BuildTask(ctx, 0, std::move(indices), m_bbox,
                                       m_bbox, 0, 0, &final_cost);
            task.execute();
        }

        Log(m_log_level, "Structural kd-tree statistics:");

        /* ==================================================================== */
        /*     Store the node and index lists in a compact contiguous format    */
        /* ==================================================================== */

        m_node_count  = (Index) ctx.node_storage.size();
        m_index_count = (Index) ctx.index_storage.size();

        m_indices.reset(new Index[m_index_count]);
        dr::parallel_for(
            dr::blocked_range<Size>(0u, m_index_count, MI_KD_GRAIN_SIZE),
            [&](const dr::blocked_range<Size> &range) {
                for (Size i = range.begin(); i != range.end(); ++i)
                    m_indices[i] = ctx.index_storage[i];
            }
        );
        ctx.index_storage.release();

        m_nodes.reset(new KDNode[m_node_count]);
        dr::parallel_for(
            dr::blocked_range<Size>(0u, m_node_count, MI_KD_GRAIN_SIZE),
            [&](const dr::blocked_range<Size> &range) {
                for (Size i = range.begin(); i != range.end(); ++i)
                    m_nodes[i] = ctx.node_storage[i];
            }
        );
        ctx.node_storage.release();

        /* Slightly avoid the bounding box to avoid numerical issues
           involving geometry that exactly lies on the boundary */
        Vector extra = (m_bbox.extents() + 1.f) * dr::Epsilon<Scalar>;
        m_bbox.min -= extra;
        m_bbox.max += extra;

        /* ==================================================================== */
        /*         Print various tree statistics if requested by the user       */
        /* ==================================================================== */

        if (Thread::thread()->logger()->log_level() <= m_log_level) {
            compute_statistics(ctx, m_nodes.get(), m_bbox, 0);

            // Trigger per-thread data release
            // ctx.local.clear(); // TODO is this necessary?

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
    }

protected:
    std::unique_ptr<KDNode[]> m_nodes;
    std::unique_ptr<Index[]> m_indices;
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
    LogLevel m_log_level = Debug;
    BoundingBox m_bbox;
};

template <typename BoundingBox, typename Index, typename CostModel, typename Derived>
Class * TShapeKDTree<BoundingBox, Index, CostModel, Derived>::m_class = new Class("TShapeKDTree", "Object", "", nullptr, nullptr);

template <typename BoundingBox, typename Index, typename CostModel, typename Derived>
const Class *TShapeKDTree<BoundingBox, Index, CostModel, Derived>::class_() const {
    return m_class;
}

template <typename Float> class SurfaceAreaHeuristic3 {
public:
    using Size          = uint32_t;
    using Index         = uint32_t;
    using BoundingBox3f = BoundingBox<Point<Float, 3>>;
    using Vector3f      = Vector<Float, 3>;

    SurfaceAreaHeuristic3(Float query_cost, Float traversal_cost,
                          Float empty_space_bonus)
        : m_query_cost(query_cost), m_traversal_cost(traversal_cost),
          m_empty_space_bonus(empty_space_bonus) {
        if (m_query_cost <= 0)
            Throw("The query cost must be > 0");
        if (m_traversal_cost <= 0)
            Throw("The traversal cost must be > 0");
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
        auto a = dr::shuffle<1, 2, 0>(extents);
        auto b = dr::shuffle<2, 0, 1>(extents);
        m_temp0 = m_temp1 = (a * b) * temp;
        m_temp2 = (a + b) * temp;

        m_temp0 = dr::fnmadd(m_temp2, bbox.min, m_temp0);
        m_temp1 = dr::fmadd(m_temp2, bbox.max, m_temp1);
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

    friend std::ostream &operator<<(std::ostream &os, const SurfaceAreaHeuristic3 &sa) {
        os << "SurfaceAreaHeuristic3[" << std::endl
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

template <typename Float, typename Spectrum>
class MI_EXPORT_LIB ShapeKDTree : public TShapeKDTree<BoundingBox<Point<dr::scalar_t<Float>, 3>>, uint32_t,
                                                          SurfaceAreaHeuristic3<dr::scalar_t<Float>>,
                                                          ShapeKDTree<Float, Spectrum>> {
public:
    MI_IMPORT_TYPES(Shape, Mesh)

    using ScalarRay3f = Ray<ScalarPoint3f, Spectrum>;

    using SurfaceAreaHeuristic3f = SurfaceAreaHeuristic3<ScalarFloat>;
    using Size                   = uint32_t;
    using Index                  = uint32_t;

    using Base = TShapeKDTree<ScalarBoundingBox3f, uint32_t, SurfaceAreaHeuristic3f, ShapeKDTree>;
    using typename Base::KDNode;
    using Base::ready;
    using Base::set_clip_primitives;
    using Base::set_exact_primitive_threshold;
    using Base::set_max_depth;
    using Base::set_min_max_bins;
    using Base::set_retract_bad_splits;
    using Base::set_stop_primitives;
    using Base::bbox;
    using Base::m_bbox;
    using Base::m_nodes;
    using Base::m_indices;
    using Base::m_index_count;
    using Base::m_node_count;

    /// Create an empty kd-tree and take build-related parameters from \c props.
    ShapeKDTree(const Properties &props);

    /// Clear the kd-tree (build-related parameters remain)
    void clear();

    /// Register a new shape with the kd-tree (to be called before \ref build())
    void add_shape(Shape *shape);

    /// Build the kd-tree
    void build();

    /// Return the number of registered shapes
    Size shape_count() const { return Size(m_shapes.size()); }

    /// Return the number of registered primitives
    Size primitive_count() const { return m_primitive_map.back(); }

    /// Return the i-th shape (const version)
    const Shape *shape(size_t i) const { Assert(i < m_shapes.size()); return m_shapes[i]; }

    /// Return the i-th shape
    Shape *shape(size_t i) { Assert(i < m_shapes.size()); return m_shapes[i]; }

    /// Return the bounding box of the i-th primitive
    MI_INLINE ScalarBoundingBox3f bbox(Index i) const {
        Index shape_index = find_shape(i);
        return m_shapes[shape_index]->bbox(i);
    }

    /// Return the (clipped) bounding box of the i-th primitive
    MI_INLINE ScalarBoundingBox3f bbox(Index i, const ScalarBoundingBox3f &clip) const {
        Index shape_index = find_shape(i);
        return m_shapes[shape_index]->bbox(i, clip);
    }

    template <bool ShadowRay>
    MI_INLINE PreliminaryIntersection3f ray_intersect_preliminary(const Ray3f &ray,
                                                                   Mask active) const {
        DRJIT_MARK_USED(active);
        if constexpr (!dr::is_array_v<Float>)
            return ray_intersect_scalar<ShadowRay>(ray);
        else
            Throw("kdtree should only be used in scalar mode");
    }

    template <bool ShadowRay>
    MI_INLINE PreliminaryIntersection<ScalarFloat, Shape>
    ray_intersect_scalar(ScalarRay3f ray) const {
        /// Ray traversal stack entry
        struct KDStackEntry {
            // Ray distance associated with the node entry and exit point
            ScalarFloat mint, maxt;
            // Pointer to the far child
            const KDNode *node;
        };

        // Allocate the node stack
        KDStackEntry stack[MI_KD_MAXDEPTH];
        int32_t stack_index = 0;

        // Resulting intersection struct
        PreliminaryIntersection<ScalarFloat, Shape> pi;

        // Intersect against the scene bounding box
        auto bbox_result = m_bbox.ray_intersect(ray);

        ScalarFloat mint = std::max(ScalarFloat(0), std::get<1>(bbox_result)),
                    maxt = std::min(ray.maxt, std::get<2>(bbox_result));

        ScalarVector3f d_rcp = dr::rcp(ray.d);

        const KDNode *node = m_nodes.get();
        while (mint <= maxt) {
            if (likely(!node->leaf())) { // Inner node
                const ScalarFloat split = node->split();
                const uint32_t axis     = node->axis();

                /* Compute parametric distance along the rays to the split plane */
                ScalarFloat t_plane = (split - ray.o[axis]) * d_rcp[axis];

                bool left_first  = (ray.o[axis] < split) ||
                                   (ray.o[axis] == split && ray.d[axis] >= 0.f),
                     start_after = t_plane<mint, end_before = t_plane> maxt ||
                                   t_plane < 0.f || !dr::isfinite(t_plane),
                     single_node = start_after || end_before;

                /* If we only need to visit one node, just pick the correct one and continue */
                if (likely(single_node)) {
                    bool visit_left = end_before == left_first;
                    node = node->left() + (visit_left ? 0 : 1);
                    continue;
                }

                /* Visit both child nodes in the right order */
                Index node_offset = left_first ? 0 : 1;
                const KDNode *left   = node->left(),
                             *n_cur  = left + node_offset,
                             *n_next = left + (1 - node_offset);

                /* Postpone visit to 'n_next' */
                KDStackEntry& entry = stack[stack_index++];
                entry.mint = t_plane;
                entry.maxt = maxt;
                entry.node = n_next;

                /* Visit 'n_cur' now */
                node = n_cur;
                maxt = t_plane;
                continue;
            } else if (node->primitive_count() > 0) { // Arrived at a leaf node
                Index prim_start = node->primitive_offset();
                Index prim_end = prim_start + node->primitive_count();
                for (Index i = prim_start; i < prim_end; i++) {
                    Index prim_index = m_indices[i];

                    PreliminaryIntersection<ScalarFloat, Shape> prim_pi =
                        intersect_prim<ShadowRay>(prim_index, ray);

                    if (unlikely(prim_pi.is_valid())) {
                        if constexpr (ShadowRay)
                            return prim_pi;

                        Assert(prim_pi.t >= 0.f && prim_pi.t <= ray.maxt);
                        pi = prim_pi;
                        ray.maxt = pi.t;
                    }
                }
            }

            if (likely(stack_index > 0)) {
                --stack_index;
                KDStackEntry& entry = stack[stack_index];
                mint = entry.mint;
                maxt = std::min(entry.maxt, ray.maxt);
                node = entry.node;
            } else {
                break;
            }
        }

        return pi;
    }

#if 0
    template <bool ShadowRay>
    MI_INLINE PreliminaryIntersection3f ray_intersect_packet(Ray3f ray,
                                                              Mask active) const {
        /// Ray traversal stack entry
        struct KDStackEntry {
            // Ray distance associated with the node entry and exit point
            Float mint, maxt;
            // Is the corresponding SIMD lane enabled?
            Mask active;
            // Pointer to the far child
            const KDNode *node;
        };

        // Allocate the node stack
        KDStackEntry stack[MI_KD_MAXDEPTH];
        int32_t stack_index = 0;

        // Resulting intersection struct
        PreliminaryIntersection3f pi = dr::zeros<PreliminaryIntersection3f>();

        const KDNode *node = m_nodes.get();

        /* Intersect against the scene bounding box */
        auto bbox_result = m_bbox.ray_intersect(ray);
        Float mint = dr::maximum(ray.mint, std::get<1>(bbox_result));
        Float maxt = dr::minimum(ray.maxt, std::get<2>(bbox_result));

        while (true) {
            active = active && (maxt >= mint);
            if (ShadowRay)
                active = active && !pi.is_valid();

            if (likely(dr::any(active))) {
                if (likely(!node->leaf())) { // Inner node
                    const dr::scalar_t<Float> split = node->split();
                    const uint32_t axis = node->axis();

                    // Compute parametric distance along the rays to the split plane
                    Float t_plane          = (split - ray.o[axis]) * ray.d_rcp[axis];
                    Mask left_first        = (ray.o[axis] < split) ||
                                              (dr::eq(ray.o[axis], split) && ray.d[axis] >= 0.f),
                         start_after       = t_plane < mint,
                         end_before        = t_plane > maxt || t_plane < 0.f || !dr::isfinite(t_plane),
                         single_node       = start_after || end_before,
                         visit_left        = dr::eq(end_before, left_first),
                         visit_only_left   = single_node &&  visit_left,
                         visit_only_right  = single_node && !visit_left;

                    bool all_visit_only_left  = dr::all(visit_only_left || !active),
                         all_visit_only_right = dr::all(visit_only_right || !active),
                         all_visit_same_node  = all_visit_only_left || all_visit_only_right;

                    /* If we only need to visit one node, just pick the correct one and continue */
                    if (all_visit_same_node) {
                        node = node->left() + (all_visit_only_left ? 0 : 1);
                        continue;
                    }

                    size_t left_votes  = count(left_first && active),
                           right_votes = count(!left_first && active);

                    bool go_left = left_votes >= right_votes;

                    Mask go_left_bcast = Mask(go_left),
                         correct_order = dr::eq(left_first, go_left_bcast),
                         visit_both    = !single_node,
                         visit_cur     = visit_both || eq (visit_left, go_left_bcast),
                         visit_next    = visit_both || dr::neq(visit_left, go_left_bcast);

                    /* Visit both child nodes in the right order */
                    Index node_offset = go_left ? 0 : 1;
                    const KDNode *left   = node->left(),
                                 *n_cur  = left + node_offset,
                                 *n_next = left + (1 - node_offset);

                    /* Postpone visit to 'n_next' */
                    Mask sel0 =  correct_order && visit_both,
                         sel1 = !correct_order && visit_both;
                    KDStackEntry& entry = stack[stack_index++];
                    entry.mint = dr::select(sel0, t_plane, mint);
                    entry.maxt = dr::select(sel1, t_plane, maxt);
                    entry.active = active && visit_next;
                    entry.node = n_next;

                    /* Visit 'n_cur' now */
                    mint = dr::select(sel1, t_plane, mint);
                    maxt = dr::select(sel0, t_plane, maxt);
                    active = active && visit_cur;
                    node = n_cur;
                    continue;
                } else if (node->primitive_count() > 0) { // Arrived at a leaf node
                    Index prim_start = node->primitive_offset();
                    Index prim_end = prim_start + node->primitive_count();
                    for (Index i = prim_start; i < prim_end; i++) {
                        Index prim_index = m_indices[i];

                        PreliminaryIntersection3f prim_pi =
                            intersect_prim<ShadowRay>(prim_index, ray, active);

                        dr::masked(pi, prim_pi.is_valid()) = prim_pi;

                        if constexpr (!ShadowRay) {
                            Assert(dr::all(!prim_pi.is_valid() ||
                                       (prim_pi.t >= ray.mint &&
                                        prim_pi.t <= ray.maxt)));
                            dr::masked(ray.maxt, prim_pi.is_valid()) = prim_pi.t;
                        }
                    }
                }
            }

            if (likely(stack_index > 0)) {
                --stack_index;
                KDStackEntry& entry = stack[stack_index];
                mint = entry.mint;
                maxt = dr::minimum(entry.maxt, ray.maxt);
                active = entry.active;
                node = entry.node;
            } else {
                break;
            }
        }

        return pi;
    }
#endif

    /// Brute force intersection routine for debugging purposes
    template <bool ShadowRay>
    MI_INLINE PreliminaryIntersection3f
    ray_intersect_naive(Ray3f ray, Mask active) const {
        if constexpr (!dr::is_array_v<Float>) {
            PreliminaryIntersection3f pi = dr::zeros<PreliminaryIntersection3f>();

            for (Size i = 0; i < primitive_count(); ++i) {
                PreliminaryIntersection3f prim_pi = intersect_prim<ShadowRay>(i, ray);

                if constexpr (dr::is_array_v<Float>) {
                    dr::masked(pi, prim_pi.is_valid()) = prim_pi;
                } else if (prim_pi.is_valid()) {
                    pi = prim_pi;
                    ray.maxt = prim_pi.t;
                }

                if (ShadowRay && dr::all(pi.is_valid() || !active))
                    break;
            }

            return pi;
        } else {
            Throw("kdtree should only be used in scalar mode");
        }
    }

    /// Return a human-readable string representation of the scene contents.
    virtual std::string to_string() const override;

    MI_DECLARE_CLASS()
protected:
    /**
     * \brief Map an abstract \ref TShapeKDTree primitive index to a specific
     * shape managed by the \ref ShapeKDTree.
     *
     * The function returns the shape index and updates the \a idx parameter to
     * point to the primitive index (e.g. triangle ID) within the shape.
     */
    MI_INLINE Index find_shape(Index &i) const {
        Assert(i < primitive_count());

        Index shape_index = math::find_interval<Index>(
            Size(m_primitive_map.size()),
            [&](Index k) DRJIT_INLINE_LAMBDA {
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

    /**
     * \brief Check whether a primitive is intersected by the given ray.
     *
     * Some temporary space is supplied to store data that can later be used to
     * create a detailed intersection record.
     */
    template <bool ShadowRay = false>
    MI_INLINE PreliminaryIntersection<ScalarFloat, Shape>
    intersect_prim(Index prim_index, const ScalarRay3f &ray) const {
        Index shape_index  = find_shape(prim_index);
        const Shape *shape = this->shape(shape_index);
        const Mesh *mesh = (const Mesh *) shape;

        PreliminaryIntersection<ScalarFloat, Shape> pi;

        if constexpr (ShadowRay) {
            bool hit;
            if (shape->is_mesh())
                hit = mesh->ray_intersect_triangle_scalar(prim_index, ray).first != dr::Infinity<ScalarFloat>;
            else
                hit = shape->ray_test_scalar(ray);
            pi.t = dr::select(hit, 0.f , pi.t);
        } else {
            uint32_t inst_index = (uint32_t) -1;
            if (shape->is_mesh())
                std::tie(pi.t, pi.prim_uv) = mesh->ray_intersect_triangle_scalar(prim_index, ray);
            else
                std::tie(pi.t, pi.prim_uv, inst_index, prim_index) =
                    shape->ray_intersect_preliminary_scalar(ray);
            pi.prim_index = prim_index;

            bool hit_inst  = (inst_index != (uint32_t) -1);
            pi.shape       = hit_inst ? (const Shape *) (size_t) shape_index : shape; // shape_index for LLVM + kdtree
            pi.instance    = hit_inst ? shape : nullptr;
            pi.shape_index = hit_inst ? inst_index : shape_index;
        }

        return pi;
    }

protected:
    std::vector<ref<Shape>> m_shapes;
    std::vector<Size> m_primitive_map;
};

MI_EXTERN_CLASS(ShapeKDTree)
NAMESPACE_END(mitsuba)
