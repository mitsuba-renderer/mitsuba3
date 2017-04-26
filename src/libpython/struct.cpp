#include <mitsuba/core/struct.h>
#include <mitsuba/core/logger.h>
#include "python.h"

/// Conversion between 'Struct' and NumPy 'dtype' data structures
py::dtype dtype_for_struct(const Struct *s) {
    py::list names, offsets, formats;

    for (auto field: *s) {
        names.append(py::str(field.name));
        offsets.append(py::int_(field.offset));
        std::string format;
        switch (field.type) {
            case Struct::EInt8:    format = "int8";   break;
            case Struct::EUInt8:   format = "uint8";  break;
            case Struct::EInt16:   format = "int16";  break;
            case Struct::EUInt16:  format = "uint16"; break;
            case Struct::EInt32:   format = "int32";  break;
            case Struct::EUInt32:  format = "uint32"; break;
            case Struct::EInt64:   format = "int64";  break;
            case Struct::EUInt64:  format = "uint64"; break;
            case Struct::EFloat16: format = "float16";  break;
            case Struct::EFloat32: format = "float32";  break;
            case Struct::EFloat64: format = "float64";  break;
            case Struct::EFloat:   format = "float"
                + std::to_string(sizeof(Float) * 8);  break;
            case Struct::EInvalid: format = "invalid";  break;
            default: Throw("Internal error.");
        }
        formats.append(py::str(format));
    }
    return py::dtype(names, formats, offsets, s->size());
}

MTS_PY_EXPORT(Struct) {
    auto c = MTS_PY_CLASS(Struct, Object);

    py::enum_<Struct::EType>(c, "EType")
        .value("EInt8",  Struct::EType::EInt8)
        .value("EUInt8", Struct::EType::EUInt8)
        .value("EInt16",  Struct::EType::EInt16)
        .value("EUInt16", Struct::EType::EUInt16)
        .value("EInt32",  Struct::EType::EInt32)
        .value("EUInt32", Struct::EType::EUInt32)
        .value("EInt64",  Struct::EType::EInt64)
        .value("EUInt64", Struct::EType::EUInt64)
        .value("EFloat16", Struct::EType::EFloat16)
        .value("EFloat32", Struct::EType::EFloat32)
        .value("EFloat64", Struct::EType::EFloat64)
        .value("EFloat", Struct::EType::EFloat)
        .value("EInvalid", Struct::EType::EInvalid)
        .export_values()
        .def("__init__", [](Struct::EType &et, py::dtype dt) {
            Struct::EType value = Struct::EInt8;
            if (dt.kind() == 'i') {
                switch (dt.itemsize()) {
                    case 1: value = Struct::EInt8; break;
                    case 2: value = Struct::EInt16; break;
                    case 4: value = Struct::EInt32; break;
                    case 8: value = Struct::EInt64; break;
                    default: throw py::type_error("Struct::EType(): Invalid integer type!");
                }
            } else if (dt.kind() == 'u') {
                switch (dt.itemsize()) {
                    case 1: value = Struct::EUInt8; break;
                    case 2: value = Struct::EUInt16; break;
                    case 4: value = Struct::EUInt32; break;
                    case 8: value = Struct::EUInt64; break;
                    default: throw py::type_error("Struct::EType(): Invalid unsigned integer type!");
                }
            } else if (dt.kind() == 'f') {
                switch (dt.itemsize()) {
                    case 2: value = Struct::EFloat16; break;
                    case 4: value = Struct::EFloat32; break;
                    case 8: value = Struct::EFloat64; break;
                    default: throw py::type_error("Struct::EType(): Invalid floating point type!");
                }
            } else {
                throw py::type_error("Struct::EType(): Invalid type!");
            }
            new (&et) Struct::EType(value);
        });

    py::implicitly_convertible<py::dtype, Struct::EType>();

    py::enum_<Struct::EByteOrder>(c, "EByteOrder")
        .value("ELittleEndian",  Struct::EByteOrder::ELittleEndian)
        .value("EBigEndian",  Struct::EByteOrder::EBigEndian)
        .value("EHostByteOrder",  Struct::EByteOrder::EHostByteOrder)
        .export_values();

    py::enum_<Struct::EFlags>(c, "EFlags", py::arithmetic())
        .value("ENormalized", Struct::EFlags::ENormalized)
        .value("EGamma", Struct::EFlags::EGamma)
        .value("EAssert", Struct::EFlags::EAssert)
        .value("EDefault", Struct::EFlags::EDefault)
        .export_values();

    c.def(py::init<bool, Struct::EByteOrder>(), "pack"_a = false,
             "byte_order"_a = Struct::EByteOrder::EHostByteOrder,
             D(Struct, Struct))
        .def("append", (Struct &(Struct::*)(const std::string&, Struct::EType, uint32_t, double)) &Struct::append,
             "name"_a, "type"_a, "flags"_a = 0, "default"_a = 0.0,
             D(Struct, append), py::return_value_policy::reference)
        .def("field", py::overload_cast<const std::string &>(&Struct::field), D(Struct, field),
             py::return_value_policy::reference_internal)
        .def("__getitem__", [](Struct & s, size_t i) -> Struct::Field& {
            if (i >= s.field_count())
                throw py::index_error();
            return s[i];
        }, py::return_value_policy::reference_internal)
        .def("__len__", &Struct::field_count)
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__hash__", [](const Struct &s) { return hash(s); })
        .mdef(Struct, size)
        .mdef(Struct, alignment)
        .mdef(Struct, byte_order)
        .mdef(Struct, field_count)
        .mdef(Struct, has_field)
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
        .def_readonly("name", &Struct::Field::name, D(Struct, Field, name))
        .def_readonly("type", &Struct::Field::type, D(Struct, Field, type))
        .def_readonly("size", &Struct::Field::size, D(Struct, Field, size))
        .def_readonly("offset", &Struct::Field::offset, D(Struct, Field, offset))
        .def_readonly("flags", &Struct::Field::flags, D(Struct, Field, flags))
        .def_readwrite("blend", &Struct::Field::blend, D(Struct, Field, blend));

    MTS_PY_CLASS(StructConverter, Object)
        .def(py::init<const Struct *, const Struct *>())
        .mdef(StructConverter, source)
        .mdef(StructConverter, target)
        .def("convert", [](const StructConverter &c, py::bytes input_) -> py::bytes {
            std::string input(input_);
            size_t count = input.length() / c.source()->size();
            std::string result(c.target()->size() * count, '\0');
            if (!c.convert(count, input.data(), (void *) result.data()))
               throw std::runtime_error("Conversion failed!");
            return result;
        });
}
