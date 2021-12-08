
/// RAII helper to catch Ctrl-C keypresses and cancel an ongoing render job
struct MTS_EXPORT ScopedSignalHandler {
    using Float = MTS_VARIANT_FLOAT;
    using Spectrum = MTS_VARIANT_SPECTRUM;

    // Defined in integrator_v.cpp
    ScopedSignalHandler(Integrator<Float, Spectrum> *);
    ~ScopedSignalHandler();
};

