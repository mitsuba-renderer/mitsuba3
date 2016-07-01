#pragma once

#include <mitsuba/core/object.h>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Descriptor for specifying the contents and in-memory layout of a
 * POD-style data record
 */
class MTS_EXPORT_CORE Struct : public Object {
public:
    /// Type of a field in the \c Struct
    enum Type {
        EInt8, EUInt8,
        EInt16, EUInt16,
        EInt32, EUInt32,
        EFloat16, EFloat32,
        EFloat64
    };
    
    /// Field specifier with size and offset
    struct Field {
        std::string name;
        Type type;
        size_t size;
        size_t offset;
    };

    /// Create a new \c Struct and indicate whether the contents are packed or aligned
    Struct(bool pack = true);

    /// Append a new field to the \c Struct; determines size and offset automatically
    void append(const std::string &name, Type type);

    /// Append a new field to the \c Struct (manual version)
    void append(Field field) { m_fields.push_back(field); }

    /// Access an individual field entry
    const Field &operator[](size_t i) const { return m_fields[i]; }

    /// Return the size (in bytes) of the data structure, including padding
    size_t size() const; 

    /// Return the alignment (in bytes) of the data structure
    size_t alignment() const; 

    /// Return the number of fields
    size_t fieldCount() const { return m_fields.size(); }

    /// Return a string representation
    std::string toString() const override;

    MTS_DECLARE_CLASS()

protected:
    std::vector<Field> m_fields;
    bool m_pack;
};

NAMESPACE_END(mitsuba)
