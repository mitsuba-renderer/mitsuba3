#pragma once

#include <mitsuba/core/fstream.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Basic cross-platform abstraction for memory mapped files
 *
 * \remark The Python API has one additional constructor
 * <tt>MemoryMappedFile(filename, array)<tt>, which creates a new
 * file, maps it into memory, and copies the array contents.
 */
class MI_EXPORT_LIB MemoryMappedFile : public Object {
public:
    /// Create a new memory-mapped file of the specified size
    MemoryMappedFile(const fs::path &filename, size_t size);

    /// Map the specified file into memory
    MemoryMappedFile(const fs::path &filename, bool write = false);

    /// Return a pointer to the file contents in memory
    void *data();

    /// Return a pointer to the file contents in memory (const version)
    const void *data() const;

    /// Return the size of the mapped region
    size_t size() const;

    /**
     * \brief Resize the memory-mapped file
     *
     * This involves remapping the file, which will
     * generally change the pointer obtained via data()
     */
    void resize(size_t size);

    /// Return the associated filename
    const fs::path &filename() const;

    /// Return whether the mapped memory region can be modified
    bool can_write() const;

    /// Return a string representation
    std::string to_string() const override;

    /**
     * \brief Create a temporary memory-mapped file
     *
     * \remark When closing the mapping, the file is automatically deleted.
     *         Mitsuba additionally informs the OS that any outstanding changes
     *         that haven't yet been written to disk can be discarded
     *         (Linux/OSX only).
     */
    static ref<MemoryMappedFile> create_temporary(size_t size);

    MI_DECLARE_CLASS()
protected:
    /// Internal constructor
    MemoryMappedFile();

    /// Release all resources
    virtual ~MemoryMappedFile();

private:
    struct MemoryMappedFilePrivate;
    std::unique_ptr<MemoryMappedFilePrivate> d;
};

NAMESPACE_END(mitsuba)
