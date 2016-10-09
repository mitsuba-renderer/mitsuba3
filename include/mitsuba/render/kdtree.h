#pragma once

#include <mitsuba/core/logger.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/tls.h>
#include <mitsuba/render/shape.h>
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
 * Size primitiveCount() const;
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
 * the overall construction time. The \ref setClipPrimitives() method can be
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

    enum {
        Dimension = BoundingBox::Dimension
    };

    /* ==================================================================== */
    /*                     Public kd-tree interface                         */
    /* ==================================================================== */

    TShapeKDTree(const CostModel &model) : m_costModel(model) { }

    /// Return the cost model used by the tree construction algorithm
    CostModel costModel() const { return m_costModel; }

    /// Return the maximum tree depth (0 == use heuristic)
    Size maxDepth() const { return m_maxDepth; }

    /// Set the maximum tree depth (0 == use heuristic)
    void setMaxDepth(Size maxDepth) { m_maxDepth = maxDepth; }

    /// Return the number of bins used for Min-Max binning
    Size minMaxBins() const { return m_minMaxBins; }

    /// Set the number of bins used for Min-Max binning
    void setMinMaxBins(Size minMaxBins) { m_minMaxBins = minMaxBins; }

    /// Return whether primitive clipping is used during tree construction
    bool clipPrimitives() const { return m_clipPrimitives; }

    /// Set whether primitive clipping is used during tree construction
    void setClipPrimitives(bool clip) { m_clipPrimitives = clip; }

    /// Return whether or not bad splits can be "retracted".
    bool retractBadSplits() const { return m_retractBadSplits; }

    /// Specify whether or not bad splits can be "retracted".
    void setRetractBadSplits(bool retract) { m_retractBadSplits = retract; }

    /**
     * \brief Return the number of bad refines allowed to happen
     * in succession before a leaf node will be created.
     */
    Size maxBadRefines() const { return m_maxBadRefines; }

    /**
     * \brief Set the number of bad refines allowed to happen
     * in succession before a leaf node will be created.
     */
    void setMaxBadRefines(Size maxBadRefines) { m_maxBadRefines = maxBadRefines; }

    /**
     * \brief Return the number of primitives, at which recursion will
     * stop when building the tree.
     */
    Size stopPrimitives() const { return m_stopPrimitives; }

    /**
     * \brief Set the number of primitives, at which recursion will
     * stop when building the tree.
     */
    void setStopPrimitives(Size stopPrimitives) { m_stopPrimitives = stopPrimitives; }

    /**
     * \brief Return the number of primitives, at which the builder will switch
     * from (approximate) Min-Max binning to the accurate O(n log n)
     * optimization method.
     */
    Size exactPrimitiveThreshold() const { return m_exactPrimThreshold; }

    /**
     * \brief Specify the number of primitives, at which the builder will
     * switch from (approximate) Min-Max binning to the accurate O(n log n)
     * optimization method.
     */
    void setExactPrimitiveThreshold(Size exactPrimThreshold) {
        m_exactPrimThreshold = exactPrimThreshold;
    }

    /// Return the log level of kd-tree status messages
    ELogLevel logLevel() const { return m_logLevel; }

    /// Return the log level of kd-tree status messages
    void setLogLevel(ELogLevel level) { m_logLevel = level; }

    bool ready() const { return false; }

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
                unsigned int leftOffset : sizeof(Index) * 8 - 3;
            } inner;

            /// Leaf node
            struct {
                #if defined(LITTLE_ENDIAN)
                    /// How many primitives does this leaf reference?
                    unsigned int primCount : 23;

                    /// Mask bits (all 1s for leaf nodes)
                    unsigned int mask : 9;
                #else /* Swap for big endian machines */
                    /// Mask bits (all 1s for leaf nodes)
                    unsigned int mask : 9;

                    /// How many primitives does this leaf reference?
                    unsigned int primCount : 23;
                #endif

                /// Start offset of the primitive list
                Index primOffset;
            } leaf;
        } data;

        /**
         * \brief Initialize a leaf kd-tree node.
         *
         * Returns \c false if the offset or number of primitives is so large
         * that it can't be represented
         */
        bool setLeafNode(size_t primOffset, size_t primCount) {
            data.leaf.primCount = (unsigned int) primCount;
            data.leaf.mask      = 0b111111111u;
            data.leaf.primOffset = (Index) primOffset;
            return (size_t) data.leaf.primOffset == primOffset &&
                   (size_t) data.leaf.primCount == primCount;
        }

        /**
         * \brief Initialize an interior kd-tree node.
         *
         * Returns \c false if the offset or number of primitives is so large
         * that it can't be represented
         */
        bool setInnerNode(int axis, Scalar split, size_t leftOffset) {
            data.inner.split = split;
            data.inner.axis = (unsigned int) axis;
            data.inner.leftOffset = (Index) leftOffset;
            return (size_t) data.inner.leftOffset == leftOffset;
        }

        /// Is this a leaf node?
        bool leaf() const { return data.leaf.mask == 0b111111111u; }

        /// Assuming this is a leaf node, return the first primitive index
        Index primitiveOffset() const { return data.leaf.primOffset; }

        /// Assuming this is a leaf node, return the number of primitives
        Index primitiveCount() const { return data.leaf.primCount; }

        /// Assuming that this is an inner node, return the relative offset to the left child
        Index leftOffset() const { return data.inner.leftOffset; }

        /// Return the left child (for interior nodes)
        const KDNode *left() const { return this + data.inner.leftOffset; }

        /// Return the left child (for interior nodes)
        const KDNode *right() const { return this + data.inner.leftOffset + 1; }

        /// Return the split plane location (for interior nodes)
        Scalar split() const { return data.inner.split; }

        /// Return the split axis (for interior nodes)
        int axis() const { return (int) data.inner.axis; }

        /// Return a string representation
        friend std::ostream& operator<<(std::ostream &os, const KDNode &n) {
            if (n.leaf()) {
                os << "KDNode[leaf, primitiveOffset=" << n.primitiveOffset()
                   << ", primitiveCount=" << n.primitiveCount() << "]";
            } else {
                os << "KDNode[interior, axis=" << n.axis()
                    << ", split=" << n.split()
                    << ", leftOffset=" << n.leftOffset() << "]";
            }
            return os;
        }
    };

    static_assert(sizeof(KDNode) == sizeof(Size) + sizeof(Scalar),
                  "kd-tree node has unexpected size. Padding issue?");

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
        OrderedChunkAllocator(size_t minAllocation = MTS_KD_MIN_ALLOC)
                : m_minAllocation(minAllocation) {
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
        template <typename T> T * SIMD_MALLOC allocate(size_t size) {
            size *= sizeof(T);

            for (auto &chunk : m_chunks) {
                if (chunk.remainder() >= size) {
                    T* result = reinterpret_cast<T *>(chunk.cur);
                    chunk.cur += size;
                    return result;
                }
            }

            /* No chunk had enough free memory */
            size_t allocSize = std::max(size, m_minAllocation);

            std::unique_ptr<uint8_t[]> data(new uint8_t[allocSize]);
            uint8_t *start = data.get(), *cur = start + size;
            m_chunks.emplace_back(std::move(data), cur, allocSize);

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
        template <typename T> void shrinkAllocation(T *ptr_, size_t newSize) {
            auto ptr = reinterpret_cast<uint8_t *>(ptr_);
            newSize *= sizeof(T);

            for (auto &chunk: m_chunks) {
                if (chunk.contains(ptr)) {
                    chunk.cur = ptr + newSize;
                    return;
                }
            }

            #if !defined(NDEBUG)
                if (newSize == 0) {
                    for (auto const &chunk : m_chunks) {
                        if (ptr == chunk.start.get() + chunk.size)
                            return; /* Potentially 0-sized buffer, don't be too stringent */
                    }
                }

                Throw("OrderedChunkAllocator: Internal error while releasing memory");
            #endif
        }

        /// Return the currently allocated number of chunks
        size_t chunkCount() const { return m_chunks.size(); }

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

        size_t m_minAllocation;
        std::vector<Chunk> m_chunks;
    };

    /* ==================================================================== */
    /*                    Build-related data structures                     */
    /* ==================================================================== */

    struct BuildContext;

    /// Helper data structure used during tree construction (used by a single thread)
    struct LocalBuildContext {
        ClassificationStorage classificationStorage;
        OrderedChunkAllocator leftAlloc;
        OrderedChunkAllocator rightAlloc;
        BuildContext *ctx = nullptr;

        ~LocalBuildContext() {
            Assert(leftAlloc.used() == 0);
            Assert(rightAlloc.used() == 0);
            if (ctx)
                ctx->tempStorage += leftAlloc.size() + rightAlloc.size() +
                                    classificationStorage.size();
        }
    };

    /// Helper data structure used during tree construction (shared by all threads)
    struct BuildContext {
        const Derived &derived;
        Thread *thread;
        tbb::concurrent_vector<KDNode> nodeStorage;
        tbb::concurrent_vector<Index> indexStorage;
        ThreadLocal<LocalBuildContext> local;

        /* Keep some statistics about the build process */
        std::atomic<size_t> emptyNodes {0};
        std::atomic<size_t> badRefines {0};
        std::atomic<size_t> retractedSplits {0};
        std::atomic<size_t> pruned {0};
        std::atomic<size_t> tempStorage {0};
        std::atomic<size_t> workUnits {0};
        double expTraversalSteps = 0;
        double expLeavesVisited = 0;
        double expPrimitivesQueried = 0;
        Size maxPrimsInLeaf = 0;
        Size nonEmptyLeafCount = 0;
        Size maxDepth = 0;
        Size primBuckets[16] { };

        BuildContext(const Derived &derived)
            : derived(derived), thread(Thread::thread()) { }
    };


    /// Data type for split candidates suggested by the tree cost model
    struct SplitCandidate {
        Scalar cost = std::numeric_limits<Scalar>::infinity();
        Scalar split = 0;
        int axis = 0;
        Size leftCount = 0, rightCount = 0;
        Size rightBin = 0;       /* used by min-max binning only */
        bool planarLeft = false; /* used by the O(n log n) builder only */

        friend std::ostream& operator<<(std::ostream &os, const SplitCandidate &c) {
            os << "SplitCandidate[" << std::endl
               << "  cost = " << c.cost << "," << std::endl
               << "  split = " << c.split << "," << std::endl
               << "  axis = " << c.axis << "," << std::endl
               << "  leftCount = " << c.leftCount << "," << std::endl
               << "  rightCount = " << c.rightCount << "," << std::endl
               << "  rightBin = " << c.rightBin << "," << std::endl
               << "  planarLeft = " << (c.planarLeft ? "yes" : "no") << std::endl
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

        void setInvalid() { pos = 0; index = 0; type = 0; axis = 7; }
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
        MinMaxBins(Size binCount, const BoundingBox &bbox)
            : m_bins(binCount * Dimension * 2, Size(0)), m_binCount(binCount),
              m_invBinSize(1 / (bbox.extents() / (Scalar) binCount)),
              m_maxBin(binCount - 1), m_bbox(bbox) {
            Assert(bbox.valid());
        }

        MinMaxBins(const MinMaxBins &other)
            : m_bins(other.m_bins), m_binCount(other.m_binCount),
              m_invBinSize(other.m_invBinSize), m_maxBin(other.m_maxBin),
              m_bbox(other.m_bbox) { }

        MinMaxBins(MinMaxBins &&other)
            : m_bins(std::move(other.m_bins)), m_binCount(other.m_binCount),
              m_invBinSize(other.m_invBinSize), m_maxBin(other.m_maxBin),
              m_bbox(other.m_bbox) { }

        void operator=(const MinMaxBins &other) {
            m_bins = other.m_bins;
            m_binCount = other.m_binCount;
            m_invBinSize = other.m_invBinSize;
            m_maxBin = other.m_maxBin;
            m_bbox = other.m_bbox;
        }

        void operator=(MinMaxBins &&other) {
            m_bins = std::move(other.m_bins);
            m_binCount = other.m_binCount;
            m_invBinSize = other.m_invBinSize;
            m_maxBin = other.m_maxBin;
            m_bbox = other.m_bbox;
        }

        MinMaxBins& operator+=(const MinMaxBins &other) {
            Assert(m_bins.size() == other.m_bins.size());
            for (Size i = 0; i < m_bins.size(); ++i)
                m_bins[i] += other.m_bins[i];
            return *this;
        }

        MTS_INLINE void put(BoundingBox bbox) {
            using IndexArray = simd::Array<Index, 3>;
            const IndexArray offsetMin = IndexArray(0, 2 * m_binCount, 4 * m_binCount);
            const IndexArray offsetMax = offsetMin + 1;
            Index *ptr = m_bins.data();

            Assert(bbox.valid());
            Vector relMin = (bbox.min - m_bbox.min) * m_invBinSize;
            Vector relMax = (bbox.max - m_bbox.min) * m_invBinSize;

            relMin = min(max(relMin, Vector::Zero()), m_maxBin);
            relMax = min(max(relMax, Vector::Zero()), m_maxBin);

            IndexArray indexMin = simd::cast<IndexArray>(relMin);
            IndexArray indexMax = simd::cast<IndexArray>(relMax);

            Assert(simd::all(indexMin <= indexMax));
            indexMin = indexMin + indexMin + offsetMin;
            indexMax = indexMax + indexMax + offsetMax;

            IndexArray minCounts = IndexArray::Gather(ptr, indexMin),
                       maxCounts = IndexArray::Gather(ptr, indexMax);

            simd::scatter(ptr, minCounts + 1, indexMin);
            simd::scatter(ptr, maxCounts + 1, indexMax);
        }

        SplitCandidate bestCandidate(Size primCount, const CostModel &model) const {
            const Index *bin = m_bins.data();
            SplitCandidate best;

            Vector step = m_bbox.extents() / Scalar(m_binCount);

            for (int axis = 0; axis < Dimension; ++axis) {
                SplitCandidate candidate;

                /* Initially: all primitives to the right, none on the left */
                candidate.leftCount = 0;
                candidate.rightCount = primCount;
                candidate.rightBin = 0;
                candidate.axis = axis;
                candidate.split = m_bbox.min[axis];

                for (Index i = 0; i < m_binCount; ++i) {
                    /* Evaluate the cost model and keep the best candidate */
                    candidate.cost = model.innerCost(
                        axis, candidate.split,
                        model.leafCost(candidate.leftCount),
                        model.leafCost(candidate.rightCount));

                    if (candidate.cost < best.cost)
                        best = candidate;

                    /* Move one bin to the right and

                       1. Increase leftCount by the number of primitives which
                          started in the bin (thus they at least overlap with
                          the left interval). This information is stored in the MIN
                          bin.

                       2. Reduce rightCount by the number of primitives which
                          ended (thus they are entirely on the left). This
                          information is stored in the MAX bin.
                    */
                    candidate.leftCount  += *bin++; /* MIN-bin */
                    candidate.rightCount -= *bin++; /* MAX-bin */
                    candidate.rightBin++;
                    candidate.split += step[axis];
                }

                /* Evaluate the cost model and keep the best candidate */
                candidate.cost = model.innerCost(
                    axis, candidate.split,
                    model.leafCost(candidate.leftCount),
                    model.leafCost(candidate.rightCount));

                if (candidate.cost < best.cost)
                    best = candidate;

                Assert(candidate.leftCount == primCount);
                Assert(candidate.rightCount == 0);
            }

            Assert(bin == m_bins.data() + m_bins.size());
            Assert(best.leftCount + best.rightCount >= primCount);

            if (best.rightBin == 0) {
                best.split = m_bbox.min[best.axis];
            } else if (best.rightBin == m_binCount) {
                best.split = m_bbox.max[best.axis];
            } else {
                auto predicate = [
                    invBinSize = m_invBinSize[best.axis],
                    offset = m_bbox.min[best.axis],
                    rightBin = best.rightBin
                ](Scalar value) {
                    /* Predicate which says whether a value falls on the left
                       of the chosen split plane. This function is meant to
                       behave exactly the same way as put() above. */
                    return Index((value - offset) * invBinSize) < rightBin;
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
            IndexVector leftIndices;
            IndexVector rightIndices;
            BoundingBox leftBounds;
            BoundingBox rightBounds;
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
                         maxBin = m_maxBin[axis],
                         invBinSize = m_invBinSize[axis];

            tbb::spin_mutex mutex;
            IndexVector leftIndices(split.leftCount);
            IndexVector rightIndices(split.rightCount);
            Size leftCount = 0, rightCount = 0;
            BoundingBox leftBounds, rightBounds;

            tbb::parallel_for(
                tbb::blocked_range<Size>(0u, Size(indices.size()), MTS_KD_GRAIN_SIZE),
                [&](const tbb::blocked_range<Index> &range) {
                    IndexVector leftIndicesLocal, rightIndicesLocal;
                    BoundingBox leftBoundsLocal, rightBoundsLocal;

                    leftIndicesLocal.reserve(range.size());
                    rightIndicesLocal.reserve(range.size());

                    for (Size i = range.begin(); i != range.end(); ++i) {
                        const Index primIndex = indices[i];
                        const BoundingBox primBBox = derived.bbox(primIndex);

                        Scalar relMin = (primBBox.min[axis] - offset) * invBinSize;
                        Scalar relMax = (primBBox.max[axis] - offset) * invBinSize;

                        relMin = std::min(std::max(relMin, Scalar(0)), maxBin);
                        relMax = std::min(std::max(relMax, Scalar(0)), maxBin);

                        const Size indexMin = (Size) relMin,
                                   indexMax = (Size) relMax;

                        if (indexMax < split.rightBin) {
                            leftIndicesLocal.push_back(primIndex);
                            leftBoundsLocal.expand(primBBox);
                        } else if (indexMin >= split.rightBin) {
                            rightIndicesLocal.push_back(primIndex);
                            rightBoundsLocal.expand(primBBox);
                        } else {
                            leftIndicesLocal.push_back(primIndex);
                            rightIndicesLocal.push_back(primIndex);
                            leftBoundsLocal.expand(primBBox);
                            rightBoundsLocal.expand(primBBox);
                        }
                    }

                    /* Merge into global results */
                    Index *targetLeft, *targetRight;
                    {
                        tbb::spin_mutex::scoped_lock lock(mutex);
                        targetLeft = &leftIndices[leftCount];
                        targetRight = &rightIndices[rightCount];
                        leftCount += leftIndicesLocal.size();
                        rightCount += rightIndicesLocal.size();
                        leftBounds.expand(leftBoundsLocal);
                        rightBounds.expand(rightBoundsLocal);
                    }

                    memcpy(targetLeft, leftIndicesLocal.data(),
                           leftIndicesLocal.size() * sizeof(Size));
                    memcpy(targetRight, rightIndicesLocal.data(),
                           rightIndicesLocal.size() * sizeof(Size));
                }
            );


            Assert(leftCount == split.leftCount);
            Assert(rightCount == split.rightCount);

            return { std::move(leftIndices), std::move(rightIndices),
                     leftBounds, rightBounds };
        }

    protected:
        std::vector<Size> m_bins;
        Size m_binCount;
        Vector m_invBinSize;
        Vector m_maxBin;
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
        BoundingBox m_tightBBox;

        /// Depth of the node within the tree
        Size m_depth;

        /// Number of "bad refines" so far
        Size m_badRefines;

        /// This scalar should be set to the final cost when done
        Scalar *m_cost;

        BuildTask(BuildContext &ctx, const NodeIterator &node,
                  IndexVector &&indices, const BoundingBox bbox,
                  const BoundingBox &tightBBox, Index depth, Size badRefines,
                  Scalar *cost)
            : m_ctx(ctx), m_node(node), m_indices(std::move(indices)),
              m_bbox(bbox), m_tightBBox(tightBBox), m_depth(depth),
              m_badRefines(badRefines), m_cost(cost) {
            Assert(m_bbox.contains(tightBBox));
        }

        /// Run one iteration of min-max binning and spawn recursive tasks
        task *execute() {
            ThreadEnvironment env(m_ctx.thread);
            Size primCount = Size(m_indices.size());
            const Derived &derived = m_ctx.derived;

            m_ctx.workUnits++;

            /* ==================================================================== */
            /*                           Stopping criteria                          */
            /* ==================================================================== */

            if (primCount <= derived.stopPrimitives() ||
                m_depth >= derived.maxDepth() || m_tightBBox.collapsed()) {
                makeLeaf(std::move(m_indices));
                return nullptr;
            }

            if (primCount <= derived.exactPrimitiveThreshold()) {
                *m_cost = transitionToNLogN();
                return nullptr;
            }

            /* ==================================================================== */
            /*                              Binning                                 */
            /* ==================================================================== */

            /* Accumulate all shapes into bins */
            MinMaxBins bins = tbb::parallel_reduce(
                tbb::blocked_range<Size>(0u, primCount, MTS_KD_GRAIN_SIZE),
                MinMaxBins(derived.minMaxBins(), m_tightBBox),

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

            CostModel model(derived.costModel());
            model.setBoundingBox(m_bbox);
            auto best = bins.bestCandidate(primCount, model);

            Assert(std::isfinite(best.cost));
            Assert(best.split >= m_bbox.min[best.axis]);
            Assert(best.split <= m_bbox.max[best.axis]);

            /* Allow a few bad refines in sequence before giving up */
            Scalar leafCost = model.leafCost(primCount);
            if (best.cost >= leafCost) {
                if ((best.cost > 4 * leafCost && primCount < 16)
                    || m_badRefines >= derived.maxBadRefines()) {
                    makeLeaf(std::move(m_indices));
                    return nullptr;
                }
                ++m_badRefines;
                m_ctx.badRefines++;
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

            NodeIterator children = m_ctx.nodeStorage.grow_by(2);
            Size leftOffset(std::distance(m_node, children));

            if (!m_node->setInnerNode(best.axis, best.split, leftOffset))
                Throw("Internal error during kd-tree construction: unable to store "
                      "overly large offset to left child node (%i)", leftOffset);

            BoundingBox leftBounds(m_bbox), rightBounds(m_bbox);

            leftBounds.max[best.axis] = Scalar(best.split);
            rightBounds.min[best.axis] = Scalar(best.split);

            partition.leftBounds.clip(leftBounds);
            partition.rightBounds.clip(rightBounds);

            Scalar leftCost = 0, rightCost = 0;

            BuildTask &leftTask = *new (allocate_child()) BuildTask(
                m_ctx, children, std::move(partition.leftIndices),
                leftBounds, partition.leftBounds, m_depth+1,
                m_badRefines, &leftCost);

            BuildTask &rightTask = *new (allocate_child()) BuildTask(
                m_ctx, std::next(children), std::move(partition.rightIndices),
                rightBounds, partition.rightBounds, m_depth + 1,
                m_badRefines, &rightCost);

            set_ref_count(3);
            spawn(leftTask);
            spawn(rightTask);
            wait_for_all();

            /* ==================================================================== */
            /*                           Final decision                             */
            /* ==================================================================== */

            *m_cost = model.innerCost(
                best.axis,
                best.split,
                leftCost, rightCost
            );

            /* Tear up bad (i.e. costly) subtrees and replace them with leaf nodes */
            if (unlikely(*m_cost > leafCost && derived.retractBadSplits())) {
                std::unordered_set<Index> temp;
                traverse(m_node, temp);
                m_ctx.retractedSplits++;
                makeLeaf(std::move(temp));
            }

            return nullptr;
        }

        /// Recursively run the O(N log N builder)
        Scalar buildNLogN(NodeIterator node, Size primCount,
                          EdgeEvent *eventsStart, EdgeEvent *eventsEnd,
                          const BoundingBox3f &bbox, Size depth,
                          Size badRefines, bool leftChild = true) {
            const Derived &derived = m_ctx.derived;

            /* Initialize the tree cost model */
            CostModel model(derived.costModel());
            model.setBoundingBox(bbox);
            Scalar leafCost = model.leafCost(primCount);

            /* ==================================================================== */
            /*                           Stopping criteria                          */
            /* ==================================================================== */

            if (primCount <= derived.stopPrimitives() || depth >= derived.maxDepth()) {
                makeLeaf(node, primCount, eventsStart, eventsEnd);
                return leafCost;
            }

            /* ==================================================================== */
            /*                        Split candidate search                        */
            /* ==================================================================== */

            /* First, find the optimal splitting plane according to the
               tree construction heuristic. To do this in O(n), the search is
               implemented as a sweep over the edge events */

            /* Initially, the split plane is placed left of the scene
               and thus all geometry is on its right side */
            Size leftCount[Dimension], rightCount[Dimension];
            for (int i=0; i<Dimension; ++i) {
                leftCount[i] = 0;
                rightCount[i] = primCount;
            }

            /* Keep track of where events for different axes start */
            EdgeEvent* eventsByDimension[Dimension + 1] { };
            eventsByDimension[0] = eventsStart;
            eventsByDimension[Dimension] = eventsEnd;

            /* Iterate over all events and find the best split plane */
            SplitCandidate best;
            for (auto event = eventsStart; event != eventsEnd; ) {
                /* Record the current position and count the number
                   and type of remaining events that are also here. */
                Size numStart = 0, numEnd = 0, numPlanar = 0;
                int axis = event->axis;
                Scalar pos = event->pos;

                while (event < eventsEnd && event->pos == pos && event->axis == axis) {
                    switch (event->type) {
                        case EdgeEvent::EEdgeStart:  ++numStart;  break;
                        case EdgeEvent::EEdgePlanar: ++numPlanar; break;
                        case EdgeEvent::EEdgeEnd:    ++numEnd;    break;
                    }
                    ++event;
                }

                /* Keep track of the beginning of each dimension */
                if (event < eventsEnd && event->axis != axis)
                    eventsByDimension[event->axis] = event;

                /* The split plane can now be moved onto 't'. Accordingly, all planar
                   and ending primitives are removed from the right side */
                rightCount[axis] -= numPlanar + numEnd;

                /* Check if the edge event is out of bounds -- when primitive
                   clipping is active, this should never happen! */
                Assert(!(derived.clipPrimitives() &&
                         (pos < bbox.min[axis] || pos > bbox.max[axis])));

                /* Calculate a score using the tree construction heuristic */
                if (likely(pos > bbox.min[axis] && pos < bbox.max[axis])) {
                    Size numLeft = leftCount[axis] + numPlanar,
                         numRight = rightCount[axis];

                    Scalar cost = model.innerCost(
                        axis, pos, model.leafCost(numLeft),
                        model.leafCost(numRight));

                    if (cost < best.cost) {
                        best.cost = cost;
                        best.split = pos;
                        best.axis = axis;
                        best.leftCount = numLeft;
                        best.rightCount = numRight;
                        best.planarLeft = true;
                    }

                    if (numPlanar != 0) {
                        /* There are planar events here -- also consider
                           placing them on the right side */
                        numLeft = leftCount[axis];
                        numRight = rightCount[axis] + numPlanar;

                        cost = model.innerCost(
                            axis, pos, model.leafCost(numLeft),
                            model.leafCost(numRight));

                        if (cost < best.cost) {
                            best.cost = cost;
                            best.split = pos;
                            best.axis = axis;
                            best.leftCount = numLeft;
                            best.rightCount = numRight;
                            best.planarLeft = false;
                        }
                    }
                }

                /* The split plane is moved past 't'. All prims,
                    which were planar on 't', are moved to the left
                    side. Also, starting prims are now also left of
                    the split plane. */
                leftCount[axis] += numStart + numPlanar;
            }

            /* Sanity checks. Everything should now be left of the split plane */
            for (int i=0; i<Dimension; ++i) {
                Assert(rightCount[i] == 0 && leftCount[i] == primCount);
                Assert(eventsByDimension[i] != eventsEnd && eventsByDimension[i]->axis == i);
                Assert((i == 0) || ((eventsByDimension[i]-1)->axis == i - 1));
            }

            /* Allow a few bad refines in sequence before giving up */
            if (best.cost >= leafCost) {
                if ((best.cost > 4 * leafCost && primCount < 16)
                    || badRefines >= derived.maxBadRefines()
                    || !std::isfinite(best.cost)) {
                    makeLeaf(node, primCount, eventsStart, eventsEnd);
                    return leafCost;
                }
                ++badRefines;
                m_ctx.badRefines++;
            }

            /* ==================================================================== */
            /*                      Primitive Classification                        */
            /* ==================================================================== */

            auto &classification = m_local->classificationStorage;

            /* Initially mark all prims as being located on both sides */
            for (auto event = eventsByDimension[best.axis];
                 event != eventsByDimension[best.axis + 1]; ++event)
                classification.set(event->index, EPrimClassification::EBoth);

            Size primsLeft = 0, primsRight = 0;
            for (auto event = eventsByDimension[best.axis];
                 event != eventsByDimension[best.axis + 1]; ++event) {

                if (event->type == EdgeEvent::EEdgeEnd &&
                    event->pos <= best.split) {
                    /* Fully on the left side (the primitive's interval ends
                       before (or on) the split plane) */
                    Assert(classification.get(event->index) == EPrimClassification::EBoth);
                    classification.set(event->index, EPrimClassification::ELeft);
                    primsLeft++;
                } else if (event->type == EdgeEvent::EEdgeStart &&
                           event->pos >= best.split) {
                    /* Fully on the right side (the primitive's interval
                       starts after (or on) the split plane) */
                    Assert(classification.get(event->index) == EPrimClassification::EBoth);
                    classification.set(event->index, EPrimClassification::ERight);
                    primsRight++;
                } else if (event->type == EdgeEvent::EEdgePlanar) {
                    /* If the planar primitive is not on the split plane,
                       the classification is easy. Otherwise, place it on
                       the side with the lower cost */
                    Assert(classification.get(event->index) == EPrimClassification::EBoth);
                    if (event->pos < best.split ||
                        (event->pos == best.split && best.planarLeft)) {
                        classification.set(event->index, EPrimClassification::ELeft);
                        primsLeft++;
                    } else if (event->pos > best.split ||
                               (event->pos == best.split && !best.planarLeft)) {
                        classification.set(event->index, EPrimClassification::ERight);
                        primsRight++;
                    }
                }
            }

            Size primsBoth = primCount - primsLeft - primsRight;

            /* Some sanity checks */
            Assert(primsLeft + primsBoth == best.leftCount);
            Assert(primsRight + primsBoth == best.rightCount);

            /* ==================================================================== */
            /*                            Partitioning                              */
            /* ==================================================================== */

            BoundingBox leftBBox = bbox, rightBBox = bbox;
            leftBBox.max[best.axis] = best.split;
            rightBBox.min[best.axis] = best.split;

            Size prunedLeft = 0, prunedRight = 0;

            auto &leftAlloc = m_local->leftAlloc;
            auto &rightAlloc = m_local->rightAlloc;

            EdgeEvent *leftEventsStart, *rightEventsStart,
                      *leftEventsEnd, *rightEventsEnd;

            /* First, allocate a conservative amount of scratch space for
               the final event lists and then resize it to the actual used
               amount */
            if (leftChild) {
                leftEventsStart = eventsStart;
                rightEventsStart = rightAlloc.template allocate<EdgeEvent>(
                    best.rightCount * 2 * Dimension);
            } else {
                leftEventsStart = leftAlloc.template allocate<EdgeEvent>(
                    best.leftCount * 2 * Dimension);
                rightEventsStart = eventsStart;
            }
            leftEventsEnd = leftEventsStart;
            rightEventsEnd = rightEventsStart;

            if (primsBoth == 0 || !derived.clipPrimitives()) {
                /* Fast path: no clipping needed. */
                for (auto it = eventsStart; it != eventsEnd; ++it) {
                    auto event = *it;

                    /* Fetch the classification of the current event */
                    switch (classification.get(event.index)) {
                        case EPrimClassification::ELeft:
                            *leftEventsEnd++ = event;
                            break;

                        case EPrimClassification::ERight:
                            *rightEventsEnd++ = event;
                            break;

                        case EPrimClassification::EBoth:
                            *leftEventsEnd++ = event;
                            *rightEventsEnd++ = event;
                            break;

                        default:
                            Assert(false);
                    }
                }

                Assert((Size) (leftEventsEnd - leftEventsStart) <= best.leftCount* 2 * Dimension);
                Assert((Size) (rightEventsEnd - rightEventsStart) <= best.rightCount * 2 * Dimension);
            } else {
                /* Slow path: some primitives are straddling the split plane
                   and primitive clipping is enabled. They will generate new
                   events that have to be sorted and merged into the current
                   sorted event lists. Start by allocating some more scratch
                   space for this.. */
                EdgeEvent *tempLeftEventsStart, *tempLeftEventsEnd,
                    *tempRightEventsStart, *tempRightEventsEnd,
                    *newLeftEventsStart, *newLeftEventsEnd,
                    *newRightEventsStart, *newRightEventsEnd;

                tempLeftEventsStart = tempLeftEventsEnd =
                    leftAlloc.template allocate<EdgeEvent>(primsLeft * 2 * Dimension);
                tempRightEventsStart = tempRightEventsEnd =
                    rightAlloc.template allocate<EdgeEvent>(primsRight * 2 * Dimension);
                newLeftEventsStart = newLeftEventsEnd =
                    leftAlloc.template allocate<EdgeEvent>(primsBoth * 2 * Dimension);
                newRightEventsStart = newRightEventsEnd =
                    rightAlloc.template allocate<EdgeEvent>(primsBoth * 2 * Dimension);

                for (auto it = eventsStart; it != eventsEnd; ++it) {
                    auto event = *it;

                    /* Fetch the classification of the current event */
                    switch (classification.get(event.index)) {
                        case EPrimClassification::ELeft:
                            *tempLeftEventsEnd++ = event;
                            break;

                        case EPrimClassification::ERight:
                            *tempRightEventsEnd++ = event;
                            break;

                        case EPrimClassification::EIgnore:
                            break;

                        case EPrimClassification::EBoth: {
                                BoundingBox clippedLeft  = derived.bbox(event.index, leftBBox);
                                BoundingBox clippedRight = derived.bbox(event.index, rightBBox);

                                Assert(leftBBox.contains(clippedLeft) || !clippedLeft.valid());
                                Assert(rightBBox.contains(clippedRight) || !clippedRight.valid());

                                if (clippedLeft.valid() &&
                                    clippedLeft.surfaceArea() > 0) {
                                    for (int axis = 0; axis < Dimension; ++axis) {
                                        Scalar min = clippedLeft.min[axis],
                                               max = clippedLeft.max[axis];

                                        if (min != max) {
                                            *newLeftEventsEnd++ = EdgeEvent(
                                                EdgeEvent::EEdgeStart, axis, min, event.index);
                                            *newLeftEventsEnd++ = EdgeEvent(
                                                EdgeEvent::EEdgeEnd, axis, max, event.index);
                                        } else {
                                            *newLeftEventsEnd++ = EdgeEvent(
                                                EdgeEvent::EEdgePlanar, axis, min, event.index);
                                        }
                                    }
                                } else {
                                    prunedLeft++;
                                }

                                if (clippedRight.valid() &&
                                    clippedRight.surfaceArea() > 0) {
                                    for (int axis = 0; axis < Dimension; ++axis) {
                                        Scalar min = clippedRight.min[axis],
                                               max = clippedRight.max[axis];

                                        if (min != max) {
                                            *newRightEventsEnd++ = EdgeEvent(
                                                EdgeEvent::EEdgeStart, axis, min, event.index);
                                            *newRightEventsEnd++ = EdgeEvent(
                                                EdgeEvent::EEdgeEnd, axis, max, event.index);
                                        } else {
                                            *newRightEventsEnd++ = EdgeEvent(
                                                EdgeEvent::EEdgePlanar, axis, min, event.index);
                                        }
                                    }
                                } else {
                                    prunedRight++;
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

                Assert((Size) (tempLeftEventsEnd - tempLeftEventsStart) <= primsLeft * 2 * Dimension);
                Assert((Size) (tempRightEventsEnd - tempRightEventsStart) <= primsRight * 2 * Dimension);
                Assert((Size) (newLeftEventsEnd - newLeftEventsStart) <= primsBoth * 2 * Dimension);
                Assert((Size) (newRightEventsEnd - newRightEventsStart) <= primsBoth * 2 * Dimension);

                m_ctx.pruned += prunedLeft + prunedRight;

                /* Sort the events due to primitives which overlap the split plane */
                std::sort(newLeftEventsStart, newLeftEventsEnd);
                std::sort(newRightEventsStart, newRightEventsEnd);

                /* Merge the left list */
                leftEventsEnd = std::merge(tempLeftEventsStart,
                    tempLeftEventsEnd, newLeftEventsStart, newLeftEventsEnd,
                    leftEventsStart);

                /* Merge the right list */
                rightEventsEnd = std::merge(tempRightEventsStart,
                    tempRightEventsEnd, newRightEventsStart, newRightEventsEnd,
                    rightEventsStart);

                /* Release temporary memory */
                leftAlloc.release(newLeftEventsStart);
                rightAlloc.release(newRightEventsStart);
                leftAlloc.release(tempLeftEventsStart);
                rightAlloc.release(tempRightEventsStart);
            }

            /* Shrink the edge event storage now that we know exactly how
               many events are on each side */
            leftAlloc.shrinkAllocation(leftEventsStart,
                                       leftEventsEnd - leftEventsStart);
            rightAlloc.shrinkAllocation(rightEventsStart,
                                        rightEventsEnd - rightEventsStart);

            /* ==================================================================== */
            /*                              Recursion                               */
            /* ==================================================================== */

            NodeIterator children = m_ctx.nodeStorage.grow_by(2);

            Size leftOffset(std::distance(node, children));

            if (!node->setInnerNode(best.axis, best.split, leftOffset))
                Throw("Internal error during kd-tree construction: unable "
                      "to store overly large offset to left child node (%i)",
                      leftOffset);
            if (node->left() == &*node || node->right() == &*node)
                Throw("Internal error..");

            Scalar leftCost =
                buildNLogN(children, best.leftCount - prunedLeft,
                           leftEventsStart, leftEventsEnd, leftBBox,
                           depth + 1, badRefines, true);

            Scalar rightCost =
                buildNLogN(std::next(children), best.rightCount - prunedRight,
                           rightEventsStart, rightEventsEnd, rightBBox,
                           depth + 1, badRefines, false);

            /* Release the index lists not needed by the children anymore */
            if (leftChild)
                rightAlloc.release(rightEventsStart);
            else
                leftAlloc.release(leftEventsStart);

            /* ==================================================================== */
            /*                           Final decision                             */
            /* ==================================================================== */

            Scalar finalCost =
                model.innerCost(best.axis, best.split, leftCost, rightCost);

            /* Tear up bad (i.e. costly) subtrees and replace them with leaf nodes */
            if (unlikely(finalCost > leafCost && derived.retractBadSplits())) {
                std::unordered_set<Index> temp;
                traverse(node, temp);

                auto it = m_ctx.indexStorage.grow_by(temp.begin(), temp.end());
                Size offset(std::distance(m_ctx.indexStorage.begin(), it));

                if (!node->setLeafNode(offset, temp.size()))
                    Throw("Internal error: could not create leaf node with %i "
                          "primitives -- too much geometry?", m_indices.size());

                m_ctx.retractedSplits++;
                return leafCost;
            }

            return finalCost;
        }

        /// Create an initial sorted edge event list and start the O(N log N) builder
        Scalar transitionToNLogN() {
            const auto &derived = m_ctx.derived;
            m_local = &((LocalBuildContext &) m_ctx.local);

            Size primCount = Size(m_indices.size()), finalPrimCount = primCount;

            /* We don't yet know how many edge events there will be. Allocate a
               conservative amount and shrink the buffer later on. */
            Size initialSize = primCount * 2 * Dimension;

            EdgeEvent *eventsStart =
                m_local->leftAlloc.template allocate<EdgeEvent>(initialSize),
                *eventsEnd = eventsStart + initialSize;

            for (Size i = 0; i<primCount; ++i) {
                Index primIndex = m_indices[i];
                BoundingBox primBBox = derived.bbox(primIndex, m_bbox);
                bool valid = primBBox.valid() && primBBox.surfaceArea() > 0;

                if (unlikely(!valid)) {
                    --finalPrimCount;
                    m_ctx.pruned++;
                }

                for (int axis = 0; axis < Dimension; ++axis) {
                    Scalar min = primBBox.min[axis], max = primBBox.max[axis];
                    Index offset = (axis * primCount + i) * 2;

                    if (unlikely(!valid)) {
                        eventsStart[offset  ].setInvalid();
                        eventsStart[offset+1].setInvalid();
                    } else if (min == max) {
                        eventsStart[offset  ] = EdgeEvent(EdgeEvent::EEdgePlanar, axis, min, primIndex);
                        eventsStart[offset+1].setInvalid();
                    } else {
                        eventsStart[offset  ] = EdgeEvent(EdgeEvent::EEdgeStart, axis, min, primIndex);
                        eventsStart[offset+1] = EdgeEvent(EdgeEvent::EEdgeEnd,   axis, max, primIndex);
                    }
                }
            }

            /* Release index list */
            IndexVector().swap(m_indices);

            /* Sort the events list and remove invalid ones from the end */
            std::sort(eventsStart, eventsEnd);
            while (eventsStart != eventsEnd && !(eventsEnd-1)->valid())
                --eventsEnd;

            m_local->leftAlloc.template shrinkAllocation<EdgeEvent>(
                eventsStart, eventsEnd - eventsStart);
            m_local->classificationStorage.resize(derived.primitiveCount());
            m_local->ctx = &m_ctx;

            Float cost = buildNLogN(m_node, finalPrimCount, eventsStart,
                                    eventsEnd, m_bbox, m_depth, 0);

            m_local->leftAlloc.release(eventsStart);

            return cost;
        }

        /// Create a leaf node using the given set of indices (called by min-max binning)
        template <typename T> void makeLeaf(T &&indices) {
            auto it = m_ctx.indexStorage.grow_by(indices.begin(), indices.end());
            Size offset(std::distance(m_ctx.indexStorage.begin(), it));

            if (!m_node->setLeafNode(offset, indices.size()))
                Throw("Internal error: could not create leaf node with %i "
                      "primitives -- too much geometry?", m_indices.size());

            *m_cost = m_ctx.derived.costModel().leafCost(Size(indices.size()));
        }

        /// Create a leaf node using the given edge event list (called by the O(N log N) builder)
        void makeLeaf(NodeIterator node, Size primCount, EdgeEvent *eventsStart,
                      EdgeEvent *eventsEnd) const {
            auto it = m_ctx.indexStorage.grow_by(primCount);
            Size offset(std::distance(m_ctx.indexStorage.begin(), it));

            if (!node->setLeafNode(offset, primCount))
                Throw("Internal error: could not create leaf node with %i "
                      "primitives -- too much geometry?", primCount);

            for (auto event = eventsStart; event != eventsEnd; ++event) {
                if (event->axis != 0)
                    break;
                if (event->type == EdgeEvent::EEdgeStart ||
                    event->type == EdgeEvent::EEdgePlanar) {
                    Assert(--primCount >= 0);
                    *it++ = event->index;
                }
            }

            Assert(primCount == 0);
        }

        /// Traverse a subtree and collect all encountered primitive references in a set
        void traverse(NodeIterator node, std::unordered_set<Index> &result) {
            if (node->leaf()) {
                for (Size i = 0; i < node->primitiveCount(); ++i)
                    result.insert(m_ctx.indexStorage[node->primitiveOffset() + i]);
            } else {
                NodeIterator leftChild = node + node->leftOffset(),
                             rightChild = leftChild + 1;
                traverse(leftChild, result);
                traverse(rightChild, result);
            }
        }
    };

    void computeStatistics(BuildContext &ctx, const KDNode *node,
                           const BoundingBox &bbox, Size depth) {
        if (depth > ctx.maxDepth)
            ctx.maxDepth = depth;

        if (node->leaf()) {
            auto primCount = node->primitiveCount();
            double value = (double) CostModel::eval(bbox);

            ctx.expLeavesVisited += value;
            ctx.expPrimitivesQueried += value * double(primCount);
            if (primCount < sizeof(ctx.primBuckets) / sizeof(Size))
                ctx.primBuckets[primCount]++;
            if (primCount > ctx.maxPrimsInLeaf)
                ctx.maxPrimsInLeaf = primCount;
            if (primCount > 0)
                ctx.nonEmptyLeafCount++;
        } else {
            ctx.expTraversalSteps += (double) CostModel::eval(bbox);

            int axis = node->axis();
            Scalar split = Scalar(node->split());
            BoundingBox leftBBox(bbox), rightBBox(bbox);
            leftBBox.max[axis] = split;
            rightBBox.min[axis] = split;
            computeStatistics(ctx, node->left(), leftBBox, depth + 1);
            computeStatistics(ctx, node->right(), rightBBox, depth + 1);
        }
    }

    void build() {
        /* Some sanity checks */
        if (ready())
            Throw("The kd-tree has already been built!");
        if (m_minMaxBins <= 1)
            Throw("The number of min-max bins must be > 2");
        if (m_stopPrimitives <= 0)
            Throw("The stopping primitive count must be greater than zero");
        if (m_exactPrimThreshold <= m_stopPrimitives)
            Throw("The exact primitive threshold must be bigger than the "
                  "stopping primitive count");

        Size primCount = derived().primitiveCount();
        if (m_maxDepth == 0)
            m_maxDepth = (int) (8 + 1.3f * math::log2i(primCount));
        m_maxDepth = std::min(m_maxDepth, (Size) MTS_KD_MAXDEPTH);

        Log(m_logLevel, "");

        Log(m_logLevel, "kd-tree configuration:");
        Log(m_logLevel, "   Cost model               : %s", m_costModel);
        Log(m_logLevel, "   Max. tree depth          : %i", m_maxDepth);
        Log(m_logLevel, "   Scene bounding box (min) : %s", m_bbox.min);
        Log(m_logLevel, "   Scene bounding box (max) : %s", m_bbox.max);
        Log(m_logLevel, "   Min-max bins             : %i", m_minMaxBins);
        Log(m_logLevel, "   O(n log n) method        : use for <= %i primitives",
            m_exactPrimThreshold);
        Log(m_logLevel, "   Stopping primitive count : %i", m_stopPrimitives);
        Log(m_logLevel, "   Perfect splits           : %s",
            m_clipPrimitives ? "yes" : "no");
        Log(m_logLevel, "   Retract bad splits       : %s",
            m_retractBadSplits ? "yes" : "no");
        Log(m_logLevel, "");

        /* ==================================================================== */
        /*              Create build context and preallocate memory             */
        /* ==================================================================== */

        BuildContext ctx(derived());

        ctx.nodeStorage.reserve(primCount);
        ctx.indexStorage.reserve(primCount);

        ctx.nodeStorage.grow_by(1);

        /* ==================================================================== */
        /*                      Build the tree in parallel                      */
        /* ==================================================================== */

        Scalar finalCost = 0;
        if (primCount == 0) {
            Log(EWarn, "kd-tree contains no geometry!");
            ctx.nodeStorage[0].setLeafNode(0, 0);
        } else {

            Log(m_logLevel, "Creating a preliminary index list (%s)",
                util::memString(primCount * sizeof(Index)).c_str());

            IndexVector indices(primCount);
            for (size_t i = 0; i < primCount; ++i)
                indices[i] = i;

            BuildTask &task = *new (tbb::task::allocate_root()) BuildTask(
                ctx, ctx.nodeStorage.begin(), std::move(indices),
                m_bbox, m_bbox, 0, 0, &finalCost);

            tbb::task::spawn_root_and_wait(task);
        }

        Log(m_logLevel, "Structural kd-tree statistics:");

        /* ==================================================================== */
        /*     Store the node and index lists in a compact contiguous format    */
        /* ==================================================================== */

        m_nodeCount = Size(ctx.nodeStorage.size());
        m_indexCount = Size(ctx.indexStorage.size());

        m_indices.reset(AlignedAllocator::alloc<Index>(m_indexCount));
        tbb::parallel_for(
            tbb::blocked_range<Size>(0u, m_indexCount, MTS_KD_GRAIN_SIZE),
            [&](const tbb::blocked_range<Size> &range) {
                for (Size i = range.begin(); i != range.end(); ++i)
                    m_indices[i] = ctx.indexStorage[i];
            }
        );

        tbb::concurrent_vector<Index>().swap(ctx.indexStorage);

        m_nodes.reset(AlignedAllocator::alloc<KDNode>(m_nodeCount));
        tbb::parallel_for(
            tbb::blocked_range<Size>(0u, m_nodeCount, MTS_KD_GRAIN_SIZE),
            [&](const tbb::blocked_range<Size> &range) {
                for (Size i = range.begin(); i != range.end(); ++i)
                    m_nodes[i] = ctx.nodeStorage[i];
            }
        );
        tbb::concurrent_vector<KDNode>().swap(ctx.nodeStorage);

        /* ==================================================================== */
        /*         Print various tree statistics if requested by the user       */
        /* ==================================================================== */

        if (Thread::thread()->logger()->logLevel() <= m_logLevel) {
            computeStatistics(ctx, m_nodes.get(), m_bbox, 0);

            // Trigger per-thread data release
            ctx.local.clear();

            ctx.expTraversalSteps /= (double) CostModel::eval(m_bbox);
            ctx.expLeavesVisited /= (double) CostModel::eval(m_bbox);
            ctx.expPrimitivesQueried /= (double) CostModel::eval(m_bbox);
            ctx.tempStorage += ctx.nodeStorage.size() * sizeof(KDNode);
            ctx.tempStorage += ctx.indexStorage.size() * sizeof(Index);

            Log(m_logLevel, "   Primitive references        : %i (%s)",
                m_indexCount, util::memString(m_indexCount * sizeof(Index)));

            Log(m_logLevel, "   kd-tree nodes               : %i (%s)",
                m_nodeCount, util::memString(m_nodeCount * sizeof(KDNode)));

            Log(m_logLevel, "   kd-tree depth               : %i",
                ctx.maxDepth);

            Log(m_logLevel, "   Temporary storage used      : %s",
                util::memString(ctx.tempStorage));

            Log(m_logLevel, "   Parallel work units         : %i",
                ctx.workUnits);

            std::ostringstream oss;
            Size primBucketCount = sizeof(ctx.primBuckets) / sizeof(Size);
            oss << "   Leaf node histogram         : ";
            for (Size i = 0; i < primBucketCount; i++) {
                oss << i << "(" << ctx.primBuckets[i] << ") ";
                if ((i + 1) % 4 == 0 && i + 1 < primBucketCount) {
                    Log(m_logLevel, "%s", oss.str());
                    oss.str("");
                    oss << "                                 ";
                }
            }
            Log(m_logLevel, "%s", oss.str().c_str());
            Log(m_logLevel, "");

            Log(m_logLevel, "Qualitative kd-tree statistics:");
            Log(m_logLevel, "   Retracted splits            : %i",
                ctx.retractedSplits);
            Log(m_logLevel, "   Bad refines                 : %i",
                ctx.badRefines);
            Log(m_logLevel, "   Pruned                      : %i",
                ctx.pruned);
            Log(m_logLevel, "   Largest leaf node           : %i primitives",
                ctx.maxPrimsInLeaf);
            Log(m_logLevel, "   Avg. prims/nonempty leaf    : %.2f",
                m_indexCount / (Scalar) ctx.nonEmptyLeafCount);
            Log(m_logLevel, "   Expected traversals/query   : %.2f",
                ctx.expTraversalSteps);
            Log(m_logLevel, "   Expected leaf visits/query  : %.2f",
                ctx.expLeavesVisited);
            Log(m_logLevel, "   Expected prim. visits/query : %.2f",
                ctx.expPrimitivesQueried);
            Log(m_logLevel, "   Final cost                  : %.2f",
                finalCost);
        }

        #if defined(__LINUX__)
            /* Forcefully release Heap memory back to the OS */
            malloc_trim(0);
        #endif
    }

protected:
    std::unique_ptr<KDNode[], AlignedAllocator> m_nodes;
    std::unique_ptr<Index[],  AlignedAllocator> m_indices;
    Size m_nodeCount = 0;
    Size m_indexCount = 0;

    CostModel m_costModel;
    bool m_clipPrimitives = true;
    bool m_retractBadSplits = true;
    Size m_maxDepth = 0;
    Size m_stopPrimitives = 3;
    Size m_maxBadRefines = 0;
    Size m_exactPrimThreshold = 65536;
    Size m_minMaxBins = 128;
    ELogLevel m_logLevel = EDebug;
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

    SurfaceAreaHeuristic3f(Float queryCost, Float traversalCost,
                           Float emptySpaceBonus)
        : m_queryCost(queryCost), m_traversalCost(traversalCost),
          m_emptySpaceBonus(emptySpaceBonus) {
        if (m_queryCost <= 0)
            Throw("The query cost must be > 0");
        if (m_traversalCost <= 0)
            Throw("The traveral cost must be > 0");
        if (m_emptySpaceBonus <= 0 || m_emptySpaceBonus > 1)
            Throw("The empty space bonus must be in [0, 1]");
    }

    /**
     * \brief Return the query cost used by the tree construction heuristic
     *
     * (This is the average cost for testing a shape against a kd-tree query)
     */
    Float queryCost() const { return m_queryCost; }

    /// Get the cost of a traversal operation used by the tree construction heuristic
    Float traversalCost() const { return m_traversalCost; }

    /**
     * \brief Return the bonus factor for empty space used by the
     * tree construction heuristic
     */
    Float emptySpaceBonus() const { return m_emptySpaceBonus; }

    /**
     * \brief Initialize the surface area heuristic with the bounds of
     * a parent node
     *
     * Precomputes some information so that traversal probabilities
     * of potential split planes can be evaluated efficiently
     */
    void setBoundingBox(const BoundingBox3f &bbox) {
        auto extents = bbox.extents();
        Float temp = 2.f / bbox.surfaceArea();
        auto a = simd::shuffle<1, 2, 0>(extents);
        auto b = simd::shuffle<2, 0, 1>(extents);
        m_temp0 = m_temp1 = (a * b) * temp;
        m_temp2 = (a + b) * temp;
        m_temp0 -= m_temp2 * Vector3f(bbox.min);
        m_temp1 += m_temp2 * Vector3f(bbox.max);
    }

    /// \brief Evaluate the cost of a leaf node
    Float leafCost(Size nElements) const {
        return m_queryCost * nElements;
    }

    /**
     * \brief Evaluate the surface area heuristic
     *
     * Given a split on axis \a axis that produces children having extents \a
     * leftWidth and \a rightWidth along \a axis, compute the probability of
     * traversing the left and right child during a typical query operation. In
     * the case of the surface area heuristic, this is simply the ratio of
     * surface areas.
     */
    Float innerCost(int axis, Float split, Float leftCost, Float rightCost) const {
        Float leftProb  = m_temp0[axis] + m_temp2[axis] * split;
        Float rightProb = m_temp1[axis] - m_temp2[axis] * split;

        Float cost = m_traversalCost +
            (leftProb * leftCost + rightProb * rightCost);

        if (unlikely(leftCost == 0 || rightCost == 0))
            cost *= m_emptySpaceBonus;

        return cost;
    }

    static Float eval(const BoundingBox3f &bbox) {
        return bbox.surfaceArea();

    }

    friend std::ostream &operator<<(std::ostream &os, const SurfaceAreaHeuristic3f &sa) {
        os << "SurfaceAreaHeuristic3f["
           << "queryCost=" << sa.queryCost() << ", "
           << "traversalCost=" << sa.traversalCost() << ", "
           << "emptySpaceBonus=" << sa.emptySpaceBonus() << "]";
        return os;
    }
private:
    Vector3f m_temp0, m_temp1, m_temp2;
    Float m_queryCost;
    Float m_traversalCost;
    Float m_emptySpaceBonus;
};

class MTS_EXPORT ShapeKDTree
    : public TShapeKDTree<BoundingBox3f, uint32_t, SurfaceAreaHeuristic3f, ShapeKDTree> {
public:
    using Base = TShapeKDTree<BoundingBox3f, uint32_t, SurfaceAreaHeuristic3f, ShapeKDTree>;
    using Base::Scalar;

    ShapeKDTree(const Properties &props);
    void addShape(Shape *shape);

    Size primitiveCount() const { return m_primitiveMap.back(); }
    Size shapeCount() const { return Size(m_shapes.size()); }

    void build() {
        Timer timer;
        Log(EInfo, "Building a SAH kd-tree (%i primitives) ..",
            primitiveCount());

        Base::build();

        Log(EInfo, "Finished. (%s of storage, took %s)",
            util::memString(m_indexCount * sizeof(Index) +
                            m_nodeCount * sizeof(KDNode)),
            util::timeString(timer.value())
        );
    }

    /// Return the i-th shape
    const Shape *shape(size_t i) const { Assert(i < m_shapes.size()); return m_shapes[i]; }

    /// Return the i-th shape
    Shape *shape(size_t i) { Assert(i < m_shapes.size()); return m_shapes[i]; }

    /// Return the bounding box of the i-th primitive
    BoundingBox3f bbox(Index i) const {
        Index shapeIndex = findShape(i);
        return m_shapes[shapeIndex]->bbox(i);
    }

    /// Return the (clipped) bounding box of the i-th primitive
    BoundingBox3f bbox(Index i, const BoundingBox3f &clip) const {
        Index shapeIndex = findShape(i);
        return m_shapes[shapeIndex]->bbox(i, clip);
    }

    /// Return a human-readable string representation of the scene contents.
    virtual std::string toString() const override;

    MTS_DECLARE_CLASS()
protected:
    /**
     * \brief Map an abstract \ref TShapeKDTree primitive index to a specific
     * shape managed by the \ref ShapeKDTree.
     *
     * The function returns the shape index and updates the \a idx parameter to
     * point to the primitive index (e.g. triangle ID) within the shape.
     */
    Index findShape(Index &i) const {
        Assert(i < primitiveCount());

        Index shapeIndex = math::findInterval(
            Size(0),
            Size(m_primitiveMap.size()),
            [&](Index k) {
                return m_primitiveMap[k] <= i;
            }
        );
        Assert(shapeIndex < shapeCount() &&
               m_primitiveMap.size() == shapeCount() + 1);

        Assert(i >= m_primitiveMap[shapeIndex]);
        Assert(i <  m_primitiveMap[shapeIndex + 1]);

        i -= m_primitiveMap[shapeIndex];

        return shapeIndex;
    }

protected:
    std::vector<ref<Shape>> m_shapes;
    std::vector<Size> m_primitiveMap;
};

NAMESPACE_END(mitsuba)
