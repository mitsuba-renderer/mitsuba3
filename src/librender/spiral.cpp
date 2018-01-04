#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/spiral.h>
#include <mitsuba/mitsuba.h>

NAMESPACE_BEGIN(mitsuba)

Spiral::Spiral(const Film *film, size_t block_size)
    : m_block_size(block_size),
      m_size(film->crop_size()), m_offset(film->crop_offset()) {

    m_blocks = Vector2i(ceil(Vector2f(m_size) / m_block_size));
    m_block_count = hprod(Vector2s(m_blocks));

    reset();
}

void Spiral::reset() {
    m_block_counter = 0;
    m_current_direction = ERight;
    m_position = m_blocks / 2;
    m_steps_left = 1;
    m_steps = 1;
}

std::pair<Vector2i, Vector2i> Spiral::next_block() {
    // Reimplementation of the spiraling block generator by Adam Arbree.
    std::lock_guard<tbb::spin_mutex> lock(m_mutex);

    if (m_block_count == m_block_counter)
        return { Vector2i(0), Vector2i(0) };

    Vector2i offset(m_position * (int) m_block_size);
    Vector2i size = min((int) m_block_size, m_size - offset);
    offset += m_offset;

    Assert(all(size > 0));

    ++m_block_counter;

    if (m_block_counter != m_block_count) {
        // Prepare the next block's position along the spiral.
        do {
            switch (m_current_direction) {
                case ERight: ++m_position.x(); break;
                case EDown:  ++m_position.y(); break;
                case ELeft:  --m_position.x(); break;
                case EUp:    --m_position.y(); break;
            }

            if (--m_steps_left == 0) {
                m_current_direction = (m_current_direction + 1) % 4;
                if (m_current_direction == ELeft || m_current_direction == ERight)
                    ++m_steps;
                m_steps_left = m_steps;
            }
        } while (any(m_position < 0 || m_position >= m_blocks));
    }

    return { offset, size };
}

MTS_IMPLEMENT_CLASS(Spiral, Object)
NAMESPACE_END(mitsuba)
