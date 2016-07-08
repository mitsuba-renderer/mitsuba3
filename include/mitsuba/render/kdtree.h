#include <mitsuba/core/logger.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/render/shape.h>

#pragma once

/// OrderedChunkAllocator: don't create chunks smaller than 512 KiB
#define MTS_KD_MIN_ALLOC 512*1024

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(detail)

/**
 * \brief Special ordered memory allocator
 *
 * During kd-tree construction, large amounts of memory are required to
 * temporarily hold index and edge event lists. When not implemented properly,
 * these allocations can become a critical bottleneck. The class
 * \ref OrderedChunkAllocator provides a specialized memory allocator, which
 * reserves memory in chunks of at least 512KiB. An important assumption made
 * by the allocator is that memory will be released in the exact same order, in
 * which it was previously allocated. This makes it possible to create an
 * implementation with a very low memory overhead. Note that no locking is
 * done, hence each thread will need its own allocator.
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
    template <typename T> T * __restrict allocate(size_t size) {
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
        chunk.start = (uint8_t *) AlignedAllocator::alloc(allocSize);
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
#if defined(MTS_KD_DEBUG)
        for (auto const &chunk : m_chunks) {
            if (ptr == chunk.begin + chunk.size)
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

#if defined(MTS_KD_DEBUG)
        if (newSize == 0) {
            for (auto const &chunk : m_chunks) {
                if (ptr == chunk.begin + chunk.size)
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

NAMESPACE_END(detail)


class MTS_EXPORT ShapeKDTree : public Object {
public:
    MTS_DECLARE_CLASS()

    void addShape(Shape *shape);

protected:
    std::vector<ref<Shape>> m_shapes;
    BoundingBox3f m_bbox;
};

NAMESPACE_END(mitsuba)
