#include <mitsuba/render/imageblock.h>

NAMESPACE_BEGIN(mitsuba)

ImageBlock::ImageBlock(Bitmap::EPixelFormat fmt, const Vector2i &size,
                       const ReconstructionFilter *filter, size_t channels,
                       bool warn)
    : m_offset(0), m_size(size), m_filter(filter), m_weights_x(nullptr),
      m_weights_y(nullptr), m_weights_x_p(nullptr), m_weights_y_p(nullptr),
      m_warn(warn) {
    m_border_size = filter ? filter->border_size() : 0;

    // Allocate a small bitmap data structure for the block
    m_bitmap = new Bitmap(fmt, Struct::EType::EFloat,
                          size + Vector2i(2 * m_border_size), channels);
    m_bitmap->clear();

    if (filter) {
        // Temporary buffers used in put()
        int temp_buffer_size = (int) std::ceil(2 * filter->radius()) + 1;
        m_weights_x = new Float[2 * temp_buffer_size];
        m_weights_y = m_weights_x + temp_buffer_size;

        // Vectorized variant
        m_weights_x_p = new FloatP[2 * temp_buffer_size];
        m_weights_y_p = m_weights_x_p + temp_buffer_size;
    }
}

ImageBlock::~ImageBlock() {
    /* Note that m_weights_y points to the same array as
       m_weights_x, so there is no need to delete it. */
    if (m_weights_x)
        delete[] m_weights_x;

    if (m_weights_x_p)
        delete[] m_weights_x_p;
}

bool ImageBlock::put(const Point2f &pos_, const Float *value) {
    Assert(m_filter != nullptr);
    size_t channels = m_bitmap->channel_count();

    // Check if all sample values are valid
    bool valid_sample = true;
    if (m_warn) {
        for (size_t i = 0; i < channels; ++i)
            valid_sample &= std::isfinite(value[i]) && value[i] >= 0;

        if (unlikely(!valid_sample)) {
            std::ostringstream oss;
            oss << "Invalid sample value: [";
            for (size_t i = 0; i < channels; ++i) {
                oss << value[i];
                if (i + 1 < channels) oss << ", ";
            }
            oss << "]";
            Log(EWarn, "%s", oss.str());
            return false;
        }
    }

    Float filter_radius = m_filter->radius();
    Vector2i size = m_bitmap->size();

    // Convert to pixel coordinates within the image block
    Point2f pos = pos_ - .5f - (m_offset - m_border_size);

    // Determine the affected range of pixels
    Point2i lo = max(Point2i(ceil (pos - filter_radius)), 0);
    Point2i hi = min(Point2i(floor(pos + filter_radius)), size - 1);

    // Lookup values from the pre-rasterized filter
    for (int x = lo.x(), idx = 0; x <= hi.x(); ++x)
        m_weights_x[idx++] = m_filter->eval_discretized(x - pos.x());

    for (int y = lo.y(), idx = 0; y <= hi.y(); ++y)
        m_weights_y[idx++] = m_filter->eval_discretized(y - pos.y());

    // Rasterize the filtered sample into the framebuffer
    for (int y = lo.y(), yr = 0; y <= hi.y(); ++y, ++yr) {
        const Float weightY = m_weights_y[yr];
        auto dest = static_cast<Float *>(m_bitmap->data())
            + (y * (size_t) size.x() + lo.x()) * channels;

        for (int x = lo.x(), xr = 0; x <= hi.x(); ++x, ++xr) {
            const Float weight = m_weights_x[xr] * weightY;

            for (size_t k = 0; k < channels; ++k)
                *dest++ += weight * value[k];
        }
    }
    return true;
}

MaskP ImageBlock::put(const Point2fP &pos_, const FloatP *value, MaskP active) {
    Assert(m_filter != nullptr);
    using Mask = mask_t<FloatP>;

    size_t channels = m_bitmap->channel_count();

    // Check if all sample values are valid
    Mask is_valid(true);
    if (m_warn) {
        for (size_t k = 0; k < channels; ++k)
            is_valid &= enoki::isfinite(value[k]) && (value[k] >= 0);

        if (unlikely(any(active && !is_valid))) {
            std::ostringstream oss;
            oss << "Invalid sample value(s): [";
            for (size_t i = 0; i < channels; ++i) {
                oss << value[i];
                if (i + 1 < channels)
                    oss << ", ";
            }
            oss << "]";
            Log(EWarn, "%s", oss.str());
        }
    }

    Float filter_radius = m_filter->radius();
    Vector2i size = m_bitmap->size();

    // Convert to pixel coordinates within the image block
    Point2fP pos = pos_ - .5f - (m_offset - m_border_size);

    // Determine the affected range of pixels
    Point2iP lo = max(Point2iP(ceil (pos - filter_radius)), 0);
    Point2iP hi = min(Point2iP(floor(pos + filter_radius)), size - 1);

    // Lookup values from the pre-rasterized filter
    Vector2iP window_sizes = hi - lo;
    Point2i max_size(hmax(window_sizes.x()), hmax(window_sizes.y()));

    auto corner = lo - pos;
    for (int i = 0; i <= max_size.x(); ++i)
        m_weights_x_p[i] = m_filter->eval_discretized(corner.x() + i);
    for (int i = 0; i <= max_size.y(); ++i)
        m_weights_y_p[i] = m_filter->eval_discretized(corner.y() + i);


    // Rasterize the filtered sample into the framebuffer
    auto *buffer = (Float *) m_bitmap->data();
    Mask enabled;
    for (int yr = 0; yr <= max_size.y(); ++yr) {
        enabled = active && is_valid && (yr <= window_sizes.y());
        auto y = lo.y() + yr;

        for (int xr = 0; xr <= max_size.x(); ++xr) {
            enabled &= xr <= window_sizes.x();
            if (none(enabled))
                continue;
            // Linearized offsets: n_channels * (y * n_x + x)
            auto offsets = channels * (y * size.x() + (lo.x() + xr));
            auto weights = m_weights_y_p[yr] * m_weights_x_p[xr];

            for (size_t k = 0; k < channels; ++k) {
                FloatP weighted_value = weights * value[k];

                // We need to be extra-careful about the "histogram problem". See:
                //   http://enoki.readthedocs.io/en/master/
                //   advanced.html#the-histogram-problem-and-conflict-detection.
                enoki::transform<FloatP>(
                    // Base offset into the bitmap's buffer.
                    buffer + k,
                    // Index of each position, relative to the base (may have
                    // repeated values, which is why we're using `transform`).
                    offsets,
                    // Perform operation on active lanes only.
                    enabled,
                    // Operation (accumulate weighted value).
                    [](auto &&x, auto &&wv) { x += wv; },
                    // Reconstruction weights, values to store.
                    weighted_value
                );
            }
        }
    }

    return is_valid;
}

std::string ImageBlock::to_string() const {
    std::ostringstream oss;
    oss << "ImageBlock[" << std::endl
        << "  offset = " << m_offset << "," << std::endl
        << "  size = "   << m_size   << "," << std::endl
        << "  border_size = " << m_border_size;
    if (m_filter)
        oss << "," << std::endl << "  filter = " << m_filter->to_string();
    oss << std::endl
        << "]";
    return oss.str();
}

MTS_IMPLEMENT_CLASS(ImageBlock, Object)
NAMESPACE_END(mitsuba)
