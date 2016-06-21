#pragma once

#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/stream.h>
#include <iosfwd>
#include "logger.h"

namespace fs = mitsuba::filesystem;
using path = fs::path;

NAMESPACE_BEGIN(mitsuba)

/** \brief Simple \ref Stream implementation backed-up by a file.
 * The underlying file abstraction is std::fstream, and so most
 * operations can be expected to behave similarly.
 */
class MTS_EXPORT_CORE FileStream : public Stream {
public:

    /** \brief Constructs a new FileStream by opening the file pointed by <tt>p</tt>.
     * The file is opened in read-only or read/write mode as specified by <tt>writeEnabled</tt>.
     *
     * If <tt>writeEnabled</tt> and the file did not exist before, it is
     * created.
     * Throws if trying to open a non-existing file in with write disabled.
     * Throws an exception if the file cannot be opened / created.
     */
    FileStream(const fs::path &p, bool writeEnabled);

    /// Returns a string representation
    std::string toString() const override;

    /** \brief Closes the stream and the underlying file.
     * No further read or write operations are permitted.
     *
     * This function is idempotent.
     * It is called automatically by the destructor.
     */
    virtual void close() override;

    /// Whether the stream is closed (no read or write are then permitted).
    virtual bool isClosed() const override;

    // =========================================================================
    //! @{ \name Implementation of the Stream interface
    // Most methods can be delegated directly to the underlying
    // standard file stream, avoiding having to deal with portability.
    // =========================================================================
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

    /// Seeks to a position inside the stream. May throw if the resulting state is invalid.
    virtual void seek(size_t pos) override;

    /** \brief Truncates the file to a given size.
     * Automatically flushes the stream before truncating the file.
     * The position is updated to <tt>min(old_position, size)</tt>.
     *
     * Throws an exception if in read-only mode.
     */
    virtual void truncate(size_t size) override;

    /// Gets the current position inside the file
    virtual size_t getPos() const override;

    /** \brief Returns the size of the file.
     * \note After a write, the size may not be updated
     * until a \ref flush is performed.
     */
    virtual size_t getSize() const override {
        return fs::file_size(m_path);
    }

    /// Flushes any buffered operation to the underlying file.
    virtual void flush() override;

    /// Whether the field was open in write-mode (and was not closed)
    virtual bool canWrite() const override { return m_writeEnabled && !isClosed(); }

    /// True except if the stream was closed.
    virtual bool canRead() const override { return !isClosed(); }

    //! @}
    // =========================================================================

    MTS_DECLARE_CLASS()

protected:

    /// Destructor
    virtual ~FileStream();

private:

    fs::path m_path;
    mutable std::unique_ptr<std::fstream> m_file;
    bool m_writeEnabled;
};

NAMESPACE_END(mitsuba)
