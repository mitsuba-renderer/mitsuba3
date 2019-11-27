#include <mitsuba/render/imageblock.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
ImageBlock<Float, Spectrum>::ImageBlock(PixelFormat fmt, const ScalarVector2i &size,
                                        const ReconstructionFilter *filter, size_t channels,
                                        bool warn, bool border, bool normalize)
    : m_offset(0), m_size(size), m_filter(filter), m_weights_x(nullptr), m_weights_y(nullptr),
      m_warn(warn), m_normalize(normalize) {
    m_border_size = (filter != nullptr && border) ? filter->border_size() : 0;

    // Allocate a small bitmap data structure for the block
    m_bitmap = new Bitmap(fmt, struct_type_v<ScalarFloat>,
                          size + ScalarVector2i(2 * m_border_size), channels);
    m_bitmap->clear();

    if (filter) {
        // Temporary buffers used in put()
        int temp_buffer_size = (int) std::ceil(2 * filter->radius()) + 1;
        m_weights_x = new Float[2 * temp_buffer_size];
        m_weights_y = m_weights_x + temp_buffer_size;
    }
}

template <typename Float, typename Spectrum>
ImageBlock<Float, Spectrum>::~ImageBlock() {
    /* Note that m_weights_y points to the same array as
       m_weights_x, so there is no need to delete it. */
    if (m_weights_x)
        delete[] m_weights_x;
}

template <typename Float, typename Spectrum>
typename ImageBlock<Float, Spectrum>::Mask
ImageBlock<Float, Spectrum>::put(const Point2f &pos_, const Float *value, Mask active) {
    Assert(m_filter != nullptr);

    uint32_t channels = (uint32_t) m_bitmap->channel_count();

    // Check if all sample values are valid
    if (m_warn) {
        Mask is_valid = true;
        for (size_t k = 0; k < channels; ++k)
            is_valid &= enoki::isfinite(value[k]) && value[k] >= 0;

        if (unlikely(any(active && !is_valid))) {
            std::ostringstream oss;
            oss << "Invalid sample value: [";
            for (size_t i = 0; i < channels; ++i) {
                oss << value[i];
                if (i + 1 < channels) oss << ", ";
            }
            oss << "]";
            Log(Warn, "%s", oss.str());
            active &= is_valid;
        }
    }

    ScalarFloat filter_radius = m_filter->radius();
    ScalarVector2i size = m_bitmap->size();

    // Convert to pixel coordinates within the image block
    Point2f pos = pos_ - (m_offset - m_border_size + .5f);

    // Determine the affected range of pixels
    Point2i lo = max(ceil2int <Point2i>(pos - filter_radius), 0),
            hi = min(floor2int<Point2i>(pos + filter_radius), size - 1);
    Vector2i window_size = hi - lo;

    // Lookup values from the pre-rasterized filter
    int n = ceil2int<int>((m_filter->radius() - 2.f * math::Epsilon<ScalarFloat>) * 2.f);

    Point2f base = lo - pos;
    ENOKI_NOUNROLL for (int i = 0; i < n; ++i) {
        Point2f p = base + i;
        m_weights_x[i] = m_filter->eval_discretized(p.x(), active);
        m_weights_y[i] = m_filter->eval_discretized(p.y(), active);
    }

    if (ENOKI_UNLIKELY(m_normalize)) {
        Float wx(0), wy(0);
        for (int i = 0; i <= n; ++i) {
            wx += m_weights_x[i];
            wy += m_weights_y[i];
        }

        Float factor = rcp(wx * wy);
        for (int i = 0; i <= n; ++i)
            m_weights_x[i] *= factor;
    }

    // Rasterize the filtered sample into the framebuffer
    ScalarFloat *buffer = (ScalarFloat *) m_bitmap->data();
    ENOKI_NOUNROLL for (int yr = 0; yr <= n; ++yr) {
        Mask enabled = active && yr <= window_size.y();
        Int32 y = lo.y() + yr;

        ENOKI_NOUNROLL for (int xr = 0; xr <= n; ++xr) {
            Int32 x = lo.x() + xr;
            Int32 offset = channels * (y * size.x() + x);
            Float weights = m_weights_y[yr] * m_weights_x[xr];

            enabled &= xr <= window_size.x();
            ENOKI_NOUNROLL for (uint32_t k = 0; k < channels; ++k)
                scatter_add(buffer + k, weights * value[k], offset, enabled);
        }
    }

    return active;
}

template <typename Float, typename Spectrum>
std::string ImageBlock<Float, Spectrum>::to_string() const {
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

MTS_INSTANTIATE_CLASS(ImageBlock)
NAMESPACE_END(mitsuba)
