#pragma once

#include <mitsuba/core/stream.h>

NAMESPACE_BEGIN(mitsuba)

/** \brief Simple memory buffer-based stream with automatic memory management
 *
 * The underlying memory storage of this implementation dynamically expands
 * as data is written to the stream.
 */
class MTS_EXPORT_CORE MemoryStream : public Stream {
public:

    // TODO: what about read/write mode for MemoryStream?
    MemoryStream(bool writeEnabled, bool tableOfContents = true)
        : Stream(writeEnabled, tableOfContents) { }

    /// Returns a string representation
    std::string toString() const override;

    // =============================================================
    //! @{ \name Implementation of the Stream interface
    // =============================================================

    // TODO

    //! @}
    // =============================================================

    MTS_DECLARE_CLASS()

protected:

    /// Destructor
    virtual ~MemoryStream();

};

NAMESPACE_END(mitsuba)
