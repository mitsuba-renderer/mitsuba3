#include "python.h"

#include <mitsuba/core/properties.h>

#define SET_ITEM_BINDING(Type, PyType)                                     \
    def("__setitem__", [](Properties& p,                                   \
                          const py::str &key, const PyType &value){        \
        p.set##Type(key, value);                                           \
    }, DM(Properties, set##Type))

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
        // Getters & setters: used as if it were a simple map
        // TODO: support all types and call the correct functions
        // TODO: how about updates with different types?
        // TODO: how to allow disabling warnings about setting the same prop twice?
        // TODO: how to get with default?
       .SET_ITEM_BINDING(Boolean, py::bool_)
       .SET_ITEM_BINDING(Long, py::int_)
       .SET_ITEM_BINDING(Float, py::float_)
       .SET_ITEM_BINDING(String, py::str)

       .def("__getitem__", [](const Properties& p, const py::str &key) {
            // We need to ask for type information to return the right cast
            // auto type_info = props.getPropertyType(key);

            // switch(type_info) {
            //     case Properties::EBoolean:
            //       return py::cast(props.getBoolean(key))
            //     default:
            //       throw std::runtime_error("Unsupported property type");
            //       return;
            // }
            return "TODO";
       }, "Get back an existing property, in the fashion of a dict."
          "Type is preserved.")

        // Operators
        // TODO: will python's assignment operator behave nicely?
        .def(py::self == py::self, DM(Properties, operator_eq))
        .def(py::self != py::self, DM(Properties, operator_ne))
        .def("__repr__", [](const Properties &p) {
            std::ostringstream oss;
            oss << p;
            return oss.str();
        });
}
