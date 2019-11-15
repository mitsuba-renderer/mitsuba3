#pragma once

#include <mitsuba/core/fwd.h>

#define MTS_CONFIGURATIONS                                                  \
    "scalar_rgb\n"                                                          \
    "scalar_mono\n"                                                         \
    "scalar_spectral\n"                                                     \
    "scalar_spectral_polarized\n"                                           \
    "packet_mono\n"                                                         \


#define MTS_CONFIGURATIONS_INDENTED                                         \
    "            scalar_rgb\n"                                              \
    "            scalar_mono\n"                                             \
    "            scalar_spectral\n"                                         \
    "            scalar_spectral_polarized\n"                               \
    "            packet_mono\n"                                             \


#define MTS_DEFAULT_MODE "scalar_spectral"                                  \


#define MTS_INSTANTIATE_OBJECT(Name)                                        \
    template class MTS_EXPORT Name<float, Color<float, 3>>;                 \
    template class MTS_EXPORT Name<float, Color<float, 1>>;                 \
    template class MTS_EXPORT Name<float, Spectrum<float, 4>>;              \
    template class MTS_EXPORT Name<float, MuellerMatrix<Spectrum<float, 4>>>; \
    template class MTS_EXPORT Name<Packet<float>, Color<Packet<float>, 1>>; \


#define MTS_INSTANTIATE_STRUCT(Name)                                        \
    template struct MTS_EXPORT Name<float, Color<float, 3>>;                \
    template struct MTS_EXPORT Name<float, Color<float, 1>>;                \
    template struct MTS_EXPORT Name<float, Spectrum<float, 4>>;             \
    template struct MTS_EXPORT Name<float, MuellerMatrix<Spectrum<float, 4>>>; \
    template struct MTS_EXPORT Name<Packet<float>, Color<Packet<float>, 1>>; \


#define MTS_EXTERN_STRUCT(Name)                                             \
    extern template struct MTS_EXPORT Name<float, Color<float, 3>>;         \
    extern template struct MTS_EXPORT Name<float, Color<float, 1>>;         \
    extern template struct MTS_EXPORT Name<float, Spectrum<float, 4>>;      \
    extern template struct MTS_EXPORT Name<float, MuellerMatrix<Spectrum<float, 4>>>; \
    extern template struct MTS_EXPORT Name<Packet<float>, Color<Packet<float>, 1>>; \


#define MTS_EXTERN_CLASS(Name)                                              \
    extern template class MTS_EXPORT Name<float, Color<float, 3>>;          \
    extern template class MTS_EXPORT Name<float, Color<float, 1>>;          \
    extern template class MTS_EXPORT Name<float, Spectrum<float, 4>>;       \
    extern template class MTS_EXPORT Name<float, MuellerMatrix<Spectrum<float, 4>>>; \
    extern template class MTS_EXPORT Name<Packet<float>, Color<Packet<float>, 1>>; \


#define MTS_EXTERN_STRUCT_FLOAT(Name)                                       \
    extern template struct MTS_EXPORT Name<float>;                          \
    extern template struct MTS_EXPORT Name<Packet<float>>;                  \


#define MTS_EXTERN_CLASS_FLOAT(Name)                                        \
    extern template class MTS_EXPORT Name<float>;                           \
    extern template class MTS_EXPORT Name<Packet<float>>;                   \


#define MTS_IMPLEMENT_PLUGIN(Name, Descr)                                   \
    extern "C" {                                                            \
        MTS_EXPORT const char *plugin_name() { return #Name; }              \
        MTS_EXPORT const char *plugin_descr() { return Descr; }             \
    }                                                                       \
    MTS_INSTANTIATE_OBJECT(Name)                                            \


/// Declare a pybind11 extern binding function for a set of bindings under a given name
#define MTS_PY_DECLARE(name)                                                \
    extern void python_export_scalar_rgb_##name(py::module &);              \
    extern void python_export_scalar_mono_##name(py::module &);             \
    extern void python_export_scalar_spectral_##name(py::module &);         \
    extern void python_export_scalar_spectral_polarized_##name(py::module &); \
    extern void python_export_packet_mono_##name(py::module &);             \


/// Define a python submodule for each rendering mode
#define MTS_PY_DEF_SUBMODULE(lib)                                           \
    auto __submodule__scalar_rgb =  m.def_submodule("scalar_rgb").def_submodule(#lib); \
    auto __submodule__scalar_mono =  m.def_submodule("scalar_mono").def_submodule(#lib); \
    auto __submodule__scalar_spectral =  m.def_submodule("scalar_spectral").def_submodule(#lib); \
    auto __submodule__scalar_spectral_polarized =  m.def_submodule("scalar_spectral_polarized").def_submodule(#lib); \
    auto __submodule__packet_mono =  m.def_submodule("packet_mono").def_submodule(#lib); \


/// Execute the pybind11 binding function for a set of bindings under a given name
#define MTS_PY_IMPORT(name)                                                 \
    python_export_scalar_rgb_##name(__submodule__scalar_rgb);               \
    python_export_scalar_mono_##name(__submodule__scalar_mono);             \
    python_export_scalar_spectral_##name(__submodule__scalar_spectral);     \
    python_export_scalar_spectral_polarized_##name(__submodule__scalar_spectral_polarized); \
    python_export_packet_mono_##name(__submodule__packet_mono);             \


/// Define the pybind11 binding function for a set of bindings under a given name
#define MTS_PY_EXPORT(name)                                                 \
    template <typename Float, typename Spectrum>                            \
    void instantiate_##name(py::module m);                                  \
                                                                            \
    void python_export_scalar_rgb_##name(py::module &m) {                   \
        instantiate_##name<float, Color<float, 3>>(m);                      \
    }                                                                       \
    void python_export_scalar_mono_##name(py::module &m) {                  \
        instantiate_##name<float, Color<float, 1>>(m);                      \
    }                                                                       \
    void python_export_scalar_spectral_##name(py::module &m) {              \
        instantiate_##name<float, Spectrum<float, 4>>(m);                   \
    }                                                                       \
    void python_export_scalar_spectral_polarized_##name(py::module &m) {    \
        instantiate_##name<float, MuellerMatrix<Spectrum<float, 4>>>(m);    \
    }                                                                       \
    void python_export_packet_mono_##name(py::module &m) {                  \
        instantiate_##name<Packet<float>, Color<Packet<float>, 1>>(m);      \
    }                                                                       \
                                                                            \
    template <typename Float, typename Spectrum>                            \
    void instantiate_##name(py::module m)                                   \


/// Define the pybind11 binding function for a structures under a given name
#define MTS_PY_EXPORT_STRUCT(name)                                          \
    template <typename Float, typename Spectrum>                            \
    void instantiate_##name(py::module m);                                  \
                                                                            \
    void python_export_scalar_rgb_##name(py::module &m) {                   \
        instantiate_##name<float, Color<float, 3>>(m);                      \
    }                                                                       \
    void python_export_scalar_mono_##name(py::module &m) {                  \
        instantiate_##name<float, Color<float, 1>>(m);                      \
    }                                                                       \
    void python_export_scalar_spectral_##name(py::module &m) {              \
        instantiate_##name<float, Spectrum<float, 4>>(m);                   \
    }                                                                       \
    void python_export_scalar_spectral_polarized_##name(py::module &m) {    \
        instantiate_##name<float, MuellerMatrix<Spectrum<float, 4>>>(m);    \
    }                                                                       \
    void python_export_packet_mono_##name(py::module &m) {                  \
        instantiate_##name<DynamicArray<Packet<float>>, Color<DynamicArray<Packet<float>>, 1>>(m); \
    }                                                                       \
                                                                            \
    template <typename Float, typename Spectrum>                            \
    void instantiate_##name(py::module m)                                   \


#define MTS_ROUTE_MODE(mode, function, ...)                                 \
    [&]() {                                                                 \
        if (mode == "scalar_rgb")                                           \
            return function<float, Color<float, 3>>(__VA_ARGS__);           \
        else if (mode == "scalar_mono")                                     \
            return function<float, Color<float, 1>>(__VA_ARGS__);           \
        else if (mode == "scalar_spectral")                                 \
            return function<float, Spectrum<float, 4>>(__VA_ARGS__);        \
        else if (mode == "scalar_spectral_polarized")                       \
            return function<float, MuellerMatrix<Spectrum<float, 4>>>(__VA_ARGS__); \
        else if (mode == "packet_mono")                                     \
            return function<Packet<float>, Color<Packet<float>, 1>>(__VA_ARGS__); \
        else                                                                \
            Throw("Unsupported mode: %s", mode);                            \
    }()                                                                     \


#define PY_CAST_VARIANTS(Name)                                              \
    if (auto tmp = dynamic_cast<Name<float, Color<float, 3>> *>(o); tmp)    \
        return py::cast(tmp);                                               \
    if (auto tmp = dynamic_cast<Name<float, Color<float, 1>> *>(o); tmp)    \
        return py::cast(tmp);                                               \
    if (auto tmp = dynamic_cast<Name<float, Spectrum<float, 4>> *>(o); tmp) \
        return py::cast(tmp);                                               \
    if (auto tmp = dynamic_cast<Name<float, MuellerMatrix<Spectrum<float, 4>>> *>(o); tmp) \
        return py::cast(tmp);                                               \
    if (auto tmp = dynamic_cast<Name<Packet<float>, Color<Packet<float>, 1>> *>(o); tmp) \
        return py::cast(tmp);                                               \


NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(detail)
template <typename Float, typename Spectrum_> constexpr const char *get_variant() {
    if constexpr (std::is_same_v<Float, float> && std::is_same_v<Spectrum_, Color<float, 3>>)
        return "scalar_rgb";
    else if constexpr (std::is_same_v<Float, float> && std::is_same_v<Spectrum_, Color<float, 1>>)
        return "scalar_mono";
    else if constexpr (std::is_same_v<Float, float> && std::is_same_v<Spectrum_, Spectrum<float, 4>>)
        return "scalar_spectral";
    else if constexpr (std::is_same_v<Float, float> && std::is_same_v<Spectrum_, MuellerMatrix<Spectrum<float, 4>>>)
        return "scalar_spectral_polarized";
    else if constexpr (std::is_same_v<Float, Packet<float>> && std::is_same_v<Spectrum_, Color<Packet<float>, 1>>)
        return "packet_mono";
    else
        return "";
}
NAMESPACE_END(detail)
NAMESPACE_END(mitsuba)
