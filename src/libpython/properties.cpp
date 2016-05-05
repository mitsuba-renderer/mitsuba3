#include <mitsuba/core/properties.h>
#include <mitsuba/core/vector.h>
#include "python.h"

#define SET_ITEM_BINDING(Name, Type)                                   \
    def("__setitem__", [](Properties& p,                               \
                          const std::string &key, const Type &value) { \
        p.set##Name(key, value, false);                                \
    }, DM(Properties, set##Name))

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
        .mdef(Properties, getPropertyNames)
        .mdef(Properties, getUnqueried)
        .mdef(Properties, merge)

        // Getters & setters: used as if it were a simple map
       .SET_ITEM_BINDING(Float, py::float_)
       .SET_ITEM_BINDING(Boolean, bool)
       .SET_ITEM_BINDING(Long, int64_t)
       .SET_ITEM_BINDING(String, std::string)
       .SET_ITEM_BINDING(Vector3f, Vector3f)
       .SET_ITEM_BINDING(Object, ref<Object>)

       .def("__getitem__", [](const Properties& p, const std::string &key) {
            // We need to ask for type information to return the right cast
            auto type = p.getPropertyType(key);

            if (type == Properties::EBoolean)
                return py::cast(p.getBoolean(key));
            else if (type == Properties::EInteger)
                return py::cast(p.getLong(key));
            else if (type == Properties::EFloat)
                return py::cast(p.getFloat(key));
            else if (type == Properties::EString)
                return py::cast(p.getString(key));
            else if (type == Properties::EVector3f)
                return py::cast(p.getVector3f(key));
            else if (type == Properties::EObject)
                return py::cast(p.getObject(key));
            else {
                throw std::runtime_error("Unsupported property type");
            }
       }, "Retrieve an existing property given its name")

        // Operators
        .def(py::self == py::self, DM(Properties, operator_eq))
        .def(py::self != py::self, DM(Properties, operator_ne))
        .def("__repr__", [](const Properties &p) {
            std::ostringstream oss;
            oss << p;
            return oss.str();
        });
}
