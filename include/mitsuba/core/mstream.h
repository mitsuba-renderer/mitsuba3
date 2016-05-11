#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/stream.h>

NAMESPACE_BEGIN(mitsuba)

/** \brief Simple memory buffer-based stream with automatic memory management
 *
 * The underlying memory storage of this implementation dynamically expands
 * as data is written to the stream.
 */
class MTS_EXPORT_CORE MemoryStream : public Stream {
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
