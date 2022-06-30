#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/spiral.h>
#include <mitsuba/mitsuba.h>

NAMESPACE_BEGIN(mitsuba)

Spiral::Spiral(const Vector2u &size, const Vector2u &offset,
               uint32_t block_size, uint32_t passes)
    : m_size(size), m_offset(offset), m_passes_left(passes),
      m_block_size(block_size) {

    m_blocks = (size + (block_size - 1)) / block_size;
    m_block_count = dr::prod(m_blocks);

    reset();
}

void Spiral::reset() {
    m_block_counter = 0;
    direction = Direction::Right;
    m_position = Vector2u(m_blocks / 2);
    m_steps_left = 1;
    m_spiral_size = 1;
}

std::tuple<Spiral::Vector2i, Spiral::Vector2u, uint32_t> Spiral::next_block() {
    // Reimplementation of the spiraling block generator by Adam Arbree.
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_block_counter == m_block_count) {
        if (m_passes_left > 1) {
            --m_passes_left;
            reset();
        } else {
            return { 0, 0, (uint32_t) -1 };
        }
    }

    // Calculate a unique identifier per block
    uint32_t block_id =
        m_block_counter + (m_passes_left - 1) * m_block_count;

    Vector2u offset = m_position * m_block_size,
             size   = dr::minimum(m_block_size, m_size - offset);

    Assert(dr::all(offset <= m_size));

    ++m_block_counter;

    if (m_block_counter != m_block_count) {
        // Prepare the next block's position along the spiral.
        do {
            switch (direction) {
                case Direction::Right: ++m_position.x(); break;
                case Direction::Down:  ++m_position.y(); break;
                case Direction::Left:  --m_position.x(); break;
                case Direction::Up:    --m_position.y(); break;
            }

            if (--m_steps_left == 0) {
                direction = Direction(((int) direction + 1) % 4);
                if (direction == Direction::Left ||
                    direction == Direction::Right)
                    ++m_spiral_size;
                m_steps_left = m_spiral_size;
            }
        } while (dr::any(m_position < 0 || m_position >= m_blocks));
    }

    return { offset + m_offset, size, block_id };
}

MI_IMPLEMENT_CLASS(Spiral, Object)
NAMESPACE_END(mitsuba)
