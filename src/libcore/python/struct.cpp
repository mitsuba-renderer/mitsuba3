#include <mitsuba/core/struct.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/python/python.h>

/// Conversion between 'Struct' and NumPy 'dtype' data structures
py::dtype dtype_for_struct(const Struct *s) {
    py::list names, offsets, formats;

    for (auto field: *s) {
        names.append(py::str(field.name));
        offsets.append(py::int_(field.offset));
        std::string format;
        switch (field.type) {
            case FieldType::Int8:    format = "int8";   break;
            case FieldType::UInt8:   format = "uint8";  break;
            case FieldType::Int16:   format = "int16";  break;
            case FieldType::UInt16:  format = "uint16"; break;
            case FieldType::Int32:   format = "int32";  break;
            case FieldType::UInt32:  format = "uint32"; break;
            case FieldType::Int64:   format = "int64";  break;
            case FieldType::UInt64:  format = "uint64"; break;
            case FieldType::Float16: format = "float16";  break;
            case FieldType::Float32: format = "float32";  break;
            case FieldType::Float64: format = "float64";  break;
            case FieldType::Invalid: format = "invalid";  break;
            default: Throw("Internal error.");
        }
        formats.append(py::str(format));
    }
    return py::dtype(names, formats, offsets, s->size());
}

MTS_PY_EXPORT(Struct) {
    MTS_PY_CHECK_ALIAS(FieldType, m) {
        py::enum_<FieldType>(m, "FieldType")
            .value("Int8",  FieldType::Int8, D(Struct, FieldType, Int8))
            .value("UInt8", FieldType::UInt8, D(Struct, FieldType, UInt8))
            .value("Int16",  FieldType::Int16, D(Struct, FieldType, Int16))
            .value("UInt16", FieldType::UInt16, D(Struct, FieldType, UInt16))
            .value("Int32",  FieldType::Int32, D(Struct, FieldType, Int32))
            .value("UInt32", FieldType::UInt32, D(Struct, FieldType, UInt32))
            .value("Int64",  FieldType::Int64, D(Struct, FieldType, Int64))
            .value("UInt64", FieldType::UInt64, D(Struct, FieldType, UInt64))
            .value("Float16", FieldType::Float16, D(Struct, FieldType, Float16))
            .value("Float32", FieldType::Float32, D(Struct, FieldType, Float32))
            .value("Float64", FieldType::Float64, D(Struct, FieldType, Float64))
            .value("Invalid", FieldType::Invalid, D(Struct, FieldType, Invalid))
            .export_values()
            .def(py::init([](py::dtype dt) {
                FieldType value = FieldType::Int8;
                if (dt.kind() == 'i') {
                    switch (dt.itemsize()) {
                        case 1: value = FieldType::Int8; break;
                        case 2: value = FieldType::Int16; break;
                        case 4: value = FieldType::Int32; break;
                        case 8: value = FieldType::Int64; break;
                        default: throw py::type_error("FieldType(): Invalid integer type!");
                    }
                } else if (dt.kind() == 'u') {
                    switch (dt.itemsize()) {
                        case 1: value = FieldType::UInt8; break;
                        case 2: value = FieldType::UInt16; break;
                        case 4: value = FieldType::UInt32; break;
                        case 8: value = FieldType::UInt64; break;
                        default: throw py::type_error("FieldType(): Invalid unsigned integer type!");
                    }
                } else if (dt.kind() == 'f') {
                    switch (dt.itemsize()) {
                        case 2: value = FieldType::Float16; break;
                        case 4: value = FieldType::Float32; break;
                        case 8: value = FieldType::Float64; break;
                        default: throw py::type_error("FieldType(): Invalid floating point type!");
                    }
                } else {
                    throw py::type_error("FieldType(): Invalid type!");
                }
                return new FieldType(value);
            }), "dtype"_a);
    }

    py::implicitly_convertible<py::dtype, FieldType>();

    MTS_PY_CHECK_ALIAS(FieldByteOrder, m) {
        py::enum_<FieldByteOrder>(m, "FieldByteOrder")
            .value("LittleEndian", FieldByteOrder::LittleEndian,
                D(Struct, FieldByteOrder, LittleEndian))
            .value("BigEndian", FieldByteOrder::BigEndian,
                D(Struct, FieldByteOrder, BigEndian))
            .value("HostByteOrder", FieldByteOrder::HostByteOrder,
                D(Struct, FieldByteOrder, HostByteOrder))
            .export_values();
    }

    MTS_PY_CHECK_ALIAS(FieldFlags, m) {
        py::enum_<FieldFlags>(m, "FieldFlags", py::arithmetic())
            .value("Normalized", FieldFlags::Normalized, D(Struct, FieldFlags, Normalized))
            .value("Gamma", FieldFlags::Gamma, D(Struct, FieldFlags, Gamma))
            .value("Weight", FieldFlags::Weight, D(Struct, FieldFlags, Weight))
            .value("Assert", FieldFlags::Assert, D(Struct, FieldFlags, Assert))
            .value("Default", FieldFlags::Default, D(Struct, FieldFlags, Default))
            .export_values();
    }

    MTS_PY_CHECK_ALIAS(Struct, m) {
        auto c = MTS_PY_CLASS(Struct, Object);

        c.def(py::init<bool, FieldByteOrder>(), "pack"_a = false,
                "byte_order"_a = FieldByteOrder::HostByteOrder,
                D(Struct, Struct))
            .def("append",
                (Struct &(Struct::*)(const std::string&, FieldType, FieldFlags, double)) &Struct::append,
                "name"_a, "type"_a, "flags"_a = 0, "default"_a = 0.0,
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

        py::class_<Struct::Field>(c, "Field", D(Struct, Field))
            .def("is_float", &Struct::Field::is_float, D(Struct, Field, is_float))
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
    }

    MTS_PY_CHECK_ALIAS(StructConverter, m) {
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
}
