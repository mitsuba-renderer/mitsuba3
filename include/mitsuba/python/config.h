#pragma once

#include <mitsuba/core/fwd.h>

/// Declare a pybind11 extern binding function for a set of bindings under a given name
#define MTS_PY_DECLARE(name)                                                    \
    extern void python_export_scalar_rgb_##name(py::module &);                  \
    extern void python_export_scalar_mono_##name(py::module &);                 \
    extern void python_export_scalar_spectral_##name(py::module &);             \
    extern void python_export_scalar_spectral_polarized_##name(py::module &);   \
    extern void python_export_packet_rgb_##name(py::module &);                  \
    extern void python_export_packet_spectral_##name(py::module &);             \


/// Define a python submodule for each rendering mode
#define MTS_PY_DEF_SUBMODULE(list, lib)                                         \
    auto __submodule__scalar_rgb =  m.def_submodule("scalar_rgb").def_submodule(#lib); \
    list.push_back(__submodule__scalar_rgb);                                    \
    auto __submodule__scalar_mono =  m.def_submodule("scalar_mono").def_submodule(#lib); \
    list.push_back(__submodule__scalar_mono);                                   \
    auto __submodule__scalar_spectral =  m.def_submodule("scalar_spectral").def_submodule(#lib); \
    list.push_back(__submodule__scalar_spectral);                               \
    auto __submodule__scalar_spectral_polarized =  m.def_submodule("scalar_spectral_polarized").def_submodule(#lib); \
    list.push_back(__submodule__scalar_spectral_polarized);                     \
    auto __submodule__packet_rgb =  m.def_submodule("packet_rgb").def_submodule(#lib); \
    list.push_back(__submodule__packet_rgb);                                    \
    auto __submodule__packet_spectral =  m.def_submodule("packet_spectral").def_submodule(#lib); \
    list.push_back(__submodule__packet_spectral);                               \


/// Execute the pybind11 binding function for a set of bindings under a given name
#define MTS_PY_IMPORT(name)                                                     \
    python_export_scalar_rgb_##name(__submodule__scalar_rgb);                   \
    python_export_scalar_mono_##name(__submodule__scalar_mono);                 \
    python_export_scalar_spectral_##name(__submodule__scalar_spectral);         \
    python_export_scalar_spectral_polarized_##name(__submodule__scalar_spectral_polarized); \
    python_export_packet_rgb_##name(__submodule__packet_rgb);                   \
    python_export_packet_spectral_##name(__submodule__packet_spectral);         \


/// Define the pybind11 binding function for a set of bindings under a given name
#define MTS_PY_EXPORT(name)                                                     \
    template <typename Float, typename Spectrum>                                \
    void instantiate_##name(py::module m);                                      \
                                                                                \
    void python_export_scalar_rgb_##name(py::module &m) {                       \
        instantiate_##name<float, Color<float, 3>>(m);                          \
    }                                                                           \
    void python_export_scalar_mono_##name(py::module &m) {                      \
        instantiate_##name<float, Color<float, 1>>(m);                          \
    }                                                                           \
    void python_export_scalar_spectral_##name(py::module &m) {                  \
        instantiate_##name<float, Spectrum<float, 4>>(m);                       \
    }                                                                           \
    void python_export_scalar_spectral_polarized_##name(py::module &m) {        \
        instantiate_##name<float, MuellerMatrix<Spectrum<float, 4>>>(m);        \
    }                                                                           \
    void python_export_packet_rgb_##name(py::module &m) {                       \
        instantiate_##name<Packet<float>, Color<Packet<float>, 3>>(m);          \
    }                                                                           \
    void python_export_packet_spectral_##name(py::module &m) {                  \
        instantiate_##name<Packet<float>, Spectrum<Packet<float>, 4>>(m);       \
    }                                                                           \
                                                                                \
    template <typename Float, typename Spectrum>                                \
    void instantiate_##name(py::module m)                                       \


/// Define the pybind11 binding function for a structures under a given name
#define MTS_PY_EXPORT_STRUCT(name)                                              \
    template <typename Float, typename Spectrum>                                \
    void instantiate_##name(py::module m);                                      \
                                                                                \
    void python_export_scalar_rgb_##name(py::module &m) {                       \
        instantiate_##name<float, Color<float, 3>>(m);                          \
    }                                                                           \
    void python_export_scalar_mono_##name(py::module &m) {                      \
        instantiate_##name<float, Color<float, 1>>(m);                          \
    }                                                                           \
    void python_export_scalar_spectral_##name(py::module &m) {                  \
        instantiate_##name<float, Spectrum<float, 4>>(m);                       \
    }                                                                           \
    void python_export_scalar_spectral_polarized_##name(py::module &m) {        \
        instantiate_##name<float, MuellerMatrix<Spectrum<float, 4>>>(m);        \
    }                                                                           \
    void python_export_packet_rgb_##name(py::module &m) {                       \
        instantiate_##name<DynamicArray<Packet<float>>, Color<DynamicArray<Packet<float>>, 3>>(m); \
    }                                                                           \
    void python_export_packet_spectral_##name(py::module &m) {                  \
        instantiate_##name<DynamicArray<Packet<float>>, Spectrum<DynamicArray<Packet<float>>, 4>>(m); \
    }                                                                           \
                                                                                \
    template <typename Float, typename Spectrum>                                \
    void instantiate_##name(py::module m)                                       \


/// Cast an Object pointer ('o') to the corresponding python object
#define PY_CAST_OBJECT(Type)                                                    \
    if (auto tmp = dynamic_cast<Type *>(o); tmp)                                \
        return py::cast(tmp);                                                   \


/// Cast any variants of an Object pointer to the corresponding python object
#define PY_CAST_OBJECT_VARIANTS(Name)                                           \
    PY_CAST_OBJECT(PYBIND11_TYPE(Name<float, Color<float, 3>>))                 \
    PY_CAST_OBJECT(PYBIND11_TYPE(Name<float, Color<float, 1>>))                 \
    PY_CAST_OBJECT(PYBIND11_TYPE(Name<float, Spectrum<float, 4>>))              \
    PY_CAST_OBJECT(PYBIND11_TYPE(Name<float, MuellerMatrix<Spectrum<float, 4>>>)) \
    PY_CAST_OBJECT(PYBIND11_TYPE(Name<Packet<float>, Color<Packet<float>, 3>>)) \
    PY_CAST_OBJECT(PYBIND11_TYPE(Name<Packet<float>, Spectrum<Packet<float>, 4>>)) \


/// Cast a void pointer ('ptr') to the corresponding python object given a std::type_info 'type'
#define PY_CAST(Type)                                                           \
    if (std::string(type.name()) == std::string(typeid(Type).name()))           \
        return py::cast(static_cast<Type *>(ptr));                              \


/// Cast any variants of a void pointer ('ptr') to the corresponding python object
#define PY_CAST_VARIANTS(Type)                                                  \
    PY_CAST(PYBIND11_TYPE(typename CoreAliases<float>::Type))                   \
    PY_CAST(PYBIND11_TYPE(typename CoreAliases<Packet<float>>::Type))           \


