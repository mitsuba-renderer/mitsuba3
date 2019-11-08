#pragma once

#include <mitsuba/core/fwd.h>

#define MTS_CONFIGURATIONS                                                  \
    "scalar_mono\n"                                                         \
    "packet_mono\n"                                                         \


#define MTS_CONFIGURATIONS_INDENTED                                         \
    "            scalar_mono\n"                                             \
    "            packet_mono\n"                                             \


#define MTS_DEFAULT_MODE "scalar_rgb"                                       \


#define MTS_INSTANTIATE_OBJECT(Name)                                        \
    template class MTS_EXPORT Name<float, Color<float, 1>>;                 \
    template class MTS_EXPORT Name<Packet<float>, Color<Packet<float>, 1>>; \


#define MTS_INSTANTIATE_STRUCT(Name)                                        \
    template struct MTS_EXPORT Name<float, Color<float, 1>>;                \
    template struct MTS_EXPORT Name<Packet<float>, Color<Packet<float>, 1>>; \


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
        instantiate_##name<Packet<float>, Color<Packet<float>, 1>>(         \
            m.def_submodule("packet_mono"));                                \
    }                                                                       \
                                                                            \
    template <typename Float, typename Spectrum>                            \
    void instantiate_##name(py::module m)                                   \


#define MTS_ROUTE_MODE(mode, function, ...)                                 \
    [&]() {                                                                 \
        if (mode == "scalar_mono")                                          \
            return function<float, Color<float, 1>>(__VA_ARGS__);           \
        else if (mode == "packet_mono")                                     \
            return function<Packet<float>, Color<Packet<float>, 1>>(__VA_ARGS__); \
        else                                                                \
            Throw("Unsupported mode: %s", mode);                            \
    }()                                                                     \


#define PY_CAST_VARIANTS(Name)                                              \
    if (auto tmp = dynamic_cast<Name<float, Color<float, 1>> *>(o))         \
        return py::cast(tmp);                                               \
    if (auto tmp = dynamic_cast<Name<Packet<float>, Color<Packet<float>, 1>> *>(o)) \
        return py::cast(tmp);                                               \


NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(detail)
template <typename Float, typename Spectrum_> constexpr const char *get_variant() {
    if constexpr (std::is_same_v<Float, float> && std::is_same_v<Spectrum_, Color<float, 1>>)
        return "scalar_mono";
    else if constexpr (std::is_same_v<Float, Packet<float>> && std::is_same_v<Spectrum_, Color<Packet<float>, 1>>)
        return "packet_mono";
    else
        return "";
}
NAMESPACE_END(detail)
NAMESPACE_END(mitsuba)
