#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>

using Caster = py::object(*)(mitsuba::Object *);
extern Caster cast_object;

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wduplicate-decl-specifier" // warning: duplicate 'const' declaration specifier
#endif

#define SET_ITEM_BINDING(Name, Type)                                   \
    def("__setitem__", [](Properties& p,                               \
                          const std::string &key, const Type &value) { \
        p.set_##Name(key, value, false);                               \
    }, D(Properties, set_##Name))

MTS_PY_EXPORT(Properties) {
    MTS_PY_CHECK_ALIAS(Properties, "Properties") {
        py::class_<Properties>(m, "Properties", D(Properties))
            // Constructors
            .def(py::init<>(), D(Properties, Properties))
            .def(py::init<const std::string &>(), D(Properties, Properties, 2))
            .def(py::init<const Properties &>(), D(Properties, Properties, 3))
            // Methods
            .def_method(Properties, has_property)
            .def_method(Properties, remove_property)
            .def_method(Properties, mark_queried)
            .def_method(Properties, was_queried)
            .def_method(Properties, plugin_name)
            .def_method(Properties, set_plugin_name)
            .def_method(Properties, id)
            .def_method(Properties, set_id)
            .def_method(Properties, copy_attribute)
            .def_method(Properties, property_names)
            .def_method(Properties, unqueried)
            .def_method(Properties, merge)
            // Getters & setters: used as if it were a simple map
            .SET_ITEM_BINDING(float, py::float_)
            .SET_ITEM_BINDING(bool, bool)
            .SET_ITEM_BINDING(long, int64_t)
            .SET_ITEM_BINDING(string, std::string)
            .SET_ITEM_BINDING(array3f, typename Properties::Array3f)
            .SET_ITEM_BINDING(transform, typename Properties::Transform4f)
            .SET_ITEM_BINDING(animated_transform, ref<AnimatedTransform>)
            .SET_ITEM_BINDING(object, ref<Object>)
            .def("__getitem__", [](const Properties& p, const std::string &key) {
                    // We need to ask for type information to return the right cast
                    auto type = p.type(key);
                    if (type == Properties::Type::Bool)
                        return py::cast(p.bool_(key));
                    else if (type == Properties::Type::Long)
                        return py::cast(p.long_(key));
                    else if (type == Properties::Type::Float)
                        return py::cast(p.float_(key));
                    else if (type == Properties::Type::String)
                        return py::cast(p.string(key));
                     else if (type == Properties::Type::Array3f)
                        return py::cast(p.array3f(key));
                    else if (type == Properties::Type::Transform)
                        return py::cast(p.transform(key));
                    else if (type == Properties::Type::AnimatedTransform)
                        return py::cast(p.animated_transform(key));
                    else if (type == Properties::Type::Object) {
                        return cast_object((ref<Object>)p.object(key));
                    } else if (type == Properties::Type::Pointer)
                        return py::cast(p.pointer(key));
                    else {
                        throw std::runtime_error("Unsupported property type");
                    }
            }, "Retrieve an existing property given its name")
            .def("__contains__", [](const Properties& p, const std::string &key) {
                return p.has_property(key);
            })
            .def("__delitem__", [](Properties& p, const std::string &key) {
                return p.remove_property(key);
            })
            // Operators
            .def(py::self == py::self, D(Properties, operator_eq))
            .def(py::self != py::self, D(Properties, operator_ne))
            .def_repr(Properties);
    }
}
