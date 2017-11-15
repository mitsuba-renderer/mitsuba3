#pragma once

#include <mutex>
#include <mitsuba/core/object.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/imageblock.h>
#include <tbb/spin_mutex.h>

#if !defined(MTS_BLOCK_SIZE)
#define MTS_BLOCK_SIZE 32
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
    /// Create a new spiral generator for an image of the given width and height
    Spiral(const Film *film);
    virtual ~Spiral() { }

    /// Reset the spiral to its initial state
    void reset();

    /// Returns the next block to be processed, or nullptr if there are none.
    ref<ImageBlock> next_block();

    /// Return the maximum block size
    int max_block_size() const { return m_block_size; }

    MTS_DECLARE_CLASS()

protected:
    enum EDirection {
        ERight = 0,
        EDown,
        ELeft,
        EUp
    };

    size_t m_blocks_counter, //< Number of blocks generated so far
           m_n_total_blocks, //< Total number of blocks to be generated
           m_block_size;     //< Size of the (square) blocks (in pixels)
    Vector2i m_size,         //< Size of the 2D image (in pixels).
             m_offset,       //< Offset to the crop region on the sensor (pixels).
             m_n_blocks;     //< Number of blocks in each direction.
    Point2i  m_position;     //< Relative position of the current block.
    /// Direction where the spiral is currently headed.
    int m_current_direction;
    /// Step counters.
    int m_steps_left, m_n_steps;
    /// Protects the spiral's state (thread safety).
    tbb::spin_mutex m_mutex;
    /// The film's reconstruction filter, passed to the image blocks on construction.
    const ReconstructionFilter *m_rfilter;
};

NAMESPACE_END(mitsuba)
