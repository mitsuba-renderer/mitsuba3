#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/simd.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER BSDF : public Object {
public:
    virtual void dummy() = 0;

    virtual bool uses_ray_differentials() const {
        return m_uses_ray_differentials;
    };

    MTS_DECLARE_CLASS()

protected:
    virtual ~BSDF() { }

    bool m_uses_ray_differentials;
};

using BSDFPointer = Array<const BSDF*, PacketSize>;

NAMESPACE_END(mitsuba)


// -----------------------------------------------------------------------
//! @{ \name Enoki support for packets of Shape pointers
// -----------------------------------------------------------------------

// Enable usage of array pointers for our types
ENOKI_CALL_SUPPORT_BEGIN(mitsuba::BSDFPointer)
ENOKI_CALL_SUPPORT_SCALAR(uses_ray_differentials)
ENOKI_CALL_SUPPORT_END(mitsuba::BSDFPointer)

ENOKI_CALL_SUPPORT_BEGIN(const mitsuba::BSDFPointer)
ENOKI_CALL_SUPPORT_SCALAR(uses_ray_differentials)
ENOKI_CALL_SUPPORT_END(const mitsuba::BSDFPointer)

//! @}
// -----------------------------------------------------------------------
