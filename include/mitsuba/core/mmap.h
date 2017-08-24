#pragma once

#include <mitsuba/core/fstream.h>

NAMESPACE_BEGIN(mitsuba)

/// Basic cross-platform abstraction for memory mapped files
class MTS_EXPORT_CORE MemoryMappedFile : public Object {
public:
    /// Create a new memory-mapped file of the specified size
    MemoryMappedFile(const fs::path &filename, size_t size);

    /// Map the specified file into memory
    MemoryMappedFile(const fs::path &filename, bool read_only = true);

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

    /// Return whether the mapped memory region is read-only
    bool is_read_only() const;

    /// Return a string representation
    std::string to_string() const override;

    /**
     * \brief Create a temporary memory-mapped file
     *
     * \remark When closing the mapping, the file is automatically deleted.
     *         Mitsuba additionally informs the OS that any outstanding changes
     *         that haven't yet been written to disk can be discarded (Linux only).
     */
    static ref<MemoryMappedFile> create_temporary(size_t size);

    /// Release all resources
    virtual ~MemoryMappedFile();

    MTS_DECLARE_CLASS()
protected:
    /// Internal constructor
    MemoryMappedFile();
private:
    struct MemoryMappedFilePrivate;
    std::unique_ptr<MemoryMappedFilePrivate> d;
};

NAMESPACE_END(mitsuba)
