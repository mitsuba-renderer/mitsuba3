#pragma once

#include <mitsuba/core/logger.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/render/shape.h>
#include <tbb/tbb.h>

/// Compile-time KD-tree depth limit to enable traversal with stack memory
#define MTS_KD_MAXDEPTH 48u

/// Number of bins used for Min-Max binning
#define MTS_KD_BINS 256u

/// OrderedChunkAllocator: don't create chunks smaller than 512 KiB
#define MTS_KD_MIN_ALLOC 512u*1024u

/// Grain size for TBB parallelization
#define MTS_KD_GRAIN_SIZE 10240u

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(detail)

/**
 * \brief Ordered memory allocator
 *
 * During kd-tree construction, large amounts of memory are required to
 * temporarily hold index and edge event lists. When not implemented properly,
 * these allocations can become a critical bottleneck. The class \ref
 * OrderedChunkAllocator provides a specialized memory allocator, which
 * reserves memory in chunks of at least 512KiB (this number is configurable).
 * An important assumption made by the allocator is that memory will be
 * released in the exact same order in which it was previously allocated. This
 * makes it possible to create an implementation with a very low memory
 * overhead. Note that no locking is done, hence each thread will need its own
 * allocator.
 */
class OrderedChunkAllocator {
public:
    OrderedChunkAllocator(size_t minAllocation = MTS_KD_MIN_ALLOC)
            : m_minAllocation(minAllocation) {
        m_chunks.reserve(16);
    }

    ~OrderedChunkAllocator() {
        cleanup();
    }

    /**
     * \brief Release all memory used by the allocator
     */
    void cleanup() {
        for (auto &chunk: m_chunks)
            AlignedAllocator::dealloc(chunk.start);
        m_chunks.clear();
    }

    /**
     * \brief Merge the chunks of another allocator into this one
     */
    void merge(OrderedChunkAllocator &&other) {
        m_chunks.reserve(m_chunks.size() + other.m_chunks.size());
        m_chunks.insert(m_chunks.end(), other.m_chunks.begin(),
                other.m_chunks.end());
        other.m_chunks.clear();
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

        Chunk chunk;
        chunk.start = AlignedAllocator::alloc<uint8_t>(allocSize);
        chunk.cur = chunk.start + size;
        chunk.size = allocSize;
        m_chunks.push_back(chunk);

        return reinterpret_cast<T *>(chunk.start);
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
                if (ptr == chunk.start + chunk.size)
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
                    if (ptr == chunk.start + chunk.size)
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
    std::string toString() const {
        std::ostringstream oss;
        oss << "OrderedChunkAllocator[" << std::endl;
        for (size_t i = 0; i < m_chunks.size(); ++i)
            oss << "    Chunk " << i << ": " << m_chunks[i].toString()
                << std::endl;
        oss << "]";
        return oss.str();
    }

private:
    struct Chunk {
        size_t size;
        uint8_t *start, *cur;

        size_t used() const { return cur - start; }
        size_t remainder() const { return size - used(); }

        bool contains(uint8_t *ptr) const { return ptr >= start && ptr < start + size; }

        std::string toString() const {
            return tfm::format("0x%x-0x%x (size=%i, used=%i)", start,
                               start + size, size, used());
        }
    };

    size_t m_minAllocation;
    std::vector<Chunk> m_chunks;
};

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

    enum {
        Dimension = BoundingBox::Dimension
    };

    TShapeKDTree() { }

    /// Get the cost of a traversal operation used by the tree construction heuristic
    Float traversalCost() const { return m_traversalCost; }

    /// Set the cost of a traversal operation used by the tree construction heuristic
    void setTraversalCost(Float traversalCost) { m_traversalCost = traversalCost; }

    /**
     * \brief Return the query cost used by the tree construction heuristic
     * (This is the average cost for testing a contained shape against
     *  a kd-tree search query)
     */
    Float queryCost() const { return m_queryCost; }

    /**
     * \brief Set the query cost used by the tree construction heuristic
     *
     * This is the average cost for testing a contained shape against a kd-tree
     * search query
     */
    void setQueryCost(Float queryCost) { m_queryCost = queryCost; }

    /**
     * \brief Return the bonus factor for empty space used by the
     * tree construction heuristic
     */
    Float emptySpaceBonus() const { return m_emptySpaceBonus; }

    /**
     * \brief Set the bonus factor for empty space used by the
     * tree construction heuristic
     */
    void setEmptySpaceBonus(Float emptySpaceBonus) { m_emptySpaceBonus = emptySpaceBonus; }

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
    Size stopPrims() const { return m_stopPrims; }

    /**
     * \brief Set the number of primitives, at which recursion will
     * stop when building the tree.
     */
    void setStopPrims(Size stopPrims) { m_stopPrims = stopPrims; }

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

    void build() {
        /* Some samity checks */
        if (ready())
            Throw("The kd-tree has already been built!");
        if (m_traversalCost <= 0)
            Throw("The traveral cost must be > 0");
        if (m_queryCost <= 0)
            Throw("The query cost must be > 0");
        if (m_emptySpaceBonus <= 0 || m_emptySpaceBonus > 1)
            Throw("The empty space bonus must be in [0, 1]");
        if (m_minMaxBins <= 1)
            Throw("The number of min-max bins must be > 2");

        Log(m_logLevel, "Building tree");

        Size primCount = derived().primitiveCount();
        if (m_maxDepth == 0)
            m_maxDepth = (int) (8 + 1.3f * math::log2i(primCount));
        m_maxDepth = std::min(m_maxDepth, (Size) MTS_KD_MAXDEPTH);

        if (primCount == 0) {
            Log(EWarn, "kd-tree contains no geometry!");
            // +1 shift is for alignment purposes (see KDNode::getSibling)
            // XXX
            //m_nodes = static_cast<KDNode *>(allocAligned(sizeof(KDNode) * 2))+1;
            //m_nodes[0].initLeafNode(0, 0);
            return;
        }

        Log(m_logLevel, "Creating a preliminary index list (%s)",
            util::memString(primCount * sizeof(Index)).c_str());

        //OrderedChunkAllocator &leftAlloc = ctx.leftAlloc;
        //Index *indices = leftAlloc.allocate<Index>(primCount);

        Log(m_logLevel, "");

        Log(m_logLevel, "kd-tree configuration:");
        Log(m_logLevel, "   Traversal cost           : %.2f", m_traversalCost);
        Log(m_logLevel, "   Query cost               : %.2f", m_queryCost);
        Log(m_logLevel, "   Empty space bonus        : %.2f", m_emptySpaceBonus);
        Log(m_logLevel, "   Max. tree depth          : %i", m_maxDepth);
        Log(m_logLevel, "   Stopping primitive count : %i", m_stopPrims);
        Log(m_logLevel, "   Scene bounding box (min) : %s", m_bbox.min);
        Log(m_logLevel, "   Scene bounding box (max) : %s", m_bbox.max);
        Log(m_logLevel, "   Min-max bins             : %i", m_minMaxBins);
        Log(m_logLevel, "   O(n log n) method        : use for <= %i primitives",
            m_exactPrimThreshold);
        Log(m_logLevel, "   Perfect splits           : %s",
            m_clipPrimitives ? "yes" : "no");
        Log(m_logLevel, "   Retract bad splits       : %s",
            m_retractBadSplits ? "yes" : "no");
        Log(m_logLevel, "");

        //MinMaxBins bins(m_minMaxBins);

        /* ==================================================================== */
        /*                              Binning                                 */
        /* ==================================================================== */

        Timer timer;
        timer.beginStage("binning (par)");

        /* Accumulate all shapes into bins */
        MinMaxBins bins = tbb::parallel_reduce(
            tbb::blocked_range<Size>(0u, primCount, MTS_KD_GRAIN_SIZE),
            MinMaxBins(m_minMaxBins, m_bbox),

            /* MAP: Bin a number of shapes and return the resulting 'MinMaxBins' data structure */
            [&](const tbb::blocked_range<uint32_t> &range, MinMaxBins bins) {
                for (uint32_t i = range.begin(); i != range.end(); ++i)
                    bins.put(derived().bbox(i));
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

        Float leafCost = primCount * m_queryCost;

        auto bestSplit = bins.bestCandidate(
            primCount,
            m_traversalCost,
            m_queryCost,
            m_emptySpaceBonus
        );

#if 0
        /* "Bad refines" heuristic from PBRT */
        if (bestSplit.cost >= leafCost) {
            if ((bestSplit.cost > 4 * leafCost && primCount < 16)
                || badRefines >= m_maxBadRefines) {
                createLeaf(ctx, node, indices, primCount);
                return leafCost;
            }
            ++badRefines;
        }
#endif

        std::cout << bestSplit << std::endl;

        timer.endStage();
    }

    MTS_DECLARE_CLASS()
protected:
    /// Data type for split candidates suggested by the tree cost model
    struct SplitCandidate {
        Float cost = std::numeric_limits<Float>::infinity();
        float pos = 0;
        int axis = 0;
        Size numLeft = 0, numRight = 0;
        Size leftBin = 0;        /* used by min-max binning only */
        bool planarLeft = false; /* used by the O(n log n) builder only */

        friend std::ostream& operator<<(std::ostream &os, const SplitCandidate &c) {
            os << "SplitCandidate[" << std::endl
               << "  cost = " << c.cost << "," << std::endl
               << "  pos = " << c.pos << "," << std::endl
               << "  axis = " << c.axis << "," << std::endl
               << "  numLeft = " << c.numLeft << "," << std::endl
               << "  numRight = " << c.numRight << "," << std::endl
               << "  leftBin = " << c.leftBin << "," << std::endl
               << "  planarLeft = " << (c.planarLeft ? "yes" : "no") << std::endl
               << "]";
            return os;
        }
    };

    /**
     * \brief Min-max binning data structure
     *
     * See
     *   "Highly Parallel Fast KD-tree Construction for Interactive Ray Tracing of
     *   Dynamic Scenes" by M. Shevtsov, A. Soupikov and A. Kapustin
     */
    class MinMaxBins {
    public:
        MinMaxBins(Size binCount, const BoundingBox &bbox)
            : m_bins(AlignedAllocator::alloc<Size>(binCount * Dimension * 2)),
              m_binCount(binCount), m_bbox(bbox) {
            memset(m_bins.get(), 0, sizeof(Size) * binCount * Dimension * 2);
            m_maxBin = Vector::Constant(Scalar(binCount - 1));
            m_invBinSize = 1 / (bbox.extents() / (Float) binCount);
        }

        MinMaxBins(const MinMaxBins &other)
            : m_bins(AlignedAllocator::alloc<Size>(other.m_binCount * Dimension * 2)),
              m_binCount(other.m_binCount), m_invBinSize(other.m_invBinSize),
              m_maxBin(other.m_maxBin), m_bbox(other.m_bbox) {
            memcpy(m_bins.get(), other.m_bins.get(),
                   sizeof(Size) * m_binCount * Dimension * 2);
        }

        MinMaxBins(MinMaxBins &&other)
            : m_bins(std::move(other.m_bins)), m_binCount(other.m_binCount),
              m_invBinSize(other.m_invBinSize), m_maxBin(other.m_maxBin),
              m_bbox(other.m_bbox) {
            other.m_binCount = 0;
        }

        void operator=(const MinMaxBins &other) {
            if (m_binCount != other.binCount) {
                m_bins = std::unique_ptr<Size[], AlignedAllocator>(
                    AlignedAllocator::alloc<Size>(other.binCount * Dimension * 2));
                m_binCount = other.m_binCount;
            }
            memcpy(m_bins.get(), other.m_bins.get(),
                   sizeof(Size) * other.binCount * Dimension * 2);
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
            other.m_binCount = 0;
        }

        MTS_INLINE void put(const BoundingBox &bbox) {
            using simd::min;
            using simd::max;
            using Int = typename Vector::Int;

            alignas(alignof(Vector)) Int indexMin[Vector::ActualSize];
            alignas(alignof(Vector)) Int indexMax[Vector::ActualSize];

            Vector relMin = (bbox.min - m_bbox.min) * m_invBinSize;
            Vector relMax = (bbox.max - m_bbox.min) * m_invBinSize;

            relMin = min(max(relMin, Vector::Zero()), m_maxBin);
            relMax = min(max(relMax, Vector::Zero()), m_maxBin);

            floatToInt(relMin).store((float *) indexMin);
            floatToInt(relMax).store((float *) indexMax);

            Index offset = 0;
            for (Index axis = 0; axis < Dimension; ++axis) {
                m_bins[offset + indexMin[axis] * 2 + 0]++;
                m_bins[offset + indexMax[axis] * 2 + 1]++;
                offset += 2 * m_binCount;
            }
        }

        MinMaxBins& operator+=(const MinMaxBins &other) {
            /* Add bin counts (using SIMD instructions, please) */
            Size * __restrict__ b        = (Size *)       SIMD_ASSUME_ALIGNED(m_bins.get());
            const Size * __restrict__ bo = (const Size *) SIMD_ASSUME_ALIGNED(other.m_bins.get());

            SIMD_IVDEP /* No, the arrays don't alias */
            for (Size i = 0, size = m_binCount * Dimension * 2; i < size; ++i)
                b[i] += bo[i];
            return *this;
        }

        SplitCandidate bestCandidate(Size primCount,
                                     Float traversalCost,
                                     Float queryCost,
                                     Float emptySpaceBonus) const {
            Vector extents = m_bbox.extents();
            Vector binSize = extents / (Float) m_binCount;
            const uint32_t *bin = m_bins.get();
            CostModel model(m_bbox);
            SplitCandidate best;
            best.cost = primCount * queryCost;

            for (int axis = 0; axis < Dimension; ++axis) {
                SplitCandidate candidate;

                /* Initially: all primitives to the right, none on the left */
                candidate.numLeft = 0;
                candidate.numRight = primCount;
                candidate.axis = axis;

                Float leftWidth = 0, rightWidth = extents[axis];
                const Float step = binSize[axis];

                for (Index i = 0; i < m_binCount - 1; ++i) {
                    /* Move one bin to the right and

                       1. Increase numLeft by the number of primitives which
                          started in the bin (thus they at least overlap with
                          the left interval). This information is stored in the MIN
                          bin.

                       2. Reduce numRight by the number of primitives which
                          ended (thus they are entirely on the left). This
                          information is stored in the MAX bin.
                    */
                    candidate.numLeft  += *bin++; /* MIN-bin */
                    candidate.numRight -= *bin++; /* MAX-bin */
                    candidate.leftBin = i;

                    /* Update interval lengths */
                    leftWidth  += step;
                    rightWidth -= step;

                    /* Compute cost model */
                    std::pair<Float, Float> prob =
                        model(axis, leftWidth, rightWidth);

                    candidate.cost =
                        traversalCost +
                        queryCost * (prob.first * candidate.numLeft +
                                     prob.second * candidate.numRight);

                    if (unlikely(candidate.numLeft == 0 || candidate.numRight == 0))
                        candidate.cost *= emptySpaceBonus;

                    if (candidate.cost < best.cost)
                        best = candidate;
                }

                candidate.numLeft  += *bin++;
                candidate.numRight -= *bin++;

                Assert(candidate.numLeft == primCount);
                Assert(candidate.numRight == 0);
            }

            Assert(std::isfinite(best.cost) && best.leftBin >= 0);

            return best;
        }

#if 0
        /**
         * \brief Given a suitable split candidate, compute tight bounding
         * boxes for the left and right subtrees and return associated
         * primitive lists.
         */
        Partition partition(
                BuildContext &ctx, const Derived *derived, Index *primIndices,
                SplitCandidate &split, bool isLeftChild, Float traversalCost,
                Float queryCost) {
            SizeType numLeft = 0, numRight = 0;
            BoundingBox leftBounds, rightBounds;
            const int axis = split.axis;

            Index *leftIndices, *rightIndices;
            if (isLeftChild) {
                OrderedChunkAllocator &rightAlloc = ctx.rightAlloc;
                leftIndices = primIndices;
                rightIndices = rightAlloc.allocate<Index>(split.numRight);
            } else {
                OrderedChunkAllocator &leftAlloc = ctx.leftAlloc;
                leftIndices = leftAlloc.allocate<Index>(split.numLeft);
                rightIndices = primIndices;
            }

            for (SizeType i=0; i<m_primCount; ++i) {
                const Index primIndex = primIndices[i];
                const BoundingBox aabb = derived->getAABB(primIndex);
                int startIdx = computeIndex(math::castflt_down(aabb.min[axis]), axis);
                int endIdx   = computeIndex(math::castflt_up  (aabb.max[axis]), axis);

                if (endIdx <= split.leftBin) {
                    KDAssert(numLeft < split.numLeft);
                    leftBounds.expandBy(aabb);
                    leftIndices[numLeft++] = primIndex;
                } else if (startIdx > split.leftBin) {
                    KDAssert(numRight < split.numRight);
                    rightBounds.expandBy(aabb);
                    rightIndices[numRight++] = primIndex;
                } else {
                    leftBounds.expandBy(aabb);
                    rightBounds.expandBy(aabb);
                    KDAssert(numLeft < split.numLeft);
                    KDAssert(numRight < split.numRight);
                    leftIndices[numLeft++] = primIndex;
                    rightIndices[numRight++] = primIndex;
                }
            }
            leftBounds.clip(m_aabb);
            rightBounds.clip(m_aabb);
            split.pos = m_min[axis] + m_binSize[axis] * (split.leftBin + 1);
            leftBounds.max[axis] = std::min(leftBounds.max[axis], (Float) split.pos);
            rightBounds.min[axis] = std::max(rightBounds.min[axis], (Float) split.pos);

            KDAssert(numLeft == split.numLeft);
            KDAssert(numRight == split.numRight);

            /// Release the unused memory regions
            if (isLeftChild)
                ctx.leftAlloc.shrinkAllocation(leftIndices, split.numLeft);
            else
                ctx.rightAlloc.shrinkAllocation(rightIndices, split.numRight);

            return Partition(leftBounds, leftIndices,
                rightBounds, rightIndices);
        }
#endif

    protected:
        std::unique_ptr<Size[], AlignedAllocator> m_bins;
        Size m_binCount = 0;
        Vector m_invBinSize;
        Vector m_maxBin;
        BoundingBox m_bbox;
    };

    /// Release all memory
    virtual ~TShapeKDTree() {
        AlignedAllocator::dealloc(m_indices);
        if (m_nodes)
            AlignedAllocator::dealloc(m_nodes - 1 /* undo shift */);
    }

protected:
    Index *m_indices = nullptr;
    Index *m_nodes = nullptr;
    Float m_traversalCost = 15;
    Float m_queryCost = 20;
    Float m_emptySpaceBonus = Float(0.9f);
    bool m_clipPrimitives = true;
    bool m_retractBadSplits = true;
    Size m_maxDepth = 0;
    Size m_stopPrims = 6;
    Size m_maxBadRefines = 3;
    Size m_exactPrimThreshold = 65536;
    Size m_minMaxBins = 128;
    Size m_nodeCount = 0;
    Size m_indexCount = 0;
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

NAMESPACE_END(detail)

class SurfaceAreaHeuristic3f {
public:
    /**
     * \brief Initialize the surface area heuristic with the bounds of
     * a parent node
     *
     * Precomputes some information so that traversal probabilities
     * of potential split planes can be evaluated efficiently
     */
    SurfaceAreaHeuristic3f(const BoundingBox3f &bbox) {
        auto extents = bbox.extents();
        Float temp = 2.f / bbox.surfaceArea();
        auto a = extents.swizzle<1, 2, 0>();
        auto b = extents.swizzle<2, 0, 1>();
        m_temp0 = (a * b) * temp;
        m_temp1 = (a + b) * temp;
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
    std::pair<Float, Float> operator()(int axis, Float leftWidth, Float rightWidth) const {
        return std::pair<Float, Float>(
            m_temp0[axis] + m_temp1[axis] * leftWidth,
            m_temp0[axis] + m_temp1[axis] * rightWidth);
    }
private:
    Vector3f m_temp0, m_temp1;
};



class MTS_EXPORT ShapeKDTree
    : public detail::TShapeKDTree<BoundingBox3f, uint32_t, SurfaceAreaHeuristic3f, ShapeKDTree> {
public:
    using Base = detail::TShapeKDTree<BoundingBox3f, uint32_t, SurfaceAreaHeuristic3f, ShapeKDTree>;

    ShapeKDTree();
    void addShape(Shape *shape);

    Size primitiveCount() const { return m_primitiveMap.back(); }
    Size shapeCount() const { return m_primitiveMap.back(); }

    BoundingBox3f bbox(Index i) const {
        //Index shapeIndex = findShape(i);
        return m_shapes[0]->bbox(i);
    }

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
            (Size) m_primitiveMap.size(),
            [&](Index k) {
                return m_primitiveMap[k] <= i;
            }
        );
        Assert(shapeIndex < shapeCount() &&
               m_primitiveMap.size() == primitiveCount() + 1);

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
