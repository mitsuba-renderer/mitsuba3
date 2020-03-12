#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/imageblock.h>
#include <tbb/spin_mutex.h>

#if !defined(MTS_BLOCK_SIZE)
#  define MTS_BLOCK_SIZE 32
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
class MTS_EXPORT_RENDER Spiral : public Object {
public:
    using Float = float;
    MTS_IMPORT_CORE_TYPES()

    /// Create a new spiral generator for the given size, offset into a larger frame, and block size
    Spiral(Vector2i size, Vector2i offset, size_t block_size, size_t passes = 1);

    template <typename Film>
    Spiral(const Film &film, size_t block_size, size_t passes = 1)
        : Spiral(film->crop_size(), film->crop_offset(), block_size, passes) {}

    /// Return the maximum block size
    size_t max_block_size() const { return m_block_size; }

    /// Return the total number of blocks
    size_t block_count() { return m_block_count; }

    /// Reset the spiral to its initial state. Does not affect the number of passes.
    void reset();

    /**
     * Sets the number of time the spiral should automatically reset.
     * Not affected by a call to \ref reset.
     */
    void set_passes(size_t passes) {
        m_remaining_passes = passes;
    }

    /**
     * \brief Return the offset, size and unique identifer of the next block.
     *
     * A size of zero indicates that the spiral traversal is done.
     */
    std::tuple<Vector2i, Vector2i, size_t> next_block();

    MTS_DECLARE_CLASS()
protected:
    enum class Direction {
        Right = 0,
        Down,
        Left,
        Up
    };

    size_t m_block_counter, //< Number of blocks generated so far
           m_block_count,   //< Total number of blocks to be generated
           m_block_size;    //< Size of the (square) blocks (in pixels)

    Vector2i m_size,        //< Size of the 2D image (in pixels).
             m_offset,      //< Offset to the crop region on the sensor (pixels).
             m_blocks;      //< Number of blocks in each direction.

    Point2i  m_position;    //< Relative position of the current block.
    /// Direction where the spiral is currently headed.
    Direction m_current_direction;
    /// Step counters.
    int m_steps_left, m_steps;

    /// Number of times the spiral should automatically restart.
    size_t m_remaining_passes;

    /// Protects the spiral's state (thread safety).
    tbb::spin_mutex m_mutex;
};

NAMESPACE_END(mitsuba)
