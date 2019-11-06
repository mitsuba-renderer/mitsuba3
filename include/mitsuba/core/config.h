#pragma once

#include <mitsuba/core/fwd.h>

#define MTS_CONFIGURATIONS                                                  \
    "scalar_mono\n"                                                         \
    "scalar_rgb\n"                                                          \
    "scalar_spectral\n"                                                     \


#define MTS_INSTANTIATE_OBJECT(Name)                                        \
    template class MTS_EXPORT_RENDER Name<float, Color<float, 1>>;          \
    template class MTS_EXPORT_RENDER Name<float, Color<float, 3>>;          \
    template class MTS_EXPORT_RENDER Name<float, Spectrum<float, 4>>;       \


#define MTS_INSTANTIATE_STRUCT(Name)                                        \
    template struct MTS_EXPORT_RENDER Name<float, Color<float, 1>>;         \
    template struct MTS_EXPORT_RENDER Name<float, Color<float, 3>>;         \
    template struct MTS_EXPORT_RENDER Name<float, Spectrum<float, 4>>;      \


#define MTS_IMPLEMENT_PLUGIN(Name, Parent, Descr)                           \
    MTS_EXPORT const char *plugin_descr = Descr;                            \
    MTS_EXPORT const char *plugin_name = #Name;                             \
    MTS_INSTANTIATE_OBJECT(Name)                                            \


#define MTS_PY_EXPORT_VARIANTS(name)                                        \
    template <typename Float, typename Spectrum>                            \
    void instantiate_##name(py::module m);                                  \
                                                                            \
    MTS_PY_EXPORT(name) {                                                   \
        instantiate_##name<float, Color<float, 1>>(                         \
            m.def_submodule("scalar_mono"));                                \
        instantiate_##name<float, Color<float, 3>>(                         \
            m.def_submodule("scalar_rgb"));                                 \
        instantiate_##name<float, Spectrum<float, 4>>(                      \
            m.def_submodule("scalar_spectral"));                            \
    }                                                                       \
                                                                            \
    template <typename Float, typename Spectrum>                            \
    void instantiate_##name(py::module m)                                   \


NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(detail)
template <typename Float, typename Spectrum_> constexpr const char *get_variant() {
    if constexpr (std::is_same_v<Float, float> && std::is_same_v<Spectrum_, Color<float, 1>>)
        return "scalar_mono";
    else if constexpr (std::is_same_v<Float, float> && std::is_same_v<Spectrum_, Color<float, 3>>)
        return "scalar_rgb";
    else if constexpr (std::is_same_v<Float, float> && std::is_same_v<Spectrum_, Spectrum<float, 4>>)
        return "scalar_spectral";
    else
        return "";
}
NAMESPACE_END(detail)NAMESPACE_END(mitsuba)