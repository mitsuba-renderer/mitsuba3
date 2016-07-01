#include <mitsuba/core/structconv.h>
#include "python.h"

MTS_PY_EXPORT(structconv) {
    auto c = MTS_PY_CLASS(Struct, Object)
        .def(py::init<bool>(), py::arg("pack") = false, DM(Struct, Struct))
        .def("append", (void (Struct::*)(const std::string&, Struct::Type)) &Struct::append, DM(Struct, append))
        .def("__getitem__", [](const Struct & s, size_t i) { if (i >= s.fieldCount()) throw py::index_error(); return s[i]; })
        .def("__len__", &Struct::fieldCount)
        .mdef(Struct, size)
        .mdef(Struct, alignment)
        .mdef(Struct, fieldCount);

    py::enum_<Struct::Type>(c, "Type")
        .value("EInt8",  Struct::Type::EInt8)
        .value("EUInt8", Struct::Type::EUInt8)
        .value("EInt16",  Struct::Type::EInt16)
        .value("EUInt16", Struct::Type::EUInt16)
        .value("EInt32",  Struct::Type::EInt32)
        .value("EUInt32", Struct::Type::EUInt32)
        .value("EFloat16", Struct::Type::EFloat16)
        .value("EFloat32", Struct::Type::EFloat32)
        .value("EFloat64", Struct::Type::EFloat64)
        .export_values();

    py::class_<Struct::Field>(c, "Field", DM(Struct, Field))
        .def_readwrite("name", &Struct::Field::name)
        .def_readwrite("type", &Struct::Field::type)
        .def_readwrite("size", &Struct::Field::size)
        .def_readwrite("offset", &Struct::Field::offset);
}
