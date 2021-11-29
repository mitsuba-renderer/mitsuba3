#include <mitsuba/render/imageblock.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/profiler.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT
ImageBlock<Float, Spectrum>::ImageBlock(const ScalarVector2i &size, size_t channel_count,
                                        const ReconstructionFilter *filter, bool warn_negative,
                                        bool warn_invalid, bool border, bool normalize)
    : m_offset(0), m_size(0), m_channel_count((uint32_t) channel_count), m_filter(filter),
      m_weights_x(nullptr), m_weights_y(nullptr), m_warn_negative(warn_negative),
      m_warn_invalid(warn_invalid), m_normalize(normalize) {
    m_border_size = (filter && border) ? filter->border_size() : 0;

    if (filter) {
        // Temporary buffers used in put()
        int filter_size = (int) ek::ceil(2 * filter->radius()) + 1;
        m_weights_x = new Float[2 * filter_size];
        m_weights_y = m_weights_x + filter_size;
    }

    set_size(size);
}

MTS_VARIANT
ImageBlock<Float, Spectrum>::ImageBlock(const TensorXf &data,
                                        const ReconstructionFilter *filter,
                                        bool warn_negative, bool warn_invalid,
                                        bool normalize)
    : m_offset(0), m_border_size(0), m_filter(filter), m_weights_x(nullptr),
      m_weights_y(nullptr), m_warn_negative(warn_negative),
      m_warn_invalid(warn_invalid), m_normalize(normalize) {
    if (filter) {
        // Temporary buffers used in put()
        int filter_size = (int) ek::ceil(2 * filter->radius()) + 1;
        m_weights_x = new Float[2 * filter_size];
        m_weights_y = m_weights_x + filter_size;
    }

    m_size = ScalarVector2i((int32_t) data.shape(1),
                            (int32_t) data.shape(0));
    m_channel_count = (uint32_t) data.shape(2);
    if constexpr (ek::is_jit_array_v<Float>)
        m_data = TensorXf(data.array().copy(), 3, data.shape().data());
    else
        m_data = TensorXf(data.array(), 3, data.shape().data());
}

MTS_VARIANT ImageBlock<Float, Spectrum>::~ImageBlock() {
    /* Note that m_weights_y points to the same array as
       m_weights_x, so there is no need to delete it. */
    if (m_weights_x)
        delete[] m_weights_x;
}

MTS_VARIANT void ImageBlock<Float, Spectrum>::clear() {
    ScalarVector2i size = m_size + 2 * m_border_size;
    size_t width = m_channel_count * ek::hprod(size);
    size_t shape[3] = { (size_t) size.y(), (size_t) size.x(), m_channel_count };
    m_data = TensorXf(ek::zero<DynamicBuffer<Float>>(width), 3, shape);
}

MTS_VARIANT void
ImageBlock<Float, Spectrum>::set_size(const ScalarVector2i &new_size) {
    if (new_size == m_size)
        return;
    m_size = new_size;

    ScalarVector2i size = m_size + 2 * m_border_size;
    size_t width = m_channel_count * ek::hprod(size);
    size_t shape[3] = { (size_t) size.y(), (size_t) size.x(), m_channel_count };
    m_data = TensorXf(ek::empty<DynamicBuffer<Float>>(width), 3, shape);
}

MTS_VARIANT void ImageBlock<Float, Spectrum>::put(const ImageBlock *block) {
    ScopedPhase sp(ProfilerPhase::ImageBlockPut);

    if (unlikely(block->channel_count() != channel_count()))
        Throw("ImageBlock::put(): mismatched channel counts!");

    ScalarVector2i source_size   = block->size() + 2 * block->border_size(),
                   target_size   =        size() + 2 *        border_size();

    ScalarPoint2i  source_offset = block->offset() - block->border_size(),
                   target_offset =        offset() -        border_size();

    if constexpr (ek::is_jit_array_v<Float>) {
        // If target block is cleared and match size, directly copy data
        if (m_data.array().is_literal() && m_data.array()[0] == 0.f &&
            m_size == block->size() && m_offset == block->offset() &&
            m_border_size == block->border_size()) {
            m_data.array() = block->data().array().copy();
        } else {
            accumulate_2d<Float &, const Float &>(
                block->data().array(), source_size,
                m_data.array(), target_size,
                ScalarVector2i(0), source_offset - target_offset,
                source_size, channel_count()
            );
        }
    } else {
        accumulate_2d(
            block->data().data(), source_size,
            m_data.data(), target_size,
            ScalarVector2i(0), source_offset - target_offset,
            source_size, channel_count()
        );
    }
}

MTS_VARIANT typename ImageBlock<Float, Spectrum>::Mask
ImageBlock<Float, Spectrum>::put(const Point2f &pos_, const Float *value, Mask active) {
    ScopedPhase sp(ProfilerPhase::ImageBlockPut);
    Assert(m_filter != nullptr);

    // Check if all sample values are valid
    if (likely(m_warn_negative || m_warn_invalid)) {
        Mask is_valid = true;

        if (m_warn_negative) {
            for (uint32_t k = 0; k < m_channel_count; ++k)
                is_valid &= value[k] >= -1e-5f;
        }

        if (m_warn_invalid) {
            for (uint32_t k = 0; k < m_channel_count; ++k)
                is_valid &= ek::isfinite(value[k]);
        }

        if (unlikely(ek::any(active && !is_valid))) {
            std::ostringstream oss;
            oss << "Invalid sample value: [";
            for (uint32_t i = 0; i < m_channel_count; ++i) {
                oss << value[i];
                if (i + 1 < m_channel_count) oss << ", ";
            }
            oss << "]";
            Log(Warn, "%s", oss.str());
        }
    }

    ScalarFloat filter_radius = m_filter->radius();
    ScalarVector2i size = m_size + 2 * m_border_size;

    // Convert to pixel coordinates within the image block
    Point2f pos = pos_ - (m_offset - m_border_size + 0.5f);

    if (filter_radius > 0.5f + math::RayEpsilon<Float>) {
        // Determine the affected range of pixels
        Point2u lo = Point2u(ek::max(ek::ceil2int <Point2i>(pos - filter_radius), 0)),
                hi = Point2u(ek::min(ek::floor2int<Point2i>(pos + filter_radius), size - 1));

        uint32_t n = ek::ceil2int<uint32_t>(
            (filter_radius - 2.f * math::RayEpsilon<ScalarFloat>) * 2.f);
        Assert(n <= ek::ceil(2 * filter_radius) + 1);

        Point2f base = lo - pos;
        for (uint32_t i = 0; i < n; ++i) {
            Point2f p = base + i;
            if constexpr (!ek::is_jit_array_v<Float>) {
                m_weights_x[i] = m_filter->eval_discretized(p.x(), active);
                m_weights_y[i] = m_filter->eval_discretized(p.y(), active);
            } else {
                m_weights_x[i] = m_filter->eval(p.x(), active);
                m_weights_y[i] = m_filter->eval(p.y(), active);
            }
        }

        if (unlikely(m_normalize)) {
            Float wx(0), wy(0);
            for (uint32_t i = 0; i < n; ++i) {
                wx += m_weights_x[i];
                wy += m_weights_y[i];
            }

            Float factor = ek::detach(ek::rcp(wx * wy));
            for (uint32_t i = 0; i < n; ++i)
                m_weights_x[i] *= factor;
        }

        ENOKI_NOUNROLL for (uint32_t yr = 0; yr < n; ++yr) {
            UInt32 y = lo.y() + yr;
            Mask enabled = active && y <= hi.y();

            ENOKI_NOUNROLL for (uint32_t xr = 0; xr < n; ++xr) {
                UInt32 x       = lo.x() + xr,
                       offset  = m_channel_count * (y * size.x() + x);
                Float weight = m_weights_y[yr] * m_weights_x[xr];

                enabled &= x <= hi.x();
                ENOKI_NOUNROLL for (uint32_t k = 0; k < m_channel_count; ++k)
                    ek::scatter_reduce(ReduceOp::Add, m_data.array(),
                                       value[k] * weight, offset + k, enabled);
            }
        }
    } else {
        Point2i lo = ek::ceil2int<Point2i>(pos - .5f);
        UInt32 offset = m_channel_count * (lo.y() * size.x() + lo.x());

        Mask enabled = active && ek::all(lo >= 0 && lo < size);
        ENOKI_NOUNROLL for (uint32_t k = 0; k < m_channel_count; ++k)
            ek::scatter_reduce(ReduceOp::Add, m_data.array(), value[k],
                               offset + k, enabled);
    }

    return active;
}

MTS_VARIANT void
ImageBlock<Float, Spectrum>::read(const Point2f &pos_, Float *output, Mask active) {
    // Convert to pixel coordinates within the image block
    Point2f pos = pos_ - m_offset;

    if (m_border_size == 0)
        active &= ek::all(pos >= 0.0) && ek::all(pos < m_size);

    pos += m_border_size - 0.5f;

    ScalarVector2i size = m_size + 2 * m_border_size;

    if (m_filter != nullptr && m_filter->radius() > 0.5f + math::RayEpsilon<Float>) {
        ScalarFloat filter_radius = m_filter->radius();

        // Determine the affected range of pixels
        Point2u lo = Point2u(ek::max(ek::ceil2int <Point2i>(pos - filter_radius), 0)),
                hi = Point2u(ek::min(ek::floor2int<Point2i>(pos + filter_radius), size - 1));

        uint32_t n = ek::ceil2int<uint32_t>(
            (filter_radius - 2.f * math::RayEpsilon<ScalarFloat>) * 2.f);
        Assert(n <= ek::ceil(2 * filter_radius) + 1);

        Point2f base = lo - pos;
        for (uint32_t i = 0; i < n; ++i) {
            Point2f p = base + i;
            if constexpr (!ek::is_jit_array_v<Float>) {
                m_weights_x[i] = m_filter->eval_discretized(p.x(), active);
                m_weights_y[i] = m_filter->eval_discretized(p.y(), active);
            } else {
                m_weights_x[i] = m_filter->eval(p.x(), active);
                m_weights_y[i] = m_filter->eval(p.y(), active);
            }
        }

        Float factor = 1.f;
        if (unlikely(m_normalize)) {
            Float weight = 0.f;
            for (uint32_t yr = 0; yr < n; ++yr) {
                UInt32 y = lo.y() + yr;
                Mask enabled = active && y <= hi.y();
                for (uint32_t xr = 0; xr < n; ++xr) {
                    UInt32 x = lo.x() + xr;
                    enabled &= x <= hi.x();
                    if (m_border_size == 0)
                        ek::masked(weight, enabled) += m_weights_y[yr] * m_weights_x[xr];
                    else
                        weight += m_weights_y[yr] * m_weights_x[xr];
                }
            }
            factor = ek::detach(ek::select(weight > 0.f, ek::rcp(weight), 1.f));
        }

        for (uint32_t k = 0; k < m_channel_count; ++k)
            output[k] = ek::zero<Float>(ek::width(pos));

        ENOKI_NOUNROLL for (uint32_t yr = 0; yr < n; ++yr) {
            UInt32 y = lo.y() + yr;
            Mask enabled = active && y <= hi.y();

            ENOKI_NOUNROLL for (uint32_t xr = 0; xr < n; ++xr) {
                UInt32 x       = lo.x() + xr,
                       offset  = m_channel_count * (y * size.x() + x);
                Float weight = m_weights_y[yr] * m_weights_x[xr] * factor;

                enabled &= x <= hi.x();
                ENOKI_NOUNROLL for (uint32_t k = 0; k < m_channel_count; ++k)
                    output[k] += ek::gather<Float>(m_data.array(), offset + k, enabled) * weight;
            }
        }
    } else {
        Point2i lo = ek::ceil2int<Point2i>(pos - .5f);
        UInt32 offset = m_channel_count * (lo.y() * size.x() + lo.x());

        Mask enabled = active && ek::all(lo >= 0 && lo < size);
        ENOKI_NOUNROLL for (uint32_t k = 0; k < m_channel_count; ++k)
            output[k] = ek::gather<Float>(m_data.array(), offset + k, enabled);
    }
}

MTS_VARIANT void
ImageBlock<Float, Spectrum>::overwrite_channel(size_t channel, const Float &value) {
    // Include the border on each side
    size_t pixel_count = ek::hprod(m_size + 2 * m_border_size);
    Assert(ek::width(value) == 1 || ek::width(value) == pixel_count);

    auto indices =
        m_channel_count * ek::arange<DynamicBuffer<UInt64>>(pixel_count) + channel;
    ek::scatter(m_data.array(), DynamicBuffer<Float>(value), indices);
}

MTS_VARIANT std::string ImageBlock<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
    oss << "ImageBlock[" << std::endl
        << "  offset = " << m_offset << "," << std::endl
        << "  size = "   << m_size << "," << std::endl
        << "  warn_negative = " << m_warn_negative << "," << std::endl
        << "  warn_invalid = " << m_warn_invalid << "," << std::endl
        << "  border_size = " << m_border_size;
    if (m_filter)
        oss << "," << std::endl << "  filter = " << string::indent(m_filter);
    oss << std::endl
        << "]";
    return oss.str();
}

MTS_IMPLEMENT_CLASS_VARIANT(ImageBlock, Object)
MTS_INSTANTIATE_CLASS(ImageBlock)
NAMESPACE_END(mitsuba)
