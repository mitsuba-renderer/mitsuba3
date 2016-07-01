#include <mitsuba/core/structconv.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/math.h>
#include <ostream>

NAMESPACE_BEGIN(mitsuba)

Struct::Struct(bool pack) : m_pack(pack) { }

size_t Struct::size() const {
    if (m_fields.empty())
        return 0;
    auto const &last = m_fields[m_fields.size() - 1];
    size_t size = last.offset + last.size;
    if (!m_pack) {
        size_t align = alignment();
        size += math::modulo(align - size, align);
    }
    return size;
}

size_t Struct::alignment() const { 
    if (m_pack)
        return 1;
    size_t size = 1;
    for (auto const &field : m_fields)
        size = std::max(size, field.size);
    return size;
}

void Struct::append(const std::string &name, Type type) {
    Field f;
    f.name = name;
    f.type = type;
    if (m_fields.empty()) {
        f.offset = 0;
    } else {
        auto const &last = m_fields[m_fields.size() - 1];
        f.offset = last.offset + last.size;
    }
    switch (type) {
        case EInt8:
        case EUInt8:   f.size = 1; break;
        case EInt16:
        case EUInt16:
        case EFloat16: f.size = 2; break;
        case EInt32:
        case EUInt32: 
        case EFloat32: f.size = 4; break;
        case EFloat64: f.size = 8; break;
        default: Throw("Struct::append(): invalid field type!"); 
    }
    if (!m_pack)
        f.offset += math::modulo(f.size - f.offset, f.size);
    m_fields.push_back(f);
}

std::string Struct::toString() const {
    std::ostringstream os;
    os << "Struct[" << std::endl;
    for (size_t i = 0; i < m_fields.size(); ++i) {
        os << "    ";
        switch (m_fields[i].type) {
            case EInt8:    os << "int8";    break;
            case EUInt8:   os << "uint8";   break;
            case EInt16:   os << "int16";   break;
            case EUInt16:  os << "uint16";  break;
            case EInt32:   os << "int32";   break;
            case EUInt32:  os << "uint32";  break;
            case EFloat16: os << "float16"; break;
            case EFloat32: os << "float32"; break;
            case EFloat64: os << "float64"; break;
            default: Throw("Struct::toString(): invalid field type!"); 
        }
        os << " " << m_fields[i].name << "; // @" << m_fields[i].offset;
        if (i + 1 < m_fields.size())
            os << ",";
        os << "\n";
    }
    os << "]";
    return os.str();
}

MTS_IMPLEMENT_CLASS(Struct, Object)
NAMESPACE_END(mitsuba)
