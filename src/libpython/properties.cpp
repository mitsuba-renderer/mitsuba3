#include "python.h"

#include <mitsuba/core/properties.h>

MTS_PY_EXPORT(Properties) {
    py::class_<Properties>(m, "Properties", DM(Properties))
        // Constructors
        .def(py::init<>(), DM(Properties, Properties))
        .def(py::init<const std::string &>(), DM(Properties, Properties, 2))
        .def(py::init<const Properties &>(), DM(Properties, Properties, 3))
        // Methods
        .mdef(Properties, hasProperty)
        .mdef(Properties, removeProperty)
        .mdef(Properties, markQueried)
        .mdef(Properties, wasQueried)
        .mdef(Properties, getPluginName)
        .mdef(Properties, setPluginName)
        .mdef(Properties, getID)
        .mdef(Properties, setID)
        .mdef(Properties, copyAttribute)
        .mdef(Properties, putPropertyNames)
        .mdef(Properties, getPropertyNames)
        .mdef(Properties, getUnqueried)
        .mdef(Properties, merge)
        // Getters & setters (in python, should be used simply as a map)
        // TODO: how to maintain consistency between the C++ map and the python object's properties?
//        .def("__setitem__", [](Properties& p,
//                               const py::str &n, const std::string &v){
//            p.setString(n, v);
//        }, "Set a property, in the fashion of a dict.")
//        .def("__getitem__", [](const Properties& p, const py::str &n) {
//            return py::cast(p.getString(n));
//        }, "Get back a set property, in the fashion of a dict.")
        // Operators
        // TODO: will python's assignment operator be valid?
        .def(py::self == py::self, DM(Properties, operator_eq))
        .def(py::self != py::self, DM(Properties, operator_ne))
        .def("__repr__", [](const Properties &p) {
            std::ostringstream oss;
            oss << p;
            return oss.str();
        });
}
