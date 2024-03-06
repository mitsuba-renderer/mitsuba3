#include <mitsuba/render/imageblock.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/profiler.h>
#include <drjit/while_loop.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT
ImageBlock<Float, Spectrum>::ImageBlock(const ScalarVector2u &size,
                                        const ScalarPoint2i &offset,
                                        uint32_t channel_count,
                                        const ReconstructionFilter *rfilter,
                                        bool border, bool normalize,
                                        bool coalesce, bool compensate,
                                        bool warn_negative, bool warn_invalid)
    : m_offset(offset), m_size(0), m_channel_count(channel_count),
      m_rfilter(rfilter), m_normalize(normalize), m_coalesce(coalesce),
      m_compensate(compensate), m_warn_negative(warn_negative),
      m_warn_invalid(warn_invalid) {

    // Detect if a box filter is being used, and just discard it in that case
    if (rfilter && rfilter->is_box_filter())
        m_rfilter = nullptr;

    // Determine the size of the boundary region from the reconstruction filter
    m_border_size = (m_rfilter && border) ? m_rfilter->border_size() : 0u;

    // Allocate memory for the image tensor
    set_size(size);
}

MI_VARIANT
ImageBlock<Float, Spectrum>::ImageBlock(const TensorXf &tensor,
                                        const ScalarPoint2i &offset,
                                        const ReconstructionFilter *rfilter,
                                        bool border, bool normalize,
                                        bool coalesce, bool compensate,
                                        bool warn_negative, bool warn_invalid)
    : m_offset(offset), m_rfilter(rfilter), m_normalize(normalize),
      m_coalesce(coalesce), m_compensate(compensate),
      m_warn_negative(warn_negative), m_warn_invalid(warn_invalid) {

    if (tensor.ndim() != 3)
        Throw("ImageBlock(const TensorXf&): expected a 3D tensor (height x width x channels)!");

    // Detect if a box filter is being used, and just discard it in that case
    if (rfilter && rfilter->is_box_filter())
        m_rfilter = nullptr;

    // Determine the size of the boundary region from the reconstruction filter
    m_border_size = (m_rfilter && border) ? m_rfilter->border_size() : 0u;

    m_size = ScalarVector2u((uint32_t) tensor.shape(1), (uint32_t) tensor.shape(0));
    m_channel_count = (uint32_t) tensor.shape(2);

    // Account for the boundary region, if present
    if (border && dr::any(m_size < 2 * m_border_size))
        Throw("ImageBlock(const TensorXf&): image is too small to have a boundary!");
    m_size -= 2 * m_border_size;

    // Copy the image tensor
    if constexpr (dr::is_jit_v<Float>)
        m_tensor = TensorXf(tensor.array().copy(), 3, tensor.shape().data());
    else
        m_tensor = TensorXf(tensor.array(), 3, tensor.shape().data());
}

MI_VARIANT ImageBlock<Float, Spectrum>::~ImageBlock() { }

MI_VARIANT void ImageBlock<Float, Spectrum>::clear() {
    using Array = typename TensorXf::Array;

    ScalarVector2u size_ext = m_size + 2 * m_border_size;

    size_t size_flat = m_channel_count * dr::prod(size_ext),
           shape[3]  = { size_ext.y(), size_ext.x(), m_channel_count };

    m_tensor = TensorXf(dr::zeros<Array>(size_flat), 3, shape);

    if (m_compensate)
        m_tensor_compensation = TensorXf(dr::zeros<Array>(size_flat), 3, shape);
}

MI_VARIANT void
ImageBlock<Float, Spectrum>::set_size(const ScalarVector2u &size) {
    using Array = typename TensorXf::Array;

    if (dr::all(size == m_size))
        return;

    ScalarVector2u size_ext = size + 2 * m_border_size;

    size_t size_flat = m_channel_count * dr::prod(size_ext),
           shape[3]  = { size_ext.y(), size_ext.x(), m_channel_count };

    m_tensor = TensorXf(dr::zeros<Array>(size_flat), 3, shape);

    if (m_compensate)
        m_tensor_compensation = TensorXf(dr::zeros<Array>(size_flat), 3, shape);

    m_size = size;
}

MI_VARIANT typename ImageBlock<Float, Spectrum>::TensorXf &ImageBlock<Float, Spectrum>::tensor() {
    if constexpr (dr::is_jit_v<Float>) {
        if (m_compensate) {
            Float &comp = m_tensor_compensation.array();
            m_tensor.array() += comp;
            comp = dr::zeros<Float>(comp.size());
        }
    }
    return m_tensor;
}

MI_VARIANT const typename ImageBlock<Float, Spectrum>::TensorXf &ImageBlock<Float, Spectrum>::tensor() const {
    return const_cast<ImageBlock&>(*this).tensor();
}

MI_VARIANT void ImageBlock<Float, Spectrum>::accum(Float value, UInt32 index, Bool active) {
    if constexpr (dr::is_jit_v<Float>) {
        if (m_compensate)
            dr::scatter_add_kahan(m_tensor.array(),
                                  m_tensor_compensation.array(),
                                  value, index, active);
        else
            dr::scatter_reduce(ReduceOp::Add, m_tensor.array(),
                               value, index, active);
    } else {
        DRJIT_MARK_USED(value);
        DRJIT_MARK_USED(index);
        DRJIT_MARK_USED(active);
    }
}

MI_VARIANT void ImageBlock<Float, Spectrum>::put_block(const ImageBlock *block) {
    ScopedPhase sp(ProfilerPhase::ImageBlockPut);

    if (unlikely(block->channel_count() != channel_count()))
        Throw("ImageBlock::put_block(): mismatched channel counts! (%u, "
              "expected %u)", block->channel_count(), channel_count());

    ScalarVector2u source_size   = block->size() + 2 * block->border_size(),
                   target_size   =        size() + 2 *        border_size();

    ScalarPoint2i  source_offset = block->offset() - block->border_size(),
                   target_offset =        offset() -        border_size();

    if constexpr (dr::is_jit_v<Float>) {
        // If target block is cleared and match size, directly copy data
        if (dr::all(m_size == block->size() && m_offset == block->offset() &&
            m_border_size == block->border_size())) {
            if (m_tensor.array().state() == VarState::Literal && m_tensor.array()[0] == 0.f)
                m_tensor.array() = block->tensor().array();
            else
                m_tensor.array() += block->tensor().array();
        } else {
            accumulate_2d<Float &, const Float &>(
                block->tensor().array(), source_size,
                m_tensor.array(), target_size,
                ScalarVector2i(0), source_offset - target_offset,
                source_size, channel_count()
            );
        }
    } else {
        accumulate_2d(
            block->tensor().data(), source_size,
            m_tensor.data(), target_size,
            ScalarVector2i(0), source_offset - target_offset,
            source_size, channel_count()
        );
    }
}

MI_VARIANT void ImageBlock<Float, Spectrum>::put(const Point2f &pos,
                                                 const Float *values,
                                                 Mask active) {
    ScopedPhase sp(ProfilerPhase::ImageBlockPut);
    constexpr bool JIT = dr::is_jit_v<Float>;

    // Check if all sample values are valid
    if (m_warn_negative || m_warn_invalid) {
        Mask is_valid = true;

        if (m_warn_negative) {
            for (uint32_t k = 0; k < m_channel_count; ++k)
                is_valid &= values[k] >= -1e-5f;
        }

        if (m_warn_invalid) {
            for (uint32_t k = 0; k < m_channel_count; ++k)
                is_valid &= dr::isfinite(values[k]);
        }

        if (unlikely(dr::any(active && !is_valid))) {
            std::ostringstream oss;
            oss << "Invalid sample value: [";
            for (uint32_t i = 0; i < m_channel_count; ++i) {
                oss << values[i];
                if (i + 1 < m_channel_count) oss << ", ";
            }
            oss << "]";
            Log(Warn, "%s", oss.str());
        }
    }

    // ===================================================================
    //  Fast special case for the box filter
    // ===================================================================

    if (!m_rfilter) {
        Point2u p = Point2u(dr::floor2int<Point2i>(pos) - m_offset);

        // Switch over to unsigned integers, compute pixel index
        UInt32 index = dr::fmadd(p.y(), m_size.x(), p.x()) * m_channel_count;

        // The sample could be out of bounds
        active &= dr::all(p < m_size);

        // Accumulate!
        if constexpr (!JIT) {
            if (unlikely(!active))
                return;

            ScalarFloat *ptr = m_tensor.array().data() + index;
            for (uint32_t k = 0; k < m_channel_count; ++k)
                *ptr++ += values[k];
        } else {
            for (uint32_t k = 0; k < m_channel_count; ++k)
                accum(values[k], index++, active);
        }

        return;
    }

    // ===================================================================
    // Prelude for the general case
    // ===================================================================

    ScalarFloat radius = m_rfilter->radius();

    // Size of the underlying image buffer
    ScalarVector2u size = m_size + 2 * m_border_size;

    // Check if the operation can be performed using a recorded loop
    bool record_loop = false;

    // TODO: Maybe revisit this now that nanobind can differentiate through loops
    if constexpr (JIT) {
        record_loop = jit_flag(JitFlag::LoopRecord) && !m_normalize;

        if constexpr (dr::is_diff_v<Float>) {
            record_loop = record_loop &&
                          !dr::grad_enabled(pos) &&
                          !dr::grad_enabled(m_tensor);

            for (uint32_t k = 0; k < m_channel_count; ++k)
                record_loop = record_loop && !dr::grad_enabled(values[k]);
        }
    }

    // ===================================================================
    // 1. Non-coalesced accumulation method (see ImageBlock constructor)
    // ===================================================================

    if (!JIT || !m_coalesce) {
        Point2f pos_f   = pos + ((int) m_border_size - m_offset - .5f),
                pos_0_f = pos_f - radius,
                pos_1_f = pos_f + radius;

        // Interval specifying the pixels covered by the filter
        Point2u pos_0_u = Point2u(dr::maximum(dr::ceil2int <Point2i>(pos_0_f), ScalarPoint2i(0))),
                pos_1_u = Point2u(dr::minimum(dr::floor2int<Point2i>(pos_1_f), ScalarPoint2i(size - 1))),
                count_u = pos_1_u - pos_0_u + 1u;

        // Base index of the top left corner
        UInt32 index =
            dr::fmadd(pos_0_u.y(), size.x(), pos_0_u.x()) * m_channel_count;

        // Compute the number of filter evaluations needed along each axis
        ScalarVector2u count;
        uint32_t count_max = dr::ceil2int<uint32_t>(2.f * radius);
        if constexpr (!JIT) {
            if (dr::any(pos_0_u > pos_1_u))
                return;
            count = count_u;
        } else {
            // Conservative bounds must be used in the vectorized case
            count = count_max;
            active &= dr::all(pos_0_u <= pos_1_u);
        }

        Point2f rel_f = Point2f(pos_0_u) - pos_f;

        if (!record_loop) {
            // ===========================================================
            // 1.1. Scalar mode / unroll the complete loop
            // ===========================================================

            // Allocate memory for reconstruction filter weights on the stack
            Float *weights_x = (Float *) alloca(sizeof(Float) * count.x()),
                  *weights_y = (Float *) alloca(sizeof(Float) * count.y());

            // Evaluate filters weights along the X and Y axes
            for (uint32_t x = 0; x < count.x(); ++x) {
                new (weights_x + x)
                    Float(JIT ? m_rfilter->eval(rel_f.x())
                              : m_rfilter->eval_discretized(rel_f.x()));
                rel_f.x() += 1.f;
            }

            for (uint32_t y = 0; y < count.y(); ++y) {
                new (weights_y + y)
                    Float(JIT ? m_rfilter->eval(rel_f.y())
                              : m_rfilter->eval_discretized(rel_f.y()));
                rel_f.y() += 1.f;
            }

            // Normalize sample contribution if desired
            if (unlikely(m_normalize)) {
                Float wx = 0.f, wy = 0.f;

                Point2f rel_f2 = dr::ceil(pos_0_f) - pos_f;
                for (uint32_t i = 0; i < count_max; ++i) {
                    wx += JIT ? m_rfilter->eval(rel_f2.x())
                              : m_rfilter->eval_discretized(rel_f2.x());
                    wy += JIT ? m_rfilter->eval(rel_f2.y())
                              : m_rfilter->eval_discretized(rel_f2.y());
                    rel_f2 += 1.f;
                }

                Float factor = dr::detach(wx * wy);

                if constexpr (JIT) {
                    factor = dr::select(factor != 0.f, dr::rcp(factor), 0.f);
                } else {
                    if (unlikely(factor == 0))
                        return;
                    factor = dr::rcp(factor);
                }

                for (uint32_t i = 0; i < count.x(); ++i)
                    weights_x[i] *= factor;
            }

            ScalarFloat *ptr = nullptr;
            if constexpr (!JIT)
                ptr = m_tensor.array().data();
            else
                (void) ptr;

            // Accumulate!
            for (uint32_t y = 0; y < count.y(); ++y) {
                Mask active_1 = active && y < count_u.y();

                for (uint32_t x = 0; x < count.x(); ++x) {
                    Mask active_2 = active_1 && x < count_u.x();

                    for (uint32_t k = 0; k < m_channel_count; ++k) {
                        Float weight = weights_x[x] * weights_y[y];

                        if constexpr (!JIT) {
                            DRJIT_MARK_USED(active_2);
                            ptr[index] = dr::fmadd(values[k], weight, ptr[index]);
                        } else {
                            accum(values[k] * weight, index, active_2);
                        }

                        index++;
                    }
                }

                index += (size.x() - count.x()) * m_channel_count;
            }

            // Destruct weight variables
            for (uint32_t i = 0; i < count.x(); ++i)
                weights_x[i].~Float();

            for (uint32_t i = 0; i < count.y(); ++i)
                weights_y[i].~Float();
        } else {
            // ===========================================================
            // 1.2. Recorded loop mode
            // ===========================================================

            UInt32 ys = 0;

            std::tie(ys, index) = dr::while_loop(
                std::make_tuple(ys, index),
                [count](const UInt32 &ys, const UInt32 &) {
                    return ys < count.y();
                },
                [this, active, count, values, pos_0_u, pos_1_u,
                 rel_f, size](UInt32 &ys, UInt32 &index) {
                    Float weight_y = m_rfilter->eval(rel_f.y() + Float(ys));
                    Mask active_1 = active && (pos_0_u.y() + ys <= pos_1_u.y());

                    UInt32 xs = 0;

                    std::tie(xs, index) = dr::while_loop(
                        std::make_tuple(xs, index),
                        [count](const UInt32 &xs, const UInt32 &) {
                            return xs < count.x();
                        },
                        [this, values, rel_f, weight_y, pos_0_u, pos_1_u,
                         active_1](UInt32 &xs, UInt32 &index) {
                            Float weight_x =
                                      m_rfilter->eval(rel_f.x() + Float(xs)),
                                  weight = weight_x * weight_y;

                            Mask active_2 =
                                active_1 && (pos_0_u.x() + xs <= pos_1_u.x());
                            for (uint32_t k = 0; k < m_channel_count; ++k)
                                accum(values[k] * weight, index++, active_2);

                            xs++;
                        },
                        "ImageBlock::put() [2]");

                    ys++;
                    index += (size.x() - count.x()) * m_channel_count;
                },
                "ImageBlock::put() [1]");
        }

        return;
    }

    // ===================================================================
    // 2. Coalesced accumulation method (see ImageBlock constructor)
    // ===================================================================

    if constexpr (JIT) {
        if (!m_coalesce)
            return;

        // Number of pixels that may need to be visited on either side (-n..n)
        uint32_t n = dr::ceil2int<uint32_t>(radius - .5f);

        // Number of pixels to be visited along each dimension
        uint32_t count = 2 * n + 1;

        // Determine integer position of top left pixel within the filter footprint
        Point2i pos_i = dr::floor2int<Point2i>(pos) - int(n);

        // Account for pixel offset of the image block instance
        Point2i pos_i_local = pos_i + ((int) m_border_size - m_offset);

        // Switch over to unsigned integers, compute pixel index
        UInt32 x = UInt32(pos_i_local.x()),
               y = UInt32(pos_i_local.y()),
               index = dr::fmadd(y, size.x(), x) * m_channel_count;

        // Evaluate filters weights along the X and Y axes
        Point2f rel_f = Point2f(pos_i) + .5f - pos;

        if (!record_loop) {
            // ===========================================================
            // 2.1. Unroll the complete loop
            // ===========================================================

            // Allocate memory for reconstruction filter weights on the stack
            Float *weights_x = (Float *) alloca(sizeof(Float) * count),
                  *weights_y = (Float *) alloca(sizeof(Float) * count);

            for (uint32_t i = 0; i < count; ++i) {
                Float weight_x = m_rfilter->eval(rel_f.x());
                Float weight_y = m_rfilter->eval(rel_f.y());

                new (weights_x + i) Float(weight_x);
                new (weights_y + i) Float(weight_y);
                rel_f += 1;
            }

            // Normalize sample contribution if desired
            if (unlikely(m_normalize)) {
                Float wx = 0.f, wy = 0.f;

                for (uint32_t i = 0; i < count; ++i) {
                    wx += weights_x[i];
                    wy += weights_y[i];
                }

                Float factor = dr::detach(wx * wy);
                factor = dr::select(factor != 0.f, dr::rcp(factor), 0.f);

                for (uint32_t i = 0; i < count; ++i)
                    weights_x[i] *= factor;
            }

            // Accumulate!
            for (uint32_t ys = 0; ys < count; ++ys) {
                Mask active_1 = active && y < size.y();

                for (uint32_t xs = 0; xs < count; ++xs) {
                    Mask active_2 = active_1 && x < size.x();
                    Float weight = weights_y[ys] * weights_x[xs];

                    for (uint32_t k = 0; k < m_channel_count; ++k)
                        accum(values[k] * weight, index++, active_2);

                    x++;
                }

                x -= count;
                y += 1;
                index += (size.x() - count) * m_channel_count;
            }

            // Destruct weight variables
            for (uint32_t i = 0; i < count; ++i) {
                weights_x[i].~Float();
                weights_y[i].~Float();
            }
        } else {
            // ===========================================================
            // 2.2. Recorded loop mode
            // ===========================================================

            UInt32 ys = 0;

            std::tie(ys, index) = dr::while_loop(
                std::make_tuple(ys, index),
                [count](const UInt32 &ys, const UInt32 &) {
                    return ys < count;
                },
                [this, active, count, values, x, y, rel_f, 
                    size](UInt32 &ys, UInt32 &index) {
                    Float weight_y = m_rfilter->eval(rel_f.y() + Float(ys));
                    Mask active_1 = active && (y + ys < size.y());

                    UInt32 xs = 0;

                    std::tie(xs, index) = dr::while_loop(
                        std::make_tuple(xs, index),
                        [count](const UInt32 &xs, const UInt32 &) {
                            return xs < count;
                        },
                        [this, values, rel_f, weight_y, x, y, size,
                            active_1](UInt32 &xs, UInt32 &index) {
                            Float weight_x = 
                                    m_rfilter->eval(rel_f.x() + Float(xs)),
                                  weight = weight_x * weight_y;

                            Mask active_2 = active_1 && (x + xs < size.x());
                            for (uint32_t k = 0; k < m_channel_count; ++k)
                                accum(values[k] * weight, index++, active_2);

                            xs++;
                        },
                        "ImageBlock::put() [2]");

                    ys++;
                    index += (size.x() - count) * m_channel_count;
                },
                "ImageBlock::put() [1]");
        }
    }
}

MI_VARIANT void ImageBlock<Float, Spectrum>::read(const Point2f &pos_,
                                                   Float *values,
                                                   Mask active) const {
    constexpr bool JIT = dr::is_jit_v<Float>;

    // Account for image block offset
    Point2f pos = pos_ - ScalarVector2f(m_offset);

    // ===================================================================
    //  Fast special case for the box filter
    // ===================================================================

    if (!m_rfilter) {
        Point2u p = Point2u(dr::floor2int<Point2i>(pos));

        // Switch over to unsigned integers, compute pixel index
        UInt32 index = dr::fmadd(p.y(), m_size.x(), p.x()) * m_channel_count;

        // The sample could be out of bounds
        active = active && dr::all(p < m_size);

        // Gather!
        for (uint32_t k = 0; k < m_channel_count; ++k) {
            values[k] = dr::gather<Float>(m_tensor.array(), index, active);
            index++;
        }

        return;
    }

    // ===================================================================
    // Prelude for the general case
    // ===================================================================

    ScalarFloat radius = m_rfilter->radius();

    // Size of the underlying image buffer
    ScalarVector2u size = m_size + 2 * m_border_size;

    // Check if the operation can be performed using a recorded loop
    bool record_loop = false;

    if constexpr (JIT) {
        record_loop = jit_flag(JitFlag::LoopRecord);

        if constexpr (dr::is_diff_v<Float>) {
            record_loop = record_loop &&
                          !dr::grad_enabled(pos) &&
                          !dr::grad_enabled(m_tensor);

            for (uint32_t k = 0; k < m_channel_count; ++k)
                record_loop = record_loop && !dr::grad_enabled(values[k]);
        }
    }

    // Exclude areas that are outside of the block
    active &= dr::all(pos >= 0.f) && dr::all(pos < m_size);

    // Zero-initialize output array
    for (uint32_t i = 0; i < m_channel_count; ++i)
        values[i] = dr::zeros<Float>(dr::width(pos));

    Point2f pos_f   = pos + ((int) m_border_size - .5f),
            pos_0_f = pos_f - radius,
            pos_1_f = pos_f + radius;

    // Interval specifying the pixels covered by the filter
    Point2u pos_0_u = Point2u(dr::maximum(dr::ceil2int <Point2i>(pos_0_f), ScalarPoint2i(0))),
            pos_1_u = Point2u(dr::minimum(dr::floor2int<Point2i>(pos_1_f), ScalarPoint2i(size - 1))),
            count_u = pos_1_u - pos_0_u + 1u;

    // Base index of the top left corner
    UInt32 index =
        dr::fmadd(pos_0_u.y(), size.x(), pos_0_u.x()) * m_channel_count;

    // Compute the number of filter evaluations needed along each axis
    ScalarVector2u count;
    if constexpr (!JIT) {
        if (dr::any(pos_0_u > pos_1_u))
            return;
        count = count_u;
    } else {
        // Conservative bounds must be used in the vectorized case
        count = dr::ceil2int<uint32_t>(2.f * radius);
        active &= dr::all(pos_0_u <= pos_1_u);
    }

    Point2f rel_f = Point2f(pos_0_u) - pos_f;

    if (!record_loop) {
        // ===========================================================
        // 1.1. Scalar mode / unroll the complete loop
        // ===========================================================

        // Allocate memory for reconstruction filter weights on the stack
        Float *weights_x = (Float *) alloca(sizeof(Float) * count.x()),
              *weights_y = (Float *) alloca(sizeof(Float) * count.y());

        // Evaluate filters weights along the X and Y axes

        for (uint32_t i = 0; i < count.x(); ++i) {
            new (weights_x + i)
                Float(JIT ? m_rfilter->eval(rel_f.x())
                          : m_rfilter->eval_discretized(rel_f.x()));
            rel_f.x() += 1.f;
        }

        for (uint32_t i = 0; i < count.y(); ++i) {
            new (weights_y + i)
                Float(JIT ? m_rfilter->eval(rel_f.y())
                          : m_rfilter->eval_discretized(rel_f.y()));
            rel_f.y() += 1.f;
        }

        // Normalize sample contribution if desired
        if (unlikely(m_normalize)) {
            Float wx = 0.f, wy = 0.f;

            for (uint32_t i = 0; i < count.x(); ++i)
                wx += weights_x[i];

            for (uint32_t i = 0; i < count.y(); ++i)
                wy += weights_y[i];

            Float factor = dr::detach(wx * wy);

            if constexpr (JIT) {
                factor = dr::select(factor != 0.f, dr::rcp(factor), 0.f);
            } else {
                if (unlikely(factor == 0))
                    return;
                factor = dr::rcp(factor);
            }

            for (uint32_t i = 0; i < count.x(); ++i)
                weights_x[i] *= factor;
        }

        // Gather!
        for (uint32_t y = 0; y < count.y(); ++y) {
            Mask active_1 = active && y < count_u.y();

            for (uint32_t x = 0; x < count.x(); ++x) {
                Mask active_2 = active_1 && x < count_u.x();

                Float weight = weights_x[x] * weights_y[y];

                for (uint32_t k = 0; k < m_channel_count; ++k) {
                    values[k] = dr::fmadd(
                        dr::gather<Float>(m_tensor.array(), index, active_2),
                        weight, values[k]);

                    index++;
                }
            }

            index += (size.x() - count.x()) * m_channel_count;
        }

        // Destruct weight variables
        for (uint32_t i = 0; i < count.x(); ++i)
            weights_x[i].~Float();

        for (uint32_t i = 0; i < count.y(); ++i)
            weights_y[i].~Float();
    } else {
        // ===========================================================
        // 1.2. Recorded loop mode
        // ===========================================================

        UInt32 ys = 0;
        Float weight_sum = 0.f;
        using Values = dr::DynamicArray<Float>;
        Values dr_values = dr::load<Values>(values, m_channel_count);

        std::tie(ys, index, weight_sum, dr_values) = dr::while_loop(
            std::make_tuple(ys, index, weight_sum, dr_values),
            [count](const UInt32 &ys, const UInt32 &, const Float&, 
                const Values&) {
                return ys < count.y();
            },
            [this, active, count, pos_0_u, pos_1_u, size,
            rel_f](UInt32& ys, UInt32& index, Float& weight_sum, 
            Values& dr_values) {
                Float weight_y = m_rfilter->eval(rel_f.y() + Float(ys));
                Mask active_1 = active && (pos_0_u.y() + ys <= pos_1_u.y());

                UInt32 xs = 0;

                std::tie(xs, index, weight_sum, dr_values) = dr::while_loop(
                    std::make_tuple(xs, index, weight_sum, dr_values),
                    [count](const UInt32& xs, const UInt32&, const Float&, 
                        const Values&) {
                        return xs < count.x();
                    },
                    [this, rel_f, weight_y, pos_0_u, pos_1_u, 
                    active_1](UInt32& xs, UInt32& index, Float& weight_sum, 
                        Values& dr_values) {

                        Float weight_x = m_rfilter->eval(rel_f.x() + Float(xs)),
                        weight = weight_x * weight_y;

                        Mask active_2 = 
                            active_1 && (pos_0_u.x() + xs <= pos_1_u.x());
                        for (uint32_t k = 0; k < m_channel_count; ++k) {
                            dr_values.entry(k) = dr::fmadd(
                                dr::gather<Float>(m_tensor.array(), index, active_2),
                                weight, dr_values.entry(k));

                            index++;
                        }

                        weight_sum += dr::select(active_2, weight, 0.f);
                        xs++;
                    },
                    "ImageBlock::read() [2]");

                ys++;
                index += (size.x() - count.x()) * m_channel_count;
            },
            "ImageBlock::read() [1]");

        for (uint32_t k = 0; k < m_channel_count; ++k)
            values[k] = dr_values[k];

        if (m_normalize) {
            Float norm =
                dr::select(weight_sum != 0.f, dr::rcp(weight_sum), 0.f);

            for (uint32_t k = 0; k < m_channel_count; ++k)
                values[k] *= norm;
        }
    }
}

MI_VARIANT std::string ImageBlock<Float, Spectrum>::to_string() const {
    std::ostringstream oss;

    oss << "ImageBlock[" << std::endl
        << "  offset = " << m_offset << "," << std::endl
        << "  size = " << m_size << "," << std::endl
        << "  channel_count = " << m_channel_count << "," << std::endl
        << "  border_size = " << m_border_size << "," << std::endl
        << "  normalize = " << m_normalize << "," << std::endl
        << "  coalesce = " << m_coalesce << "," << std::endl
        << "  compensate = " << m_compensate << "," << std::endl
        << "  warn_negative = " << m_warn_negative << "," << std::endl
        << "  warn_invalid = " << m_warn_invalid << "," << std::endl
        << "  rfilter = " << (m_rfilter ? string::indent(m_rfilter) : "BoxFilter[]")
        << std::endl
        << "]";

    return oss.str();
}

MI_IMPLEMENT_CLASS_VARIANT(ImageBlock, Object)
MI_INSTANTIATE_CLASS(ImageBlock)
NAMESPACE_END(mitsuba)
