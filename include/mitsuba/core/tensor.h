#pragma once

#include <mitsuba/core/mmap.h>
#include <mitsuba/core/struct.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Simple exchange format for tensor data of arbitrary rank and size
 *
 * This class provides convenient memory-mapped read-only access to tensor
 * data, usually exported from NumPy.
 *
 * The Python functions :python:func:`mi.tensor_io.write(filename, tensor_1=..,
 * tensor_2=.., ...) <mitsuba.tensor_io.write>` and :py:func:`tensor_file =
 * mi.tensor_io.read(filename) <mitsuba.tensor_io.read>` can be used to create
 * and modify these files within Python.
 *
 * On the C++ end, use ``tensor_file.field("field_name").as<TensorXf>()`` to
 * upload the data and obtain a device tensor handle.
 */
class MI_EXPORT_LIB TensorFile : public MemoryMappedFile {
public:

    /// Information about the specified field
    struct MI_EXPORT_LIB Field {
        /// Data type (uint32, float, ...) of the tensor
        Struct::Type dtype;

        /// Offset within the file
        size_t offset;

        /// Specifies both rank and size along each dimension
        std::vector<size_t> shape;

        /// Const pointer to the start of the tensor
        const void *data;

        /// Return a human-readable summary
        std::string to_string() const;

        template <typename T> T to() const {
            if (struct_type_v<dr::scalar_t<T>> != dtype)
                Throw("Tensor::TensorFile::to(): incompatible component format!");
            return T(data, shape.size(), shape.data());
        }
    };

    /// Map the specified file into memory
    TensorFile(const fs::path &filename);

    /// Destructor
    ~TensorFile();

    /// Does the file contain a field of the specified name?
    bool has_field(std::string_view name) const;

    /// Return a data structure with information about the specified field
    const Field &field(std::string_view name) const;

    /// Return a human-readable summary
    std::string to_string() const override;

    MI_DECLARE_CLASS(TensorFile)

private:
    struct TensorFileImpl;
    dr::unique_ptr<TensorFileImpl> d;
};

NAMESPACE_END(mitsuba)
