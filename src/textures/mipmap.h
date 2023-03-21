#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/distr_2d.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/core/bitmap.h>
#include <drjit/tensor.h>
#include <drjit/texture.h>
#include <drjit/struct.h>
#include <mutex>

NAMESPACE_BEGIN(mitsuba)

// Texture interpolation methods: dr::FilterMode
// Texture wrapping methods: dr::WrapMode

#define MI_MIPMAP_LUT_SIZE 128

/// Specifies the desired antialiasing filter
enum MIPFilterType {
    /// No filtering, nearest neighbor lookups
    Nearest = 0,
    /// No filtering, only bilinear interpolation
    Bilinear = 1,
    /// Basic trilinear filtering
    Trilinear = 2,
    /// Elliptically weighted average
    EWA = 3
};

/**
 * A simple version of texture which contains the eval function and constructor. Used for mipmap access.
 * \param HasCudaTexture CUDA texture (CUmipmappedArray) is not used
*/
template <typename Value, size_t Dimension> class MIPMap {
public:
    static constexpr bool IsCUDA = dr::is_cuda_v<Value>;
    static constexpr bool IsDiff = dr::is_diff_v<Value>;
    static constexpr bool IsDynamic = dr::is_dynamic_v<Value>;

    // CUDA texture
    static constexpr bool HasCudaTexture =
        std::is_same_v<dr::scalar_t<Value>, float> && IsCUDA;

    using Int32 = dr::int32_array_t<Value>;
    using UInt32 = dr::uint32_array_t<Value>;
    using Mask = dr::mask_t<Value>;
    using PosF = dr::Array<Value, Dimension>;
    using PosI = dr::int32_array_t<PosF>;
    using ArrayX = dr::DynamicArray<Value>;
    using Storage = std::conditional_t<IsDynamic, Value, dr::DynamicArray<Value>>;
    using StorageI = dr::replace_scalar_t<Storage, uint32_t>;
    using TensorXf = dr::Tensor<Storage>;

    using DFloat = std::conditional_t<dr::is_dynamic_array_v<Value>, Value, dr::DynamicArray<Value>>;
    using DUInt = dr::replace_scalar_t<DFloat, uint64_t>;
    using DInt = dr::replace_scalar_t<DFloat, int64_t>;
    using DUIntX = dr::Array<DUInt, Dimension>;
    using DIntX = dr::Array<DInt, Dimension>;

    /// Default constructor: create an invalid texture object
    MIPMap() = default;

    MIPMap(const TensorXf &tensor, std::vector<dr::Array<uint32_t, Dimension>> &res, bool use_accel = false, bool migrate = true,
            dr::FilterMode filter_mode = dr::FilterMode::Linear,
            dr::WrapMode wrap_mode = dr::WrapMode::Clamp):
                m_levels(res.size()),
                m_level_res(dr::zeros<DIntX>(res.size()))
    {
        if (tensor.ndim() != Dimension + 1)
            dr::drjit_raise("Texture::Texture(): tensor dimension must equal "
                        "texture dimension plus one.");
        init(tensor.shape().data(), tensor.shape(Dimension), res, use_accel,
             filter_mode, wrap_mode);
        set_tensor(tensor, res, migrate);
    }

    const TensorXf &tensor() const {
        // if constexpr (HasCudaTexture) {

        // }

        return m_value;
    }

    TensorXf &tensor() {
        return const_cast<TensorXf &>(
            const_cast<const MIPMap<Value, Dimension> *>(this)->tensor());
    }

    void set_tensor(const TensorXf &tensor, std::vector<dr::Array<uint32_t, Dimension>> &res, bool migrate=false) {
        if (tensor.ndim() != Dimension + 1)
            dr::drjit_raise("Texture::set_tensor(): tensor dimension must equal "
                        "texture dimension plus one (channels).");

        bool is_inplace_update = (&tensor == &m_value);

        // if constexpr (HasCudaTexture) {

        // } else {
            init(tensor.shape().data(), tensor.shape(Dimension), res,
                 m_use_accel, m_filter_mode, m_wrap_mode, false);
        // }

        if constexpr (!IsDynamic)
            if (is_inplace_update)
                return;

        set_value(tensor.array(), migrate);
    }

    void set_value(const Storage &value, bool migrate=false) {
        if (value.size() != m_size)
            dr::drjit_raise("Texture::set_value(): unexpected array size!");

        drjit::eval(value);

        // if constexpr (HasCudaTexture) {

        // }

        m_value.array() = value;
    }

    void eval_nonaccel(const dr::Array<Value, Dimension> &pos, Value *out, UInt32 lod,
                       Mask active = true) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        const uint32_t channels = (uint32_t) m_value.shape(Dimension);
        if (DRJIT_UNLIKELY(m_filter_mode == dr::FilterMode::Nearest)) {
            const PosF pos_f = pos * PosF(m_shape_opaque);
            const PosI pos_i = dr::floor2int<PosI>(pos_f);
            const PosI pos_i_w = wrap(pos_i, lod);

            UInt32 idx = index(pos_i_w, lod);

            for (uint32_t ch = 0; ch < channels; ++ch)
                out[ch] = dr::gather<Value>(m_value.array(), idx + ch, active);
        } else {
            using InterpOffset = dr::Array<Int32, ipow(2, Dimension)>;
            using InterpPosI = dr::Array<InterpOffset, Dimension>;
            using InterpIdx = dr::uint32_array_t<InterpOffset>;

            const PosF pos_f = dr::fmadd(pos, PosF(m_shape_opaque), -.5f);
            const PosI pos_i = dr::floor2int<PosI>(pos_f);

            int32_t offset[2] = { 0, 1 };

            InterpPosI pos_i_w = interp_positions<PosI, 2>(offset, pos_i);
            pos_i_w = wrap(pos_i_w, lod);
            InterpIdx idx = index(pos_i_w, lod);

            for (uint32_t ch = 0; ch < channels; ++ch)
                out[ch] = dr::zeros<Value>();

            #define DR_TEX_ACCUM(index, weight)                                                 \
                {                                                                               \
                    UInt32 index_ = index;                                                      \
                    Value weight_ = weight;                                                     \
                    for (uint32_t ch = 0; ch < channels; ++ch)                                  \
                        out[ch] =                                                               \
                            dr::fmadd(dr::gather<Value>(m_value.array(), index_ + ch, active),  \
                                weight_, out[ch]);                                              \
                }

            const PosF w1 = pos_f - pos_i,
                       w0 = 1.f - w1;

            if constexpr (Dimension == 1) {
                DR_TEX_ACCUM(idx.x(), w0.x());
                DR_TEX_ACCUM(idx.y(), w1.x());
            } else if constexpr (Dimension == 2) {
                DR_TEX_ACCUM(idx.x(), w0.x() * w0.y());
                DR_TEX_ACCUM(idx.y(), w1.x() * w0.y());
                DR_TEX_ACCUM(idx.z(), w0.x() * w1.y());
                DR_TEX_ACCUM(idx.w(), w1.x() * w1.y());
            } else if constexpr (Dimension == 3) {
                DR_TEX_ACCUM(idx[0], w0.x() * w0.y() * w0.z());
                DR_TEX_ACCUM(idx[1], w1.x() * w0.y() * w0.z());
                DR_TEX_ACCUM(idx[2], w0.x() * w1.y() * w0.z());
                DR_TEX_ACCUM(idx[3], w1.x() * w1.y() * w0.z());
                DR_TEX_ACCUM(idx[4], w0.x() * w0.y() * w1.z());
                DR_TEX_ACCUM(idx[5], w1.x() * w0.y() * w1.z());
                DR_TEX_ACCUM(idx[6], w0.x() * w1.y() * w1.z());
                DR_TEX_ACCUM(idx[7], w1.x() * w1.y() * w1.z());
            }

            #undef DR_TEX_ACCUM
        }
    }

    void eval(const dr::Array<Value, Dimension> &pos, Value *out, UInt32 lod, 
              Mask active = true) const {
        // if constexpr (HasCudaTexture) {

        // }

        eval_nonaccel(pos, out, lod, active);
    }

    template <typename T> T wrap(const T &pos, const UInt32 lod) const {
        using Scalar = dr::scalar_t<T>;
        static_assert(
            dr::array_size_v<T> == Dimension &&
            std::is_integral_v<Scalar> &&
            std::is_signed_v<Scalar>
        );

        dr::Array<Int32, Dimension> shape = dr::gather<dr::Array<Int32, Dimension>>(m_shape_opaque, lod);
        std::cout<<"Shape: "<<shape<<std::endl;
        if (m_wrap_mode == dr::WrapMode::Clamp) {
            return clamp(pos, 0, shape - 1);
        } else {
            T value_shift_neg = select(pos < 0, pos + 1, pos);

            T div;
            for (size_t i = 0; i < Dimension; ++i)
                div[i] = m_inv_resolution[i](value_shift_neg[i]);

            T mod = pos - div * shape;
            mod[mod < 0] += T(shape);

            if (m_wrap_mode == dr::WrapMode::Mirror)
                // Starting at 0, flip the texture every other repetition
                // (flip when: even number of repetitions in negative direction,
                // or odd number of repetitions in positive direction)
                mod = select(eq(div & 1, 0) ^ (pos < 0), mod, shape - 1 - mod);

            return mod;
        }
    }


protected:
    void init(const size_t *shape, size_t channels, std::vector<dr::Array<uint32_t, Dimension>> &res, bool use_accel,
              dr::FilterMode filter_mode, dr::WrapMode wrap_mode,
              bool init_tensor = true) {
        if (channels == 0)
            dr::drjit_raise("Texture::Texture(): must have at least 1 channel!");

        m_size = channels;
        size_t tensor_shape[Dimension + 1]{};

        //===================================================================== res now is actually just support 2D
        std::vector<uint32_t> off(res.size()+1);
        for(auto i = 0; i<res.size(); i++){
            off[i+1] = off[i] + res[i].x();
        }
        m_offset = dr::load<StorageI>(off.data(), res.size()+1);
        //=====================================================================

        for (size_t i = 0; i < Dimension; ++i) {
            tensor_shape[i] = shape[i];
            m_size *= shape[i];

            for(size_t j = 0; j < res.size(); j++){
                std::cout<<m_shape_opaque[Dimension - 1 - i]<<std::endl;
                dr::scatter(m_shape_opaque[Dimension - 1 - i], UInt32(res[j][i]), UInt32(j));
                m_inv_resolution[Dimension - 1 - i] = dr::divisor<int32_t>((int32_t) shape[i]);
            }
        }
        tensor_shape[Dimension] = channels;

        if (init_tensor)
            m_value = TensorXf(dr::zeros<Storage>(m_size), Dimension + 1, tensor_shape);

        m_use_accel = use_accel;
        m_filter_mode = filter_mode;
        m_wrap_mode = wrap_mode;

        // if constexpr (HasCudaTexture) {

        // }
    }

private:
    template <typename T>
    constexpr static T ipow(T num, unsigned int pow) {
        return pow == 0 ? 1 : num * ipow(num, pow - 1);
    }
    
    /// Builds the set of integer positions necessary for the interpolation
    template <typename T, size_t Length>
    static dr::Array<dr::Array<Int32, ipow(Length, Dimension)>, Dimension>
    interp_positions(const int *offset, const T &pos) {
        using Scalar = dr::scalar_t<T>;
        using InterpOffset = dr::Array<Int32, ipow(Length, Dimension)>;
        using InterpPosI = dr::Array<InterpOffset, Dimension>;
        static_assert(
            dr::array_size_v<T> == Dimension &&
            std::is_integral_v<Scalar> &&
            std::is_signed_v<Scalar>
        );

        InterpPosI pos_i;
        if constexpr (Dimension == 1) {
            for (uint32_t ix = 0; ix < Length; ix++) {
                pos_i[0][ix] = offset[ix] + pos.x();
            }
        } else if constexpr (Dimension == 2) {
            for (uint32_t iy = 0; iy < Length; iy++) {
                for (uint32_t ix = 0; ix < Length; ix++) {
                    pos_i[0][iy * Length + ix] = offset[ix] + pos.x();
                    pos_i[1][ix * Length + iy] = offset[ix] + pos.y();
                }
            }
        } else if constexpr (Dimension == 3) {
            constexpr size_t LengthSqr = Length * Length;
            for (uint32_t iz = 0; iz < Length; iz++) {
                for (uint32_t iy = 0; iy < Length; iy++) {
                    for (uint32_t ix = 0; ix < Length; ix++) {
                        pos_i[0][iz * LengthSqr + iy * Length + ix] =
                            offset[ix] + pos.x();
                        pos_i[1][iz * LengthSqr + ix * Length + iy] =
                            offset[ix] + pos.y();
                        pos_i[2][ix * LengthSqr + iy * Length + iz] =
                            offset[ix] + pos.z();
                    }
                }
            }
        }

        return pos_i;
    }

    template <typename T>
    dr::uint32_array_t<dr::value_t<T>> index(const T &pos, UInt32 lod) const {
        using Scalar = dr::scalar_t<T>;
        using Index = dr::uint32_array_t<dr::value_t<T>>;
        static_assert(
            dr::array_size_v<T> == Dimension &&
            std::is_integral_v<Scalar> &&
            std::is_signed_v<Scalar>
        );

        UInt32 offset = (dr::gather<UInt32>(m_offset, lod)) * dr::gather<UInt32>(m_shape_opaque.x(), lod);
        std::cout<<lod<<" "<<offset<<std::endl;
        Index index;
        if constexpr (Dimension == 1) {
            index = Index(pos.x());
        } else if constexpr (Dimension == 2) {
            index = Index(
                dr::fmadd(Index(pos.y()), m_shape_opaque.x(), Index(pos.x())));
        } else if constexpr (Dimension == 3) {
            index = Index(dr::fmadd(
                dr::fmadd(Index(pos.z()), m_shape_opaque.y(), Index(pos.y())),
                m_shape_opaque.x(), Index(pos.x())));
        }

        uint32_t channels = (uint32_t) m_value.shape(Dimension);

        std::cout<<(offset + index) * channels<<std::endl;

        return (offset + index) * channels;
    }


private:
    size_t m_size = 0;
    size_t m_levels;
    DIntX m_level_res;
    StorageI m_offset;

    // This is the 2D mipmap
    mutable TensorXf m_value;

    dr::Array<UInt32, Dimension> m_shape_opaque;
    dr::divisor<int32_t> m_inv_resolution[Dimension] { };

    dr::FilterMode m_filter_mode;
    dr::WrapMode m_wrap_mode;
    bool m_use_accel = false;
    mutable bool m_migrated = false;
};


NAMESPACE_END(mitsuba)