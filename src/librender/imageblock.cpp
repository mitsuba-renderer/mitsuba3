#include <mitsuba/render/imageblock.h>

NAMESPACE_BEGIN(mitsuba)

ImageBlock::ImageBlock(Bitmap::EPixelFormat fmt, const Vector2i &size,
    const ReconstructionFilter *filter, size_t channels, bool warn)
    : m_offset(0), m_size(size), m_filter(filter),
    m_weights_x(nullptr), m_weights_y(nullptr), m_warn(warn) {
    m_border_size = filter ? filter->border_size() : 0;

    // Allocate a small bitmap data structure for the block
    m_bitmap = new Bitmap(fmt, Struct::EType::EFloat,
        size + Vector2i(2 * m_border_size), channels);

    if (filter) {
        // Temporary buffers used in put()
        int temp_buffer_size = (int)std::ceil(2 * filter->radius()) + 1;
        m_weights_x = new Float[2 * temp_buffer_size];
        m_weights_y = m_weights_x + temp_buffer_size;

        // TODO Pre_allocate vectorized weights buffers
    }
}

ImageBlock::~ImageBlock() {
    if (m_weights_x)
        delete[] m_weights_x;
}

bool ImageBlock::put(const Point2f &_pos, const Float *value) {
    const int channels = m_bitmap->channel_count();

    // Check if all sample values are valid
    bool valid_sample = true;
    if (m_warn) {
        for (int i = 0; i < channels; ++i) {
            if (unlikely((!std::isfinite(value[i]) || value[i] < 0))) {
                valid_sample = false;
                break;
            }
        }
    }

    if (unlikely(!valid_sample)) {
        std::ostringstream oss;
        oss << "Invalid sample value : [";
        for (int i = 0; i < channels; ++i) {
            oss << value[i];
            if (i + 1 < channels)
                oss << ", ";
        }
        oss << "]";
        Log(EWarn, "%s", oss.str());
        return false;
    } else {
        const Float filter_radius = m_filter->radius();
        const Vector2i &size = m_bitmap->size();

        // Convert to pixel coordinates within the image block
        const Point2f pos(
            _pos.x() - 0.5f - (m_offset.x() - m_border_size),
            _pos.y() - 0.5f - (m_offset.y() - m_border_size)
        );

        // Determine the affected range of pixels
        const Point2i lo(std::max((int)std::ceil(pos.x() - filter_radius), 0),
            std::max((int)std::ceil(pos.y() - filter_radius), 0));
        const Point2i hi(std::min((int)std::floor(pos.x() + filter_radius), size.x() - 1),
            std::min((int)std::floor(pos.y() + filter_radius), size.y() - 1));

        // Lookup values from the pre-rasterized filter
        for (int x = lo.x(), idx = 0; x <= hi.x(); ++x)
            m_weights_x[idx++] = m_filter->eval_discretized(x - pos.x());
        for (int y = lo.y(), idx = 0; y <= hi.y(); ++y)
            m_weights_y[idx++] = m_filter->eval_discretized(y - pos.y());

        // Rasterize the filtered sample into the framebuffer
        for (int y = lo.y(), yr = 0; y <= hi.y(); ++y, ++yr) {
            const Float weightY = m_weights_y[yr];
            auto dest = static_cast<Float *>(m_bitmap->data())
                + (y * (size_t)size.x() + lo.x()) * channels;

            for (int x = lo.x(), xr = 0; x <= hi.x(); ++x, ++xr) {
                const Float weight = m_weights_x[xr] * weightY;

                for (int k = 0; k < channels; ++k)
                    *dest++ += weight * value[k];
            }
        }
        return true;
    }
}

bool ImageBlock::put(const Point2fP &_pos, const FloatP *value) {
    using Mask = mask_t<FloatP>;

    const int channels = m_bitmap->channel_count();

    // Check if all sample values are valid
    Mask is_valid(true);
    if (m_warn) {
        for (int i = 0; i < channels; ++i)
            is_valid &= (enoki::isfinite(value[i]) & value[i] >= 0);
    }

    if (any(is_valid)) {
        const Float filter_radius = m_filter->radius();
        const Vector2i &size = m_bitmap->size();

        // Convert to pixel coordinates within the image block
        const Point2fP pos(
            _pos.x() - 0.5f - (m_offset.x() - m_border_size),
            _pos.y() - 0.5f - (m_offset.y() - m_border_size)
        );

        // Determine the affected range of pixels
        const Point2iP lo(max((Int32P)ceil(pos.x() - filter_radius), 0),
            max((Int32P)ceil(pos.y() - filter_radius), 0));
        const Point2iP hi(min((Int32P)floor(pos.x() + filter_radius), size.x() - 1),
            min((Int32P)floor(pos.y() + filter_radius), size.y() - 1));

        // Lookup values from the pre-rasterized filter
        Vector2iP pixel_range = hi - lo;

        int max_range_x = hmax(pixel_range.x());
        int max_range_y = hmax(pixel_range.y());

        std::vector<FloatP> weights_x(max_range_x);
        for (int idx = 0; idx <= max_range_x; ++idx) {
            weights_x[idx] = m_filter->eval_discretized(lo.x() + idx - pos.x());
        }

        std::vector<FloatP> weights_y(max_range_y);
        for (int idx = 0; idx <= max_range_y; ++idx) {
            weights_y[idx] = m_filter->eval_discretized(lo.y() + idx - pos.y());
        }

        // Rasterize the filtered sample into the framebuffer
        for (int yr = 0; yr <= max_range_y; ++yr) {
            const FloatP &weight_y = weights_y[yr];

            SizeP offset_y = ((lo.y() + yr) * (size_t)size.x() + lo.x()) * channels;

            for (int xr = 0; xr <= max_range_x; ++xr) {
                const FloatP weight = weights_x[xr] * weight_y;

                // Ensure we are not writing out of the filter range
                Mask invalid_store = pixel_range.y() < yr || pixel_range.x() < xr || !is_valid;
                if (all(invalid_store)) continue;

                for (int k = 0; k < channels; ++k) {
                    FloatP val = gather<FloatP>(m_bitmap->data(), k + xr + offset_y, !invalid_store);
                    val += select(invalid_store, 0.f, weight * value[k]);
                    scatter(m_bitmap->data(), val, k + xr + offset_y, !invalid_store);
                }
            }
        }
    }

    if (m_warn && any(!is_valid)) {
        std::ostringstream oss;
        oss << "Invalid sample value : [";
        for (int i = 0; i < channels; ++i) {
            oss << value[i];
            if (i + 1 < channels)
                oss << ", ";
        }
        oss << "]";
        Log(EWarn, "%s", oss.str());

        return false;
    }

    return true;
}

std::string ImageBlock::to_string() const {
    std::ostringstream oss;
    oss << "ImageBlock[" << std::endl
        << "  offset = " << m_offset << "," << std::endl
        << "  size = "   << m_size   << "," << std::endl
        << "  border_size = " << m_border_size << std::endl
        << "]";
    return oss.str();
}


template MTS_EXPORT_RENDER bool ImageBlock::put<>(const Point2f &pos, const Spectrumf &spec, Float alpha);
template MTS_EXPORT_RENDER bool ImageBlock::put<>(const Point2fP &pos, const SpectrumfP &spec, FloatP alpha);


MTS_IMPLEMENT_CLASS(ImageBlock, Object)


NAMESPACE_END(mitsuba)
