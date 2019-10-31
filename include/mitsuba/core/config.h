#pragma once

#define MTS_CONFIGURATIONS                                                  \
    "scalar_mono\n"                                                         \
    "scalar_rgb\n"                                                          \
    "scalar_spectral\n"                                                     \
    "scalar_spectral_polarized\n"                                           \


#define MTS_INSTANTIATE_OBJECT(Name)                                        \
    template class MTS_EXPORT_CORE Name<float, Color<float, 1>>;            \
    template class MTS_EXPORT_CORE Name<float, Color<float, 3>>;            \
    template class MTS_EXPORT_CORE Name<float, Spectrum<float, 4>>;         \
    template class MTS_EXPORT_CORE Name<float, MuellerMatrix<Spectrum<float, 4>>>; \


#define MTS_INSTANTIATE_STRUCT(Name)                                        \
    template struct MTS_EXPORT_CORE Name<float, Color<float, 1>>;           \
    template struct MTS_EXPORT_CORE Name<float, Color<float, 3>>;           \
    template struct MTS_EXPORT_CORE Name<float, Spectrum<float, 4>>;        \
    template struct MTS_EXPORT_CORE Name<float, MuellerMatrix<Spectrum<float, 4>>>; \


#define MTS_DECLARE_PLUGIN(Name, Parent)                                    \
    MTS_REGISTER_CLASS(Name, Parent)                                        \
    MTS_IMPORT_TYPES()                                                      \


#define MTS_IMPLEMENT_PLUGIN(Name, Parent, Descr)                           \
    extern "C" {                                                            \
        MTS_EXPORT const char *plugin_descr = Descr;                        \
        MTS_EXPORT Object *plugin_create(const char *config,                \
                                         const Properties &props) {         \
            constexpr size_t PacketSize = enoki::max_packet_size / sizeof(float); \
            ENOKI_MARK_USED(PacketSize);                                    \
            if (strcmp(config, "scalar_mono") == 0) {                       \
                using Float = float;                                        \
                return new Name<float, Color<Float, 1>>(props);             \
            } else if (strcmp(config, "scalar_rgb") == 0) {                 \
                using Float = float;                                        \
                return new Name<float, Color<Float, 3>>(props);             \
            } else if (strcmp(config, "scalar_spectral") == 0) {            \
                using Float = float;                                        \
                return new Name<float, Spectrum<Float, 4>>(props);          \
            } else if (strcmp(config, "scalar_spectral_polarized") == 0) {  \
                using Float = float;                                        \
                return new Name<float, MuellerMatrix<Spectrum<Float, 4>>>(props); \
            } else {                                                        \
                return nullptr;                                             \
            }                                                               \
        }                                                                   \
    }                                                                       \
                                                                            \
    MTS_INSTANTIATE_OBJECT(Name)                                            \


#define MTS_PY_EXPORT_FLOAT_VARIANTS(name)                                  \
    template <typename Float>                                               \
    void instantiate_##name(py::module m);                                  \
                                                                            \
    MTS_PY_EXPORT(name) {                                                   \
        instantiate_##name<float>(m);                                       \
    }                                                                       \
                                                                            \
    template <typename Float>                                               \
    void instantiate_##name(py::module m)                                   \


#define MTS_PY_EXPORT_MODE_VARIANTS(name)                                   \
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
        instantiate_##name<float, MuellerMatrix<Spectrum<float, 4>>>(       \
            m.def_submodule("scalar_spectral_polarized"));                  \
    }                                                                       \
                                                                            \
    template <typename Float, typename Spectrum>                            \
    void instantiate_##name(py::module m)                                   \


