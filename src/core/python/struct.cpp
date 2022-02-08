#include <mitsuba/core/struct.h>
#include <mitsuba/core/simd.h>
#include <mitsuba/core/logger.h>
#include <pybind11/numpy.h>
#include <mitsuba/python/python.h>

/// Conversion between 'Struct' and NumPy 'dtype' data structures
py::dtype dtype_for_struct(const Struct *s) {
    py::list names, offsets, formats;

    for (auto field: *s) {
        names.append(py::str(field.name));
        offsets.append(py::int_(field.offset));
        std::string format;
        switch (field.type) {
            case Struct::Type::Int8:    format = "int8";   break;
            case Struct::Type::UInt8:   format = "uint8";  break;
            case Struct::Type::Int16:   format = "int16";  break;
            case Struct::Type::UInt16:  format = "uint16"; break;
            case Struct::Type::Int32:   format = "int32";  break;
            case Struct::Type::UInt32:  format = "uint32"; break;
            case Struct::Type::Int64:   format = "int64";  break;
            case Struct::Type::UInt64:  format = "uint64"; break;
            case Struct::Type::Float16: format = "float16";  break;
            case Struct::Type::Float32: format = "float32";  break;
            case Struct::Type::Float64: format = "float64";  break;
            case Struct::Type::Invalid: format = "invalid";  break;
            default: Throw("Internal error.");
        }
        formats.append(py::str(format));
    }
    return py::dtype(names, formats, offsets, s->size());
}

MTS_PY_EXPORT(Struct) {
    auto c = MTS_PY_CLASS(Struct, Object);
    py::class_<Struct::Field> field(c, "Field", D(Struct, Field));

    py::enum_<Struct::Type>(c, "Type")
        .value("Int8",    Struct::Type::Int8,    D(Struct, Type, Int8))
        .value("UInt8",   Struct::Type::UInt8,   D(Struct, Type, UInt8))
        .value("Int16",   Struct::Type::Int16,   D(Struct, Type, Int16))
        .value("UInt16",  Struct::Type::UInt16,  D(Struct, Type, UInt16))
        .value("Int32",   Struct::Type::Int32,   D(Struct, Type, Int32))
        .value("UInt32",  Struct::Type::UInt32,  D(Struct, Type, UInt32))
        .value("Int64",   Struct::Type::Int64,   D(Struct, Type, Int64))
        .value("UInt64",  Struct::Type::UInt64,  D(Struct, Type, UInt64))
        .value("Float16", Struct::Type::Float16, D(Struct, Type, Float16))
        .value("Float32", Struct::Type::Float32, D(Struct, Type, Float32))
        .value("Float64", Struct::Type::Float64, D(Struct, Type, Float64))
        .value("Invalid", Struct::Type::Invalid, D(Struct, Type, Invalid))
        .def(py::init([](py::dtype dt) {
            Struct::Type value = Struct::Type::Int8;
            if (dt.kind() == 'i') {
                switch (dt.itemsize()) {
                    case 1: value = Struct::Type::Int8; break;
                    case 2: value = Struct::Type::Int16; break;
                    case 4: value = Struct::Type::Int32; break;
                    case 8: value = Struct::Type::Int64; break;
                    default: throw py::type_error("Struct::Type(): Invalid integer type!");
                }
            } else if (dt.kind() == 'u') {
                switch (dt.itemsize()) {
                    case 1: value = Struct::Type::UInt8; break;
                    case 2: value = Struct::Type::UInt16; break;
                    case 4: value = Struct::Type::UInt32; break;
                    case 8: value = Struct::Type::UInt64; break;
                    default: throw py::type_error("Struct::Type(): Invalid unsigned integer type!");
                }
            } else if (dt.kind() == 'f') {
                switch (dt.itemsize()) {
                    case 2: value = Struct::Type::Float16; break;
                    case 4: value = Struct::Type::Float32; break;
                    case 8: value = Struct::Type::Float64; break;
                    default: throw py::type_error("Struct::Type(): Invalid floating point type!");
                }
            } else {
                throw py::type_error("Struct::Type(): Invalid type!");
            }
            return new Struct::Type(value);
        }), "dtype"_a);

    py::implicitly_convertible<py::dtype, Struct::Type>();

    py::enum_<Struct::Flags>(c, "Flags", py::arithmetic())
        .value("Normalized", Struct::Flags::Normalized, D(Struct, Flags, Normalized))
        .value("Gamma",      Struct::Flags::Gamma,   D(Struct, Flags, Gamma))
        .value("Weight",     Struct::Flags::Weight,  D(Struct, Flags, Weight))
        .value("Assert",     Struct::Flags::Assert,  D(Struct, Flags, Assert))
        .value("Alpha",     Struct::Flags::Alpha,  D(Struct, Flags, Alpha))
        .value("PremultipliedAlpha",     Struct::Flags::PremultipliedAlpha,  D(Struct, Flags, PremultipliedAlpha))
        .value("Default",    Struct::Flags::Default, D(Struct, Flags, Default))
        .def(py::self | py::self)
        .def(int() | py::self)
        .def(int() & py::self);

    py::enum_<Struct::ByteOrder>(c, "ByteOrder")
        .value("LittleEndian", Struct::ByteOrder::LittleEndian,
            D(Struct, ByteOrder, LittleEndian))
        .value("BigEndian", Struct::ByteOrder::BigEndian,
            D(Struct, ByteOrder, BigEndian))
        .value("HostByteOrder", Struct::ByteOrder::HostByteOrder,
            D(Struct, ByteOrder, HostByteOrder));

    c.def(py::init<bool, Struct::ByteOrder>(), "pack"_a = false,
            "byte_order"_a = Struct::ByteOrder::HostByteOrder,
            D(Struct, Struct))
        .def("append",
            (Struct &(Struct::*)(const std::string&, Struct::Type, uint32_t, double)) &Struct::append,
            "name"_a, "type"_a, "flags"_a = Struct::Flags::None, "default"_a = 0.0,
            D(Struct, append), py::return_value_policy::reference)
        .def("field", py::overload_cast<const std::string &>(&Struct::field), D(Struct, field),
            py::return_value_policy::reference_internal)
        .def("__getitem__", [](Struct &s, size_t i) -> Struct::Field& {
            if (i >= s.field_count())
                throw py::index_error();
            return s[i];
        }, py::return_value_policy::reference_internal)
        .def("__len__", &Struct::field_count)
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__hash__", [](const Struct &s) { return hash(s); })
        .def_method(Struct, size)
        .def_method(Struct, alignment)
        .def_method(Struct, byte_order)
        .def_method(Struct, field_count)
        .def_method(Struct, has_field)
        .def_static("is_float", &Struct::is_float, D(Struct, is_float))
        .def_static("is_integer", &Struct::is_integer, D(Struct, is_integer))
        .def_static("is_signed", &Struct::is_signed, D(Struct, is_signed))
        .def_static("is_unsigned", &Struct::is_unsigned, D(Struct, is_unsigned))
        .def_static("range", &Struct::range, D(Struct, range))
        .def("dtype", &dtype_for_struct, "Return a NumPy dtype corresponding to this data structure");

    field.def("is_float", &Struct::Field::is_float, D(Struct, Field, is_float))
        .def("is_integer", &Struct::Field::is_integer, D(Struct, Field, is_integer))
        .def("is_signed", &Struct::Field::is_signed, D(Struct, Field, is_signed))
        .def("is_unsigned", &Struct::Field::is_unsigned, D(Struct, Field, is_unsigned))
        .def("range", &Struct::Field::range, D(Struct, Field, range))
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__hash__", [](const Struct::Field &f) { return hash(f); })
        .def_readwrite("type", &Struct::Field::type, D(Struct, Field, type))
        .def_readwrite("size", &Struct::Field::size, D(Struct, Field, size))
        .def_readwrite("offset", &Struct::Field::offset, D(Struct, Field, offset))
        .def_readwrite("flags", &Struct::Field::flags, D(Struct, Field, flags))
        .def_readwrite("name", &Struct::Field::name, D(Struct, Field, name))
        .def_readwrite("blend", &Struct::Field::blend, D(Struct, Field, blend));

    MTS_PY_CLASS(StructConverter, Object)
        .def(py::init<const Struct *, const Struct *, bool>(), "source"_a, "target"_a, "dither"_a = false)
        .def_method(StructConverter, source)
        .def_method(StructConverter, target)
        .def("convert", [](const StructConverter &c, py::bytes input_) -> py::bytes {
            std::string input(input_);
            size_t count = input.length() / c.source()->size();
            std::string result(c.target()->size() * count, '\0');
            if (!c.convert(count, input.data(), (void *) result.data()))
            throw std::runtime_error("Conversion failed!");
            return result;
        });
}
