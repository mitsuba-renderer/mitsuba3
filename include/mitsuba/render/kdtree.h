#pragma once

#include <mitsuba/core/logger.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/util.h>
#include <mitsuba/render/shape.h>
#include <tbb/tbb.h>

/// Compile-time KD-tree depth limit to enable traversal with stack memory
#define MTS_KD_MAXDEPTH 48

/// Number of bins used for Min-Max binning
#define MTS_KD_BINS 256

/// OrderedChunkAllocator: don't create chunks smaller than 512 KiB
#define MTS_KD_MIN_ALLOC 512*1024

/// Optional: turn on lots of assertions in the kd-tree builder
#define MTS_KD_DEBUG 1

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

        #if MTS_KD_DEBUG == 1
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

        #if MTS_KD_DEBUG == 1
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

template <typename BoundingBox_,
          typename Index_,
          typename Derived_> class TShapeKDTree : public Object {
public:
    using BoundingBox = BoundingBox_;
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

        MinMaxBins bins(m_minMaxBins);

        bins.setBoundingBox(m_bbox);
        for (Size i = 0; i<primCount; ++i) {
            bins.put(derived().bbox(i));
        }
    }

    MTS_DECLARE_CLASS()
protected:
    /**
     * \brief Min-max binning data structure
     *
     * See
     *   "Highly Parallel Fast KD-tree Construction for Interactive Ray Tracing of
     *   Dynamic Scenes" by M. Shevtsov, A. Soupikov and A. Kapustin
     */
    class MinMaxBins {
    public:
        MinMaxBins(Size binCount) : m_binCount(binCount) {
            m_bins = AlignedAllocator::alloc<Size>(m_binCount * Dimension * 2);
            memset(m_bins, 0, sizeof(Size) * m_binCount * Dimension * 2);
            m_maxBin = Vector::Constant(Scalar(m_binCount - 1));
        }

        ~MinMaxBins() {
            AlignedAllocator::dealloc(m_bins);
        }

        SIMD_API void put(BoundingBox bbox) {
            using simd::min;
            using simd::max;
            using Int = typename Vector::Int;

            alignas(alignof(Vector)) Int indexMin[Vector::ActualSize];
            alignas(alignof(Vector)) Int indexMax[Vector::ActualSize];

            Vector relMin = (bbox.min - m_offset) * m_invBinSize;
            Vector relMax = (bbox.max - m_offset) * m_invBinSize;

            relMin = min(max(relMin, Vector::Zero()), m_maxBin);
            relMax = min(max(relMax, Vector::Zero()), m_maxBin);

            floatToInt(relMin).store((float *) indexMin);
            floatToInt(relMax).store((float *) indexMax);

            Index offset = 0;
            for (Index axis = 0; axis < Dimension; ++axis) {
                m_bins[offset + indexMin[axis] * 2 + 0]++;
                m_bins[offset + indexMax[axis] * 2 + 1]++;

                offset += 2 * Dimension * m_binCount;
            }
        }

        MinMaxBins& operator+=(const MinMaxBins &other) {
            /* Add bin counts (using SIMD instructions, please) */
            Size * __restrict__ b        = (Size *)       SIMD_ASSUME_ALIGNED(m_bins);
            const Size * __restrict__ bo = (const Size *) SIMD_ASSUME_ALIGNED(other.m_bins);

            SIMD_IVDEP /* No, the arrays don't alias */
            for (Size i = 0, size = m_binCount * Dimension * 2; i < size; ++i)
                b[i] += bo[i];
            return *this;
        }

        /// \brief Prepare to bin for the specified bounds
        void setBoundingBox(const BoundingBox &bbox) {
            std::cout << "setBOundingBox = " << bbox << std::endl;
            m_binSize = bbox.extents() / (Float) m_binCount;
            m_offset = bbox.min;
            m_invBinSize = 1 / m_binSize;
        }

    protected:
        Size *m_bins;
        Size m_binCount;
        Vector m_binSize;
        Vector m_invBinSize;
        Vector m_maxBin;
        Point m_offset;
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

template <typename BoundingBox, typename Index, typename Derived>
Class *TShapeKDTree<BoundingBox, Index, Derived>::m_class =
    new Class("TShapeKDTree", "Object", true, nullptr, nullptr);

template <typename BoundingBox, typename Index, typename Derived>
const Class *TShapeKDTree<BoundingBox, Index, Derived>::class_() const {
    return m_class;
}

NAMESPACE_END(detail)

class MTS_EXPORT ShapeKDTree
    : public detail::TShapeKDTree<BoundingBox3f, uint32_t, ShapeKDTree> {
public:
    using Base = detail::TShapeKDTree<BoundingBox3f, uint32_t, ShapeKDTree>;

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
