#include <mitsuba/core/struct.h>
#include <mitsuba/core/logger.h>
#include "python.h"

/// Conversion between 'Struct' and NumPy 'dtype' data structures
py::dtype dtypeForStruct(const Struct *s) {
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
        .export_values();

    py::enum_<Struct::EByteOrder>(c, "EByteOrder")
        .value("ELittleEndian",  Struct::EByteOrder::ELittleEndian)
        .value("EBigEndian",  Struct::EByteOrder::EBigEndian)
        .value("EHostByteOrder",  Struct::EByteOrder::EHostByteOrder)
        .export_values();

    py::enum_<Struct::EFlags>(c, "EFlags")
        .value("ENormalized", Struct::EFlags::ENormalized)
        .value("EGamma", Struct::EFlags::EGamma)
        .value("EAssert", Struct::EFlags::EAssert)
        .value("EDefault", Struct::EFlags::EDefault)
        .export_values();

    c.def(py::init<bool, Struct::EByteOrder>(), py::arg("pack") = false,
             py::arg("byteOrder") = Struct::EByteOrder::EHostByteOrder,
             DM(Struct, Struct))
        .def("append", (Struct &(Struct::*)(const std::string&, Struct::EType, uint32_t, double)) &Struct::append,
             py::arg("name"), py::arg("type"), py::arg("flags") = 0, py::arg("default") = 0.0,
             DM(Struct, append), py::return_value_policy::reference)
        .def("__getitem__", [](const Struct & s, size_t i) {
            if (i >= s.fieldCount())
                throw py::index_error();
            return s[i];
        })
        .def("__len__", &Struct::fieldCount)
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__hash__", [](const Struct &s) { return hash(s); })
        .mdef(Struct, size)
        .mdef(Struct, alignment)
        .mdef(Struct, byteOrder)
        .mdef(Struct, fieldCount)
        .mdef(Struct, hasField)
        .def("dtype", &dtypeForStruct, "Return a NumPy dtype corresponding to this data structure");

    py::class_<Struct::Field>(c, "Field", DM(Struct, Field))
        .def("isFloat", &Struct::Field::isFloat, DM(Struct, Field, isFloat))
        .def("isInteger", &Struct::Field::isInteger, DM(Struct, Field, isInteger))
        .def("isSigned", &Struct::Field::isSigned, DM(Struct, Field, isSigned))
        .def("isUnsigned", &Struct::Field::isUnsigned, DM(Struct, Field, isUnsigned))
        .def("range", &Struct::Field::range, DM(Struct, Field, range))
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__hash__", [](const Struct::Field &f) { return hash(f); })
        .def_readonly("name", &Struct::Field::name, DM(Struct, Field, name))
        .def_readonly("type", &Struct::Field::type, DM(Struct, Field, type))
        .def_readonly("size", &Struct::Field::size, DM(Struct, Field, size))
        .def_readonly("offset", &Struct::Field::offset, DM(Struct, Field, offset))
        .def_readonly("flags", &Struct::Field::flags, DM(Struct, Field, flags));

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
