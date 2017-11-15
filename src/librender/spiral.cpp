#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/spiral.h>
#include <mitsuba/mitsuba.h>

NAMESPACE_BEGIN(mitsuba)

Spiral::Spiral(const Film *film)
    : m_block_size(MTS_BLOCK_SIZE),
      m_size(film->crop_size()), m_offset(film->crop_offset()),
      m_rfilter(film->reconstruction_filter()) {

    m_n_blocks = Vector2i(
        (int) std::ceil(m_size.x() / (Float) m_block_size),
        (int) std::ceil(m_size.y() / (Float) m_block_size));
    m_n_total_blocks = m_n_blocks.x() * m_n_blocks.y();

    reset();
}

void Spiral::reset() {
    m_blocks_counter = 0;
    m_current_direction = ERight;
    m_position = m_n_blocks / 2;
    m_steps_left = 1;
    m_n_steps = 1;
}

ref<ImageBlock> Spiral::next_block() {
    // Reimplementation of the spiraling block generator by Adam Arbree.
    std::lock_guard<tbb::spin_mutex> lock(m_mutex);

    if (m_n_total_blocks == m_blocks_counter)
        return nullptr;

    Vector2i offset(m_position * m_block_size);
    Vector2i size(std::min((int) m_block_size, m_size.x() - offset.x()),
                  std::min((int) m_block_size, m_size.y() - offset.y()));
    Assert(size.x() > 0 && size.y() > 0);

    // TODO: use the RGBAW format (once it's supported).
    ref<ImageBlock> block(new ImageBlock(Bitmap::ERGB, size));
    block->set_offset(offset);
    block->set_offset(m_offset + offset);

    if (++m_blocks_counter == m_n_total_blocks)
        return block;

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
                ++m_n_steps;
            m_steps_left = m_n_steps;
        }
    } while (m_position.x() < 0 || m_position.y() < 0
          || m_position.x() >= m_n_blocks.x()
          || m_position.y() >= m_n_blocks.y());

    return block;
}

MTS_IMPLEMENT_CLASS(Spiral, Object)
NAMESPACE_END(mitsuba)
