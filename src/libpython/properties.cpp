#include <mitsuba/core/properties.h>
#include <mitsuba/core/vector.h>
#include "python.h"

#define SET_ITEM_BINDING(Name, Type)                                   \
    def("__setitem__", [](Properties& p,                               \
                          const std::string &key, const Type &value) { \
        p.set_##Name(key, value, false);                               \
    }, D(Properties, set_##Name))

MTS_PY_EXPORT(Properties) {
    py::class_<Properties>(m, "Properties", D(Properties))
        // Constructors
        .def(py::init<>(), D(Properties, Properties))
        .def(py::init<const std::string &>(), D(Properties, Properties, 2))
        .def(py::init<const Properties &>(), D(Properties, Properties, 3))

        // Methods
        .mdef(Properties, has_property)
        .mdef(Properties, remove_property)
        .mdef(Properties, mark_queried)
        .mdef(Properties, was_queried)
        .mdef(Properties, plugin_name)
        .mdef(Properties, set_plugin_name)
        .mdef(Properties, id)
        .mdef(Properties, set_id)
        .mdef(Properties, copy_attribute)
        .mdef(Properties, property_names)
        .mdef(Properties, unqueried)
        .mdef(Properties, merge)

        // Getters & setters: used as if it were a simple map
       .SET_ITEM_BINDING(float, py::float_)
       .SET_ITEM_BINDING(bool, bool)
       .SET_ITEM_BINDING(long, int64_t)
       .SET_ITEM_BINDING(string, std::string)
       .SET_ITEM_BINDING(vector3f, Vector3f)
       .SET_ITEM_BINDING(object, ref<Object>)

       .def("__getitem__", [](const Properties& p, const std::string &key) {
            // We need to ask for type information to return the right cast
            auto type = p.property_type(key);

            if (type == Properties::EBool)
                return py::cast(p.bool_(key));
            else if (type == Properties::ELong)
                return py::cast(p.long_(key));
            else if (type == Properties::EFloat)
                return py::cast(p.float_(key));
            else if (type == Properties::EString)
                return py::cast(p.string(key));
            else if (type == Properties::EVector3f)
                return py::cast(p.vector3f(key));
            else if (type == Properties::EPoint3f)
                return py::cast(p.point3f(key));
            else if (type == Properties::EObject)
                return py::cast(p.object(key));
            else {
                throw std::runtime_error("Unsupported property type");
            }
       }, "Retrieve an existing property given its name")
       .def("__contains__", [](const Properties& p, const std::string &key) {
               std::cout << "Checking " << key << std::endl;
           return p.has_property(key);
       })
       .def("__delitem__", [](Properties& p, const std::string &key) {
           return p.remove_property(key);
       })

        // Operators
        .def(py::self == py::self, D(Properties, operator_eq))
        .def(py::self != py::self, D(Properties, operator_ne))
        .def("__repr__", [](const Properties &p) {
            std::ostringstream oss;
            oss << p;
            return oss.str();
        });
}
