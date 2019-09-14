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
            if (strcmp(config, "scalar-mono") == 0)                         \
                return new Name<float, Color1f>(props);                     \
            else if (strcmp(config, "scalar-rgb") == 0)                     \
                return new Name<float, Color3f>(props);                     \
            else if (strcmp(config, "scalar-spectral") == 0)                \
                return new Name<float, Spectrumf>(props);                   \
            else if (strcmp(config, "scalar-spectral-polarized") == 0)      \
                return new Name<float, MuellerMatrixSf>(props);             \
            else                                                            \
                return nullptr;                                             \
        }                                                                   \
    }                                                                       \
                                                                            \
    template class MTS_EXPORT_CORE Name<float, Color1f>;                    \
    template class MTS_EXPORT_CORE Name<float, Color3f>;                    \
    template class MTS_EXPORT_CORE Name<float, Spectrumf>;                  \
    template class MTS_EXPORT_CORE Name<float, MuellerMatrixSf>;            \


