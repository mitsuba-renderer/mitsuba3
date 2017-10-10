#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Emitter : public Object {
public:
    /**
     * \brief Flags used to classify the emission profile of
     * different types of emitters.
     */
    enum EEmitterType {
        /// Emission profile contains a Dirac delta term with respect to direction.
        EDeltaDirection = 0x01,

        /// Emission profile contains a Dirac delta term with respect to position.
        EDeltaPosition  = 0x02,

        /// Is the emitter associated with a surface in the scene?
        EOnSurface      = 0x04
    };

    // =============================================================
    //! @{ \name Other query functions
    // =============================================================

    /// Return the local space to world space transformation
    const AnimatedTransform *world_transform() const;


    /// Set the local space to world space transformation
    void set_world_transform(const AnimatedTransform *trafo);

    //! @}
    // =============================================================

    MTS_DECLARE_CLASS()

protected:
    Emitter(const Properties &props);

    virtual ~Emitter();

protected:
    Properties m_properties;

    ref<const AnimatedTransform> m_world_transform;
    uint32_t m_type;
};

NAMESPACE_END(mitsuba)
