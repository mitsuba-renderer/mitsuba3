#include <drjit/struct.h>
#include <drjit/tensor.h>
#include <drjit/texture.h>
#include <drjit/vcall.h>
#include <drjit/packet.h>
#include <mitsuba/render/records.h>
#include <mutex>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MiTextureHolder: public Object {
public:
    virtual void eval() const = 0;
    DRJIT_VCALL_REGISTER(Float, mitsuba::MiTextureHolder)
    MI_DECLARE_CLASS()
};

template <typename Float, typename Spectrum>
class drTexWrapper: public MiTextureHolder<Float, Spectrum> {
public:
    drTexWrapper(int i){
        myi = i;
    }
    void eval() const override {
        std::cout<<myi<<std::endl;
    }

private:
    int myi;
};

NAMESPACE_END(mitsuba)

DRJIT_VCALL_TEMPLATE_BEGIN(mitsuba::MiTextureHolder)
    DRJIT_VCALL_METHOD(eval)
DRJIT_VCALL_TEMPLATE_END(mitsuba::MiTextureHolder)