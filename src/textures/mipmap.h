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

template <typename Float>
class drTexWrapper: public Object {
public:
    static constexpr bool IsCUDA = dr::is_cuda_v<Float>;
    static constexpr bool IsDiff = dr::is_diff_v<Float>;
    static constexpr bool IsDynamic = dr::is_dynamic_v<Float>;
    using Int32 = dr::int32_array_t<Float>;
    using UInt32 = dr::uint32_array_t<Float>;
    using Mask = dr::mask_t<Float>;
    using Storage = std::conditional_t<IsDynamic, Float, dr::DynamicArray<Float>>;
    using TensorXf = dr::Tensor<Storage>;
    using Color3f = mitsuba::Color<Float, 3>;
    using Point2f = mitsuba::Point<Float, 2>;
    using Vector2f = dr::Array<Float, 2>;
    using Vector2i = dr::Array<Int32, 2>;

    drTexWrapper(const TensorXf &tensor, bool use_accel = true, bool migrate = true, dr::FilterMode filter_mode = dr::FilterMode::Linear, dr::WrapMode wrap_mode = dr::WrapMode::Clamp):
        m_tex{tensor, use_accel, migrate, filter_mode, wrap_mode} { 
            const size_t *shape = m_tex.shape();
            res[0] = shape[1];
            res[1] = shape[0];
        }

    virtual ~drTexWrapper() { }

    virtual float test() const {
        return m_tex.tensor().array()[0];
    }

    virtual dr::Array<Float, 2> resolution() const {
        return res;
    }

    virtual Float eval_1(const dr::Array<Float, 2> &pos, Mask active = true) const {
        Float tmp = 0;
        m_tex.eval(pos, &tmp, active);
        return tmp;
    }

    virtual Float eval_1_box(const dr::Array<Float, 2> &pos, Mask active = true) const {
        Float tmp = 0;
        if (m_tex.filter_mode() == dr::FilterMode::Nearest){
            m_tex.eval(pos, &tmp, active);
        }
        else{
            // fetch and find the nearest
            dr::Array<Float *, 4> fetch_values;
            Float f00, f10, f01, f11;
            fetch_values[0] = &f00;
            fetch_values[1] = &f10;
            fetch_values[2] = &f01;
            fetch_values[3] = &f11;
            m_tex.eval_fetch(pos, fetch_values, active);
            dr::Array<Float, 2> uv = dr::fmadd(pos, res, -.5f);
            dr::Array<Float, 2> uv_i = dr::floor2int<Vector2i>(uv);
            Point2f w1 = uv - Point2f(uv_i);

            dr::masked(w1.x(), w1.x() >= 0.5f) = 1;
            dr::masked(w1.x(), w1.x() < 0.5f) = 0;
            dr::masked(w1.y(), w1.y() >= 0.5f) = 1;
            dr::masked(w1.y(), w1.y() < 0.5f) = 0;

            Point2f w0 = 1.f - w1;

            Float f0 = dr::fmadd(w0.x(), f00, w1.x() * f10);
            Float f1 = dr::fmadd(w0.x(), f01, w1.x() * f11);
            tmp = dr::fmadd(w0.y(), f0, w1.y() * f1);
        }
        return tmp;
    }

    virtual Color3f eval_3(const dr::Array<Float, 2> &pos, Mask active = true) const {
        Color3f tmp = 0;
        m_tex.eval(pos, tmp.data(), active);
        return tmp;
    }

    virtual Color3f eval_3_box(const dr::Array<Float, 2> &pos, Mask active = true) const {
        Color3f tmp = 0;
        if (m_tex.filter_mode() == dr::FilterMode::Nearest){
            m_tex.eval(pos, tmp.data(), active);
        }
        else{
            // fetch and find the nearest
            dr::Array<Float *, 4> fetch_values;
            Color3f f00, f10, f01, f11;
            fetch_values[0] = f00.data();
            fetch_values[1] = f10.data();
            fetch_values[2] = f01.data();
            fetch_values[3] = f11.data();
            m_tex.eval_fetch(pos, fetch_values, active);
            dr::Array<Float, 2> uv = dr::fmadd(pos, res, -.5f);
            dr::Array<Float, 2> uv_i = dr::floor2int<Vector2i>(uv);
            Point2f w1 = uv - Point2f(uv_i);

            dr::masked(w1.x(), w1.x() >= 0.5f) = 1;
            dr::masked(w1.x(), w1.x() < 0.5f) = 0;
            dr::masked(w1.y(), w1.y() >= 0.5f) = 1;
            dr::masked(w1.y(), w1.y() < 0.5f) = 0;

            Point2f w0 = 1.f - w1;

            Color3f f0 = dr::fmadd(w0.x(), f00, w1.x() * f10);
            Color3f f1 = dr::fmadd(w0.x(), f01, w1.x() * f11);
            tmp = dr::fmadd(w0.y(), f0, w1.y() * f1);
        }
        return tmp;
    }

    virtual const TensorXf &tensor() const  {
        return m_tex.tensor();
    }

    virtual dr::Array<Float, 1 << 2> eval_fetch_1(const dr::Array<Float, 2> &pos, Mask active = true) const {
        dr::Array<Float, 4> out;
        dr::Array<Float*, 4> fetch_values;
        fetch_values[0] = &out[0];
        fetch_values[1] = &out[1];
        fetch_values[2] = &out[2];
        fetch_values[3] = &out[3];
        m_tex.eval_fetch(pos, fetch_values, active);
        return out;
    }

    virtual dr::Array<Color3f, 1 << 2> eval_fetch_3(const dr::Array<Float, 2> &pos, Mask active = true) const {
        dr::Array<Color3f, 4> out;
        dr::Array<Float*, 4> fetch_values;
        fetch_values[0] = out[0].data();
        fetch_values[1] = out[1].data();
        fetch_values[2] = out[2].data();
        fetch_values[3] = out[3].data();
        m_tex.eval_fetch(pos, fetch_values, active);
        return out;
    }

    DRJIT_VCALL_REGISTER(Float, mitsuba::drTexWrapper)

private:
    dr::Texture<Float, 2> m_tex;
    dr::Array<uint, 2> res;
};

NAMESPACE_END(mitsuba)

DRJIT_VCALL_TEMPLATE_BEGIN(mitsuba::drTexWrapper)
    DRJIT_VCALL_METHOD(test)
    DRJIT_VCALL_METHOD(resolution)
    DRJIT_VCALL_METHOD(tensor)
    DRJIT_VCALL_METHOD(eval_1)
    DRJIT_VCALL_METHOD(eval_1_box)
    DRJIT_VCALL_METHOD(eval_3)
    DRJIT_VCALL_METHOD(eval_3_box)
    DRJIT_VCALL_METHOD(eval_fetch_1)
    DRJIT_VCALL_METHOD(eval_fetch_3)
DRJIT_VCALL_TEMPLATE_END(mitsuba::drTexWrapper)