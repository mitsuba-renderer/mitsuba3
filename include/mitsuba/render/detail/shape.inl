// -----------------------------------------------------------------------
//! @{ \name Enoki support for vectorized function calls
// -----------------------------------------------------------------------

// TODO: re-enable this
// ENOKI_CALL_SUPPORT_BEGIN(mitsuba::Shape)
//     ENOKI_CALL_SUPPORT_METHOD(normal_derivative)
//     ENOKI_CALL_SUPPORT_METHOD(fill_surface_interaction)
//     ENOKI_CALL_SUPPORT_GETTER_TYPE(emitter, m_emitter, const mitsuba::Emitter *)
//     ENOKI_CALL_SUPPORT_GETTER_TYPE(sensor, m_sensor, const mitsuba::Sensor *)
//     ENOKI_CALL_SUPPORT_GETTER_TYPE(bsdf, m_bsdf, const mitsuba::BSDF *)
//     ENOKI_CALL_SUPPORT_GETTER_TYPE(interior_medium, m_interior_medium, const mitsuba::Medium *)
//     ENOKI_CALL_SUPPORT_GETTER_TYPE(exterior_medium, m_exterior_medium, const mitsuba::Medium *)
//     auto is_emitter() const { return neq(emitter(), nullptr); }
//     auto is_sensor() const { return neq(sensor(), nullptr); }
//     auto is_medium_transition() const { return neq(interior_medium(), nullptr) ||
//                                                neq(exterior_medium(), nullptr); }
// ENOKI_CALL_SUPPORT_END(mitsuba::Shape)

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name Macro for template implementation of shape methods
// -----------------------------------------------------------------------

/*
 * \brief These macros should be used in the definition of Shape
 * plugins to instantiate concrete versions of the interface.
 */
#define MTS_IMPLEMENT_SHAPE_SCALAR()                                           \
    PositionSample3f sample_position(Float time, const Point2f &sample)        \
        const override {                                                       \
        return sample_position_impl(time, sample, true);                       \
    }                                                                          \
    Float pdf_position(const PositionSample3f &ps) const override {            \
        return pdf_position_impl(ps, true);                                    \
    }                                                                          \
    std::pair<bool, Float> ray_intersect(const Ray3f &ray, Float *cache)       \
        const override {                                                       \
        return ray_intersect_impl(ray, cache, true);                           \
    }                                                                          \
    bool ray_test(const Ray3f &ray) const override {                           \
        return ray_test_impl(ray, true);                                       \
    }                                                                          \
    void fill_surface_interaction(const Ray3f &ray, const Float *cache,        \
                                  SurfaceInteraction3f &si) const override {   \
        fill_surface_interaction_impl(ray, cache, si, true);                   \
    }                                                                          \
    std::pair<Vector3f, Vector3f> normal_derivative(                           \
        const SurfaceInteraction3f &si, bool shading_frame) const override {   \
        return normal_derivative_impl(si, shading_frame, true);                \
    }

#define MTS_IMPLEMENT_SHAPE_PACKET()                                           \
    PositionSample3fP sample_position(FloatP time, const Point2fP &sample,     \
                                      MaskP active) const override {           \
        return sample_position_impl(time, sample, active);                     \
    }                                                                          \
    FloatP pdf_position(const PositionSample3fP &ps, MaskP active)             \
        const override {                                                       \
        return pdf_position_impl(ps, active);                                  \
    }                                                                          \
    std::pair<MaskP, FloatP> ray_intersect(const Ray3fP &ray, FloatP *cache,   \
                                           MaskP active) const override {      \
        return ray_intersect_impl(ray, cache, active);                         \
    }                                                                          \
    MaskP ray_test(const Ray3fP &ray, MaskP active) const override {           \
        return ray_test_impl(ray, active);                                     \
    }                                                                          \
    void fill_surface_interaction(const Ray3fP &ray, const FloatP *cache,      \
                                  SurfaceInteraction3fP &si, MaskP active)     \
        const override {                                                       \
        fill_surface_interaction_impl(ray, cache, si, active);                 \
    }                                                                          \
    std::pair<Vector3fP, Vector3fP> normal_derivative(                         \
        const SurfaceInteraction3fP &si, bool shading_frame, MaskP active)     \
        const override {                                                       \
        return normal_derivative_impl(si, shading_frame, active);              \
    }

#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_SHAPE_AUTODIFF()
#else
#  define MTS_IMPLEMENT_SHAPE_AUTODIFF()                                       \
    PositionSample3fD sample_position(FloatD time, const Point2fD &sample,     \
                                      MaskD active) const override {           \
        return sample_position_impl(time, sample, active);                     \
    }                                                                          \
    FloatD pdf_position(const PositionSample3fD &ps, MaskD active)             \
        const override {                                                       \
        return pdf_position_impl(ps, active);                                  \
    }                                                                          \
    std::pair<MaskD, FloatD> ray_intersect(const Ray3fD &ray, FloatD *cache,   \
                                           MaskD active) const override {      \
        return ray_intersect_impl(ray, cache, active);                         \
    }                                                                          \
    MaskD ray_test(const Ray3fD &ray, MaskD active) const override {           \
        return ray_test_impl(ray, active);                                     \
    }                                                                          \
    void fill_surface_interaction(const Ray3fD &ray, const FloatD *cache,      \
                                  SurfaceInteraction3fD &si, MaskD active)     \
        const override {                                                       \
        fill_surface_interaction_impl(ray, cache, si, active);                 \
    }                                                                          \
    std::pair<Vector3fD, Vector3fD> normal_derivative(                         \
        const SurfaceInteraction3fD &si, bool shading_frame, MaskD active)     \
        const override {                                                       \
        return normal_derivative_impl(si, shading_frame, active);              \
    }
#endif

#define MTS_IMPLEMENT_SHAPE_ALL()                                              \
    MTS_IMPLEMENT_SHAPE_SCALAR()                                               \
    MTS_IMPLEMENT_SHAPE_PACKET()                                               \
    MTS_IMPLEMENT_SHAPE_AUTODIFF()

#define MTS_IMPLEMENT_SHAPE_SAMPLE_DIRECTION_SCALAR()                          \
    DirectionSample3f sample_direction(const Interaction3f &it,                \
                                       const Point2f &sample) const override { \
        return sample_direction_impl(it, sample, true);                        \
    }                                                                          \
    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds)  \
        const override {                                                       \
        return pdf_direction_impl(it, ds, true);                               \
    }

#define MTS_IMPLEMENT_SHAPE_SAMPLE_DIRECTION_PACKET()                          \
    DirectionSample3fP sample_direction(const Interaction3fP &it,              \
                                        const Point2fP &sample, MaskP active)  \
        const override {                                                       \
        return sample_direction_impl(it, sample, active);                      \
    }                                                                          \
    FloatP pdf_direction(const Interaction3fP &it,                             \
                         const DirectionSample3fP &ds, MaskP active)           \
        const override {                                                       \
        return pdf_direction_impl(it, ds, active);                             \
    }

#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_SHAPE_SAMPLE_DIRECTION_AUTODIFF()
#else
#  define MTS_IMPLEMENT_SHAPE_SAMPLE_DIRECTION_AUTODIFF()                      \
    DirectionSample3fD sample_direction(const Interaction3fD &it,              \
                                        const Point2fD &sample, MaskD active)  \
        const override {                                                       \
        return sample_direction_impl(it, sample, active);                      \
    }                                                                          \
    FloatD pdf_direction(const Interaction3fD &it,                             \
                         const DirectionSample3fD &ds, MaskD active)           \
        const override {                                                       \
        return pdf_direction_impl(it, ds, active);                             \
    }
#endif

#define MTS_IMPLEMENT_SHAPE_SAMPLE_DIRECTION_ALL()                             \
    MTS_IMPLEMENT_SHAPE_SAMPLE_DIRECTION_SCALAR()                              \
    MTS_IMPLEMENT_SHAPE_SAMPLE_DIRECTION_PACKET()                              \
    MTS_IMPLEMENT_SHAPE_SAMPLE_DIRECTION_AUTODIFF()

//! @}
// -----------------------------------------------------------------------
