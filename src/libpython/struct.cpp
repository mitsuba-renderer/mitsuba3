#include <mitsuba/core/struct.h>
#include "python.h"

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
        .export_values();

    py::enum_<Struct::EByteOrder>(c, "EByteOrder")
        .value("ELittleEndian",  Struct::EByteOrder::ELittleEndian)
        .value("EBigEndian",  Struct::EByteOrder::EBigEndian)
        .export_values();

    py::enum_<Struct::EFlags>(c, "EFlags")
        .value("ENormalized",  Struct::EFlags::ENormalized)
        .value("EGamma",  Struct::EFlags::EGamma)
        .export_values();

    c.def(py::init<bool, Struct::EByteOrder>(), py::arg("pack") = false,
             py::arg("byteOrder") = Struct::EByteOrder::ELittleEndian,
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
        .mdef(Struct, fieldCount);

    py::class_<Struct::Field>(c, "Field", DM(Struct, Field))
        .def("isFloat", &Struct::Field::isFloat, DM(Struct, Field, isFloat))
        .def("isInteger", &Struct::Field::isInteger, DM(Struct, Field, isInteger))
        .def("isSigned", &Struct::Field::isSigned, DM(Struct, Field, isSigned))
        .def("isUnsigned", &Struct::Field::isUnsigned, DM(Struct, Field, isUnsigned))
        .def("getRange", &Struct::Field::getRange, DM(Struct, Field, getRange))
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
            c.convert(count, input.data(), (void *) result.data());
            return result;
        });
}
