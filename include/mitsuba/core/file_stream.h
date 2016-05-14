#pragma once

#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/stream.h>
#include <fstream>
#include "logger.h"

namespace fs = mitsuba::filesystem;
using path = fs::path;

NAMESPACE_BEGIN(mitsuba)

/** \brief Simple \ref Stream implementation backed-up by a file.
 *
 * TODO: explain which low-level tools are used for implementation.
 */
class MTS_EXPORT_CORE FileStream : public Stream {
public:

    /** \brief Constructs a new FileStream by opening the file pointed by <tt>p</tt>.
     * The file is opened in append mode, and read / write mode as specified
     * by <tt>writeEnabled</tt>.
     *
     * Throws an exception if the file cannot be open.
     */
    FileStream(const fs::path &p, bool writeEnabled);

    /// Returns a string representation
    std::string toString() const override;

    // =========================================================================
    //! @{ \name Implementation of the Stream interface
    // Most methods can be delegated directly to the underlying
    // standard file stream, avoiding having to deal with portability.
    // =========================================================================
protected:

    /**
     * \brief Reads a specified amount of data from the stream.
     * Throws an exception when the stream ended prematurely.
     */
    virtual void read(void *p, size_t size) override;

    /**
     * \brief Writes a specified amount of data into the stream.
     * Throws an exception when not all data could be written.
     */
    virtual void write(const void *p, size_t size) override;

public:

    /** Seeks to a position inside the stream. Throws an exception when trying
     * to seek beyond the limits of the file.
     */
    virtual void seek(size_t pos) override;

    /** \brief Truncates the file to a given size.
     * Automatically flushes the stream before truncating the file.
     * The position is updated to <tt>min(old_position, size)</tt>.
     *
     * Throws an exception if in read-only mode.
     */
    virtual void truncate(size_t size) override;

    /// Gets the current position inside the file
    virtual size_t getPos() override;

    /// Returns the size of the file
    // TODO: would need to flush first to get accurate results?
    virtual size_t getSize() override {
        return fs::file_size(m_path);
    }

    /// Flushes any buffered operation to the underlying file.
    virtual void flush() override {
        m_file.flush();
    }

    //! @}
    // =========================================================================

    MTS_DECLARE_CLASS()

protected:

    /// Destructor
    virtual ~FileStream();

private:

    fs::path m_path;
    std::fstream m_file;

};

NAMESPACE_END(mitsuba)
