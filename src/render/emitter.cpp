#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/endpoint.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Emitter<Float, Spectrum>::Emitter(const Properties &props)
    : Base(props) {
        m_sampling_weight = props.get<ScalarFloat>("sampling_weight", 1.0f);

        if constexpr (dr::is_jit_v<Float>)
            jit_registry_put(dr::backend_v<Float>, "mitsuba::Emitter", this);
    }

MI_VARIANT Emitter<Float, Spectrum>::~Emitter() { 
    if constexpr (dr::is_jit_v<Float>)
        jit_registry_remove(this);
}

MI_VARIANT
void Emitter<Float, Spectrum>::traverse(TraversalCallback *callback) {
    callback->put_parameter("sampling_weight", m_sampling_weight, +ParamFlags::NonDifferentiable);
}

MI_VARIANT
void Emitter<Float, Spectrum>::parameters_changed(const std::vector<std::string> &keys) {
    set_dirty(true);
    Base::parameters_changed(keys);
}

MI_IMPLEMENT_CLASS_VARIANT(Emitter, Endpoint, "emitter")
MI_INSTANTIATE_CLASS(Emitter)
NAMESPACE_END(mitsuba)
