#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/stream.h>

NAMESPACE_BEGIN(mitsuba)

/** \brief Simple \ref Stream implementation for accessing files.
 *
 * TODO: explain which low-level tools are used for implementation.
 */
class MTS_EXPORT_CORE FileStream : public Stream {
public:

    /// Returns a string representation
    std::string toString() const override;

    // =============================================================
    //! @{ \name Implementation of the Stream interface
    // =============================================================

    // TODO

    //! @}
    // =============================================================

};

NAMESPACE_END(mitsuba)
