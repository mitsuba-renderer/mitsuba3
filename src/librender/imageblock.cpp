#include <mitsuba/render/imageblock.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
ImageBlock<Float, Spectrum>::ImageBlock(Bitmap::EPixelFormat fmt, const Vector2i &size,
                                        const ReconstructionFilter *filter, size_t channels,
                                        bool warn, bool border, bool normalize)
    : m_offset(0), m_size(size), m_filter(filter), m_weights_x(nullptr), m_weights_y(nullptr),
      m_warn(warn), m_normalize(normalize) {
    m_border_size = (filter != nullptr && border) ? filter->border_size() : 0;

    // Allocate a small bitmap data structure for the block
    m_bitmap = new Bitmap(fmt, Struct::EType::EFloat,
                          size + Vector2i(2 * m_border_size), channels);
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

// TODO(!): `active` mask should be used!
template <typename Float, typename Spectrum>
bool ImageBlock<Float, Spectrum>::put(const Point2f &pos_, const Float *value, Mask /*active*/) {
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
            Log(Warn, "%s", oss.str());
            return false;
        }
    }

    Float filter_radius = m_filter->radius();
    Vector2i size = m_bitmap->size();

    // Convert to pixel coordinates within the image block
    Point2f pos = pos_ - (m_offset - m_border_size + .5f);

    // Determine the affected range of pixels
    Point2i lo = max(ceil2int <Point2i>(pos - filter_radius), 0),
            hi = min(floor2int<Point2i>(pos + filter_radius), size - 1);

    // Lookup values from the pre-rasterized filter
    for (int x = lo.x(), idx = 0; x <= hi.x(); ++x)
        m_weights_x[idx++] = m_filter->eval_discretized(x - pos.x());

    for (int y = lo.y(), idx = 0; y <= hi.y(); ++y)
        m_weights_y[idx++] = m_filter->eval_discretized(y - pos.y());

    if (ENOKI_UNLIKELY(m_normalize)) {
        Float wx = 0.f, wy = 0.f;
        for (int x = 0; x <= hi.x() - lo.x(); ++x)
            wx += m_weights_x[x];
        for (int y = 0; y <= hi.y() - lo.y(); ++y)
            wy += m_weights_y[y];
        Float factor = rcp(wx * wy);
        for (int x = 0; x <= hi.x() - lo.x(); ++x)
            m_weights_x[x] *= factor;
    }

    // Rasterize the filtered sample into the framebuffer
    for (int y = lo.y(), yr = 0; y <= hi.y(); ++y, ++yr) {
        const Float weightY = m_weights_y[yr];
        Float *dest = ((Float *) m_bitmap->data())
            + (y * (size_t) size.x() + lo.x()) * channels;

        for (int x = lo.x(), xr = 0; x <= hi.x(); ++x, ++xr) {
            const Float weight = m_weights_x[xr] * weightY;

            for (size_t k = 0; k < channels; ++k)
                *dest++ += weight * value[k];
        }
    }

    return true;
}

#if 0
// TODO: merge the scalar, vector & differentiable implementations if possible
template <typename Float, typename Spectrum>
MaskP ImageBlock<Float, Spectrum>::put(const Point2fP &pos_, const FloatP *value, MaskP active) {
    Assert(m_filter != nullptr);
    using Mask = mask_t<FloatP>;

    uint32_t channels = (uint32_t) m_bitmap->channel_count();

    // Check if all sample values are valid
    if (m_warn) {
        Mask is_valid = true;
        for (size_t k = 0; k < channels; ++k)
            is_valid &= enoki::isfinite(value[k]) && value[k] >= 0;

        if (unlikely(any(active && !is_valid))) {
            std::ostringstream oss;
            for (size_t i = 0; i < PacketSize; ++i) {
                if (is_valid[i])
                    continue;
                oss << "Invalid sample value(s): [";
                for (uint32_t j = 0; j < channels; ++j) {
                    oss << value[j].coeff(i);
                    if (j + 1 < channels)
                        oss << ", ";
                }
            }
            oss << "]";
            Log(Warn, "%s", oss.str());
            active &= is_valid;
        }
    }

    Float filter_radius = m_filter->radius();
    Vector2i size = m_bitmap->size();

    // Convert to pixel coordinates within the image block
    Point2fP pos = pos_ - (m_offset - m_border_size + .5f);

    // Determine the affected range of pixels
    Point2iP lo = max(ceil2int <Point2iP>(pos - filter_radius), 0),
             hi = min(floor2int<Point2iP>(pos + filter_radius), size - 1);
    Vector2iP window_size = hi - lo;

    // Lookup values from the pre-rasterized filter
    Point2i max_size(hmax(window_size.x()),
                     hmax(window_size.y()));

    Point2fP base = lo - pos;
    ENOKI_NOUNROLL for (int i = 0; i <= max_size.x(); ++i)
        m_weights_x_p[i] = m_filter->eval_discretized(base.x() + (Float) i, active);

    ENOKI_NOUNROLL for (int i = 0; i <= max_size.y(); ++i)
        m_weights_y_p[i] = m_filter->eval_discretized(base.y() + (Float) i, active);

    if (ENOKI_UNLIKELY(m_normalize)) {
        FloatP wx = 0.f, wy = 0.f;
        for (int x = 0; x <= max_size.x(); ++x)
            wx += m_weights_x_p[x];
        for (int y = 0; y <= max_size.y(); ++y)
            wy += m_weights_y_p[y];
        FloatP factor = rcp(wx * wy);
        for (int x = 0; x <= max_size.x(); ++x)
            m_weights_x_p[x] *= factor;
    }

    // Rasterize the filtered sample into the framebuffer
    Float *buffer = (Float *) m_bitmap->data();
    ENOKI_NOUNROLL for (int yr = 0; yr <= max_size.y(); ++yr) {
        Mask enabled = active && yr <= window_size.y();
        Int32P y = lo.y() + yr;

        ENOKI_NOUNROLL for (int xr = 0; xr <= max_size.x(); ++xr) {
            Int32P x      = lo.x() + xr,
                   offset = channels * (y * size.x() + x);
            FloatP weights = m_weights_y_p[yr] * m_weights_x_p[xr];

            enabled &= xr <= window_size.x();
            ENOKI_NOUNROLL for (uint32_t k = 0; k < channels; ++k)
                scatter_add(buffer + k, weights * value[k], offset, enabled);
        }
    }

    return active;
}
#endif

#if defined(MTS_ENABLE_AUTODIFF)
template <typename Float, typename Spectrum>
MaskD ImageBlock<Float, Spectrum>::put(const Point2fD &pos_, const FloatD *value, MaskD active) {
    Assert(m_filter != nullptr);

    uint32_t channels = (uint32_t) m_bitmap->channel_count();
    MaskD is_valid(true);

    // Check if all sample values are valid
    if (m_warn) {
        MaskD is_valid = true;
        for (size_t k = 0; k < channels; ++k)
            is_valid &= enoki::isfinite(value[k]) && value[k] >= 0;

        if (any(active && !is_valid))
            Log(Warn, "ImageBlock::put(): invalid (negative/NaN) sample values detected!");

        active &= is_valid;
    }

    Vector2i size = Vector2i(m_bitmap->size());

    // Convert to pixel coordinates within the image block
    Point2fD pos = pos_ - (m_offset - m_border_size + .5f);

    // Determine the affected range of pixels
    Float filter_radius = m_filter->radius();
    Point2iD lo = max(ceil2int <Point2iD>(pos - filter_radius), 0),
             hi = min(floor2int<Point2iD>(pos + filter_radius), size - 1);

    Point2fD base = lo - pos;

    while (m_bitmap->channel_count() != m_bitmap_d.size())
        m_bitmap_d.push_back(zero<FloatD>(hprod(m_bitmap->size())));

    /// Subtraction of 2*math::Epsilon ensures that we only do 1 scatter_add
    /// instead of the conservative 4 when using the box filter
    int n = (int) std::ceil((m_filter->radius() - 2*math::Epsilon) * 2);
    std::vector<FloatD> weights_x_d(n), weights_y_d(n);

    for (int k = 0; k < n; ++k) {
        Point2fD p = base + (Float) k;
        weights_x_d[k] = m_filter->eval(p.x());
        weights_y_d[k] = m_filter->eval(p.y());
    }

    if (ENOKI_UNLIKELY(m_normalize)) {
        FloatD wx = 0.f, wy = 0.f;
        for (int k = 0; k < n; ++k) {
            wx += weights_x_d[k];
            wy += weights_y_d[k];
        }

        FloatD factor = rcp(wx * wy);
        for (int k = 0; k < n; ++k)
            weights_x_d[k] *= factor;
    }

    for (int yr = 0; yr < n; ++yr) {
        Int32D y = lo.y() + yr;
        MaskD enabled = active && y <= hi.y();

        for (int xr = 0; xr < n; ++xr) {
            Int32D x     = lo.x() + xr,
                   index = y * (int32_t) size.x() + x;

            enabled &= x <= hi.x();

            FloatD weight = weights_x_d[xr] * weights_y_d[yr];
            for (uint32_t k = 0; k < channels; ++k)
                scatter_add(m_bitmap_d[k], value[k] * weight, index, enabled);
        }
    }

    return active;
}

template <typename Float, typename Spectrum>
void ImageBlock<Float, Spectrum>::clear_d() {
    m_bitmap_d.resize(m_bitmap->channel_count());
    for (size_t i = 0; i < m_bitmap_d.size(); ++i)
        m_bitmap_d[i] = zero<FloatD>(hprod(m_bitmap->size()));
}

#endif

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

MTS_INSTANTIATE_OBJECT(ImageBlock)
NAMESPACE_END(mitsuba)
