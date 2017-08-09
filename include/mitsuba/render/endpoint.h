
#pragma once

#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Endpoint: an abstract interface to light sources and sensors
 *
 * This class implements an abstract interface to all sensors and light sources
 * emitting radiance and importance, respectively. Subclasses must implement
 * functions for evaluating and sampling the emission profile and furthermore
 * support querying the probability density of the provided sampling technique.
 *
 * Subclasses must also provide a specialized \a direct sampling method
 * (a generalization of direct illumination sampling to both emitters \a and
 * sensors). A direct sampling is given an arbitrary input position in the
 * scene and in turn returns a sampled emitter position and direction, which
 * has a nonzero contribution towards the provided position. The main idea is
 * that direct sampling reduces the underlying space from 4D to 2D, hence it
 * is often possible to use smarter sampling techniques than in the fully
 * general case.
 *
 * Since the emission profile is defined as function over both positions
 * and directions, there are functions to sample and query \a each of the
 * two components separately. Furthermore, there is a convenience function
 * to sample both at the same time, which is mainly used by unidirectional
 * rendering algorithms that do not need this level of flexibility.
 *
 * One underlying assumption of this interface is that position and
 * direction sampling will happen <em>in sequence</em>. This means that
 * the direction sampling step is allowed to statistically condition on
 * properties of the preceding position sampling step.
 *
 * When rendering scenes involving participating media, it is important
 * to know what medium surrounds the sensors and light sources. For
 * this reason, every emitter instance keeps a reference to a medium
 * (or \c nullptr when it is surrounded by vacuum).
 */
class MTS_EXPORT_RENDER Endpoint : public Object {
public:
    /**
     * \brief Flags used to classify the emission profile of
     * different types of emitters
     */
    enum EFlags {
        /// Emission profile contains a Dirac delta term with respect to position
        EDeltaPosition  = 0x01,

        /// Emission profile contains a Dirac delta term with respect to direction
        EDeltaDirection = 0x02,

        /// Is the emitter associated with a surface in the scene?
        EOnSurface      = 0x04
    };

    MTS_DECLARE_CLASS()

protected:
    virtual ~Endpoint();
};

NAMESPACE_END(mitsuba)
