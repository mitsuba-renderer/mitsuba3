#pragma once

#include <mitsuba/core/stream.h>

NAMESPACE_BEGIN(mitsuba)

/** \brief Simple \ref Stream implementation for accessing files.
 *
 * TODO: explain which low-level tools are used for implementation.
 */
class MTS_EXPORT_CORE FileStream : public Stream {
public:

    FileStream(bool writeEnabled, bool tableOfContents = true)
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
    virtual ~FileStream();

};

NAMESPACE_END(mitsuba)
