
/// RAII helper to catch Ctrl-C keypresses and cancel an ongoing render job
struct MI_EXPORT ScopedSignalHandler {
    using IntegratorT = mitsuba::Integrator<MI_VARIANT_FLOAT, MI_VARIANT_SPECTRUM>;

    // Defined in integrator_v.cpp
    ScopedSignalHandler(IntegratorT *);
    ~ScopedSignalHandler();
};

