#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/imageblock.h>
#include <mutex>

#if !defined(MI_BLOCK_SIZE)
#  define MI_BLOCK_SIZE 32
#endif

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Generates a spiral of blocks to be rendered.
 *
 * \author Adam Arbree
 * Aug 25, 2005
 * RayTracer.java
 * Used with permission.
 * Copyright 2005 Program of Computer Graphics, Cornell University
 * \ingroup librender
 */
class MI_EXPORT_LIB Spiral : public Object {
public:
    using Vector2i = Vector<int32_t, 2>;
    using Vector2u = Vector<uint32_t, 2>;
    using Point2i = Point<int32_t, 2>;

    /// Create a new spiral generator for the given size, offset into a larger frame, and block size
    Spiral(const Vector2u &size,
           const Vector2u &offset,
           uint32_t block_size,
           uint32_t passes = 1);

    /// Return the maximum block size
    uint32_t max_block_size() const { return m_block_size; }

    /// Return the total number of blocks
    uint32_t block_count() { return m_block_count; }

    /// Reset the spiral to its initial state. Does not affect the number of passes.
    void reset();

    /**
     * \brief Return the offset, size, and unique identifier of the next block.
     *
     * A size of zero indicates that the spiral traversal is done.
     */
    std::tuple<Vector2i, Vector2u, uint32_t> next_block();

    MI_DECLARE_CLASS()
protected:
    enum class Direction { Right, Down, Left, Up };

    std::mutex m_mutex;       //< Protects the state for thread safety
    Vector2u m_size;          //< Size of the 2D image (in pixels)
    Vector2u m_offset;        //< Offset to the crop region on the sensor (pixels)
    Vector2u m_blocks;        //< Number of blocks in each direction
    Point2i  m_position;      //< Relative position of the current block
    Direction direction;      //< Current spiral direction
    uint32_t m_block_counter; //< Number of blocks generated so far
    uint32_t m_block_count;   //< Number of blocks to be generated in pass
    uint32_t m_passes_left;   //< Remaining spiral passes to be generated
    uint32_t m_block_size;    //< Size of the (square) blocks (in pixels)
    uint32_t m_steps_left;    //< Steps before next change of direction
    uint32_t m_spiral_size;   //< Current spiral size in blocks
};

NAMESPACE_END(mitsuba)
