
/// RAII helper to catch Ctrl-C keypresses and cancel an ongoing render job
struct MTS_EXPORT ScopedSignalHandler {
    using IntegratorT = mitsuba::Integrator<MTS_VARIANT_FLOAT, MTS_VARIANT_SPECTRUM>;

    // Defined in integrator_v.cpp
    ScopedSignalHandler(IntegratorT *);
    ~ScopedSignalHandler();
};

