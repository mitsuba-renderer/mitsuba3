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
#define MTS_PY_DEF_SUBMODULE(lib)                                               \
    auto __submodule__scalar_rgb =  m.def_submodule("scalar_rgb").def_submodule(#lib); \
    auto __submodule__scalar_mono =  m.def_submodule("scalar_mono").def_submodule(#lib); \
    auto __submodule__scalar_spectral =  m.def_submodule("scalar_spectral").def_submodule(#lib); \
    auto __submodule__scalar_spectral_polarized =  m.def_submodule("scalar_spectral_polarized").def_submodule(#lib); \
    auto __submodule__packet_rgb =  m.def_submodule("packet_rgb").def_submodule(#lib); \
    auto __submodule__packet_spectral =  m.def_submodule("packet_spectral").def_submodule(#lib); \


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


#define PY_CAST_VARIANTS(Name)                                                  \
    if (auto tmp = dynamic_cast<Name<float, Color<float, 3>> *>(o); tmp)        \
        return py::cast(tmp);                                                   \
    if (auto tmp = dynamic_cast<Name<float, Color<float, 1>> *>(o); tmp)        \
        return py::cast(tmp);                                                   \
    if (auto tmp = dynamic_cast<Name<float, Spectrum<float, 4>> *>(o); tmp)     \
        return py::cast(tmp);                                                   \
    if (auto tmp = dynamic_cast<Name<float, MuellerMatrix<Spectrum<float, 4>>> *>(o); tmp) \
        return py::cast(tmp);                                                   \
    if (auto tmp = dynamic_cast<Name<Packet<float>, Color<Packet<float>, 3>> *>(o); tmp) \
        return py::cast(tmp);                                                   \
    if (auto tmp = dynamic_cast<Name<Packet<float>, Spectrum<Packet<float>, 4>> *>(o); tmp) \
        return py::cast(tmp);                                                   \


