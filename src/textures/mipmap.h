#include <drjit/struct.h>
#include <drjit/tensor.h>
#include <drjit/texture.h>
#include <drjit/vcall.h>
#include <drjit/packet.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/core/fwd.h>
#include <mutex>


NAMESPACE_BEGIN(mitsuba)
#define Dimenstion 2
// template <typename Float, typename Spectrum>
// class MiTextureHolder: public Object {
// public:
//     MI_IMPORT_TYPES()
//     MiTextureHolder() = default;
//     virtual float test() const = 0;
//     virtual Float eval(const dr::Array<Float, 2> &pos, Mask active = true) const = 0;
//     virtual const TensorXf &tensor() const = 0;
//     virtual void eval_fetch(const dr::Array<Float, 2> &pos,
//                         dr::Array<Float *, 1 << 2> &out,
//                         Mask active = true) const = 0;
//     DRJIT_VCALL_REGISTER(Float, mitsuba::MiTextureHolder)
// };

template <typename Float>
class drTexWrapper: public Object {
public:
    static constexpr bool IsCUDA = dr::is_cuda_v<Float>;
    static constexpr bool IsDiff = dr::is_diff_v<Float>;
    static constexpr bool IsDynamic = dr::is_dynamic_v<Float>;
    using Int32 = dr::int32_array_t<Float>;
    using UInt32 = dr::uint32_array_t<Float>;
    using Mask = dr::mask_t<Float>;
    using ArrayX = dr::DynamicArray<Float>;
    using Storage = std::conditional_t<IsDynamic, Float, dr::DynamicArray<Float>>;
    using TensorXf = dr::Tensor<Storage>;

    drTexWrapper(const TensorXf &tensor, bool use_accel = true, bool migrate = true, dr::FilterMode filter_mode = dr::FilterMode::Linear, dr::WrapMode wrap_mode = dr::WrapMode::Clamp):
        m_tex{tensor, use_accel, migrate, filter_mode, wrap_mode} { }

    virtual ~drTexWrapper() { }

    virtual float test() const {
        return m_tex.tensor().array()[0];
    }

    virtual Float eval(const dr::Array<Float, 2> &pos, Mask active = true) const {
        Float tmp = 0;
        m_tex.eval(pos, &tmp, active);
        return tmp;
    }

    virtual const TensorXf &tensor() const  {
        return m_tex.tensor();
    }

    virtual void eval_fetch(const dr::Array<Float, 2> &pos,
                            dr::Array<Float *, 1 << 2> &out,
                            Mask active = true) const {
        m_tex.eval_fetch(pos, out, active);
    }

    DRJIT_VCALL_REGISTER(Float, mitsuba::drTexWrapper)

private:
    dr::Texture<Float, 2> m_tex;
};

NAMESPACE_END(mitsuba)

DRJIT_VCALL_TEMPLATE_BEGIN(mitsuba::drTexWrapper)
    DRJIT_VCALL_METHOD(test)
    DRJIT_VCALL_METHOD(tensor)
    DRJIT_VCALL_METHOD(eval)
    DRJIT_VCALL_METHOD(eval_fetch)
DRJIT_VCALL_TEMPLATE_END(mitsuba::drTexWrapper)