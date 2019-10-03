#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Medium : public Object {
public:

    MTS_DECLARE_CLASS()

protected:
    virtual ~Medium();

};

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for packets of Medium pointers
// -----------------------------------------------------------------------

// Enable usage of array pointers for our types
// TODO: re-enable this
// ENOKI_CALL_SUPPORT_BEGIN(mitsuba::Medium)
// ENOKI_CALL_SUPPORT_END(mitsuba::Medium)

//! @}
// -----------------------------------------------------------------------
