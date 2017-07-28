#pragma once

#include <mitsuba/render/fwd.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/simd.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER BSDF : public Object {
public:
    virtual void dummy() = 0;

    bool uses_ray_differentials() const {
        return m_uses_ray_differentials;
    };

    MTS_DECLARE_CLASS()

protected:
    virtual ~BSDF() { }

private:
    bool m_uses_ray_differentials;
};

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for packets of BSDF pointers
// -----------------------------------------------------------------------

// Enable usage of array pointers for our types
ENOKI_CALL_SUPPORT_BEGIN(mitsuba::BSDFP)
ENOKI_CALL_SUPPORT_SCALAR(uses_ray_differentials)
ENOKI_CALL_SUPPORT_END(mitsuba::BSDFP)

//! @}
// -----------------------------------------------------------------------
