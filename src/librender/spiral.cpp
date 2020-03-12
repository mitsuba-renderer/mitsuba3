#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/spiral.h>
#include <mitsuba/mitsuba.h>
#include <mutex>

NAMESPACE_BEGIN(mitsuba)

Spiral::Spiral(Vector2i size, Vector2i offset, size_t block_size, size_t passes)
    : m_block_size(block_size),
      m_size(size), m_offset(offset),
      m_remaining_passes(passes) {

    m_blocks = Vector2i(ceil(Vector2f(m_size) / m_block_size));
    m_block_count = hprod(m_blocks);

    reset();
}

void Spiral::reset() {
    m_block_counter = 0;
    m_current_direction = Direction::Right;
    m_position = m_blocks / 2;
    m_steps_left = 1;
    m_steps = 1;
}

std::tuple<Spiral::Vector2i, Spiral::Vector2i, size_t> Spiral::next_block() {
    // Reimplementation of the spiraling block generator by Adam Arbree.
    std::lock_guard<tbb::spin_mutex> lock(m_mutex);

    if (m_block_count == m_block_counter) {
        if (m_remaining_passes > 1) {
            --m_remaining_passes;
            reset();
        }
        else
            return { Vector2i(0), Vector2i(0), (size_t) -1 };
    }

    // Calculate a unique identifer per block
    size_t block_id = m_block_counter + (m_remaining_passes - 1) * m_block_count;

    Vector2i offset(m_position * (int) m_block_size);
    Vector2i size = min((int) m_block_size, m_size - offset);
    offset += m_offset;

    Assert(all(size > 0));

    ++m_block_counter;

    if (m_block_counter != m_block_count) {
        // Prepare the next block's position along the spiral.
        do {
            switch (m_current_direction) {
                case Direction::Right: ++m_position.x(); break;
                case Direction::Down:  ++m_position.y(); break;
                case Direction::Left:  --m_position.x(); break;
                case Direction::Up:    --m_position.y(); break;
            }

            if (--m_steps_left == 0) {
                m_current_direction = Direction(((int) m_current_direction + 1) % 4);
                if (m_current_direction == Direction::Left ||
                    m_current_direction == Direction::Right)
                    ++m_steps;
                m_steps_left = m_steps;
            }
        } while (any(m_position < 0 || m_position >= m_blocks));
    }

    return { offset, size, block_id };
}

MTS_IMPLEMENT_CLASS(Spiral, Object)
NAMESPACE_END(mitsuba)
