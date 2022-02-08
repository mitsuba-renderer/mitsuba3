#pragma once

#include <mitsuba/core/mmap.h>
#include <mitsuba/core/struct.h>
#include <unordered_map>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Simple exchange format for tensor data of arbitrary rank and size
 *
 * This class provides convenient memory-mapped read-only access to tensor
 * data, usually exported from NumPy.
 */
class MI_EXPORT_LIB TensorFile : public MemoryMappedFile {
public:

    /// Information about the specified field
    struct Field {
        /// Data type (uint32, float, ...) of the tensor
        Struct::Type dtype;

        /// Offset within the file
        size_t offset;

        /// Specifies both rank and size along each dimension
        std::vector<size_t> shape;

        /// Const pointer to the start of the tensor
        const void *data;
    };

    /// Map the specified file into memory
    TensorFile(const fs::path &filename);

    /// Does the file contain a field of the specified name?
    bool has_field(const std::string &name) const;

    /// Return a data structure with information about the specified field
    const Field &field(const std::string &name) const;

    /// Return a human-readable summary
    std::string to_string() const override;

    MI_DECLARE_CLASS()
protected:
    /// Destructor
    ~TensorFile();

private:
    std::unordered_map<std::string, Field> m_fields;
};

NAMESPACE_END(mitsuba)
