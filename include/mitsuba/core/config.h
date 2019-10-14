#define MTS_CONFIGURATIONS                                                  \
    "scalar-mono\n"                                                         \
    "scalar-rgb\n"                                                          \
    "scalar-spectral\n"                                                     \
    "scalar-spectral-polarized\n"                                           \


#define MTS_DECLARE_PLUGIN()                                                \
    MTS_DECLARE_CLASS()                                                     \
    MTS_IMPORT_TYPES()                                                      \


#define MTS_IMPLEMENT_PLUGIN(Name, Parent, Descr)                           \
    MTS_IMPLEMENT_CLASS_TEMPLATE(Name, Parent)                              \
    extern "C" {                                                            \
        MTS_EXPORT const char *plugin_descr = Descr;                        \
        MTS_EXPORT Object *plugin_create(const char *config,                \
                                         const Properties &props) {         \
            constexpr size_t PacketSize = enoki::max_packet_size / sizeof(float); \
            ENOKI_MARK_USED(PacketSize);                                    \
            if (strcmp(config, "scalar-mono") == 0) {                       \
                using Float = float;                                        \
                return new Name<float, Color<Float, 1>>(props);             \
            } else if (strcmp(config, "scalar-rgb") == 0) {                 \
                using Float = float;                                        \
                return new Name<float, Color<Float, 3>>(props);             \
            } else if (strcmp(config, "scalar-spectral") == 0) {            \
                using Float = float;                                        \
                return new Name<float, Spectrum<Float, 4>>(props);          \
            } else if (strcmp(config, "scalar-spectral-polarized") == 0) {  \
                using Float = float;                                        \
                return new Name<float, MuellerMatrix<Spectrum<Float, 4>>>(props); \
            } else {                                                        \
                return nullptr;                                             \
            }                                                               \
        }                                                                   \
    }                                                                       \
                                                                            \
template class MTS_EXPORT_CORE Name<float, Color<float, 1>>;                \
template class MTS_EXPORT_CORE Name<float, Color<float, 3>>;                \
template class MTS_EXPORT_CORE Name<float, Spectrum<float, 4>>;             \
template class MTS_EXPORT_CORE Name<float, MuellerMatrix<Spectrum<float, 4>>>; \

