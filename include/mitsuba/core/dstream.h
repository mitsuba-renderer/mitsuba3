#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/stream.h>

NAMESPACE_BEGIN(mitsuba)

/** \brief \ref Stream implementation that never writes to disk, but keeps track
 * of the size of the content being written.
 * It can be used, for example, to measure the precise amount of memory needed
 * to store serialized content.
 */
class MTS_EXPORT_CORE DummyStream : public Stream {
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
