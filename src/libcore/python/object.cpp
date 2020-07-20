#include <mitsuba/core/logger.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/python/python.h>

extern py::object cast_object(Object *o);

// Trampoline for derived types implemented in Python
class PyTraversalCallback : public TraversalCallback {
public:
    void put_parameter_impl(const std::string &name,
                            const std::type_info &type,
                            void *ptr) override {
        py::gil_scoped_acquire gil;
        py::function overload = py::get_overload(this, "put_parameter");

        if (overload)
            overload(name, (void *) &type, ptr);
        else
            Throw("TraversalCallback doesn't overload the method \"put_parameter\"");
    }

    void put_object(const std::string &name, Object *obj) override {
        py::gil_scoped_acquire gil;
        py::function overload = py::get_overload(this, "put_object");

        if (overload)
            overload(name, cast_object(obj));
        else
            Throw("TraversalCallback doesn't overload the method \"put_object\"");
    }
};

MTS_PY_EXPORT(Object) {
    py::class_<Class>(m, "Class", D(Class))
        .def_method(Class, name)
        .def_method(Class, variant)
        .def_method(Class, alias)
        .def_method(Class, parent, py::return_value_policy::reference);

    py::class_<PluginManager, std::unique_ptr<PluginManager, py::nodelete>>(m, "PluginManager", D(PluginManager))
        .def_static_method(PluginManager, instance, py::return_value_policy::reference)
        .def("get_plugin_class", [](PluginManager &pmgr, const std::string &name, const std::string &variant){
            try {
                return pmgr.get_plugin_class(name, variant);
            } catch(std::runtime_error &e){
                return static_cast<const Class *>(nullptr);
            }
        }, "name"_a, "variant"_a, py::return_value_policy::reference, D(PluginManager, get_plugin_class));

    py::class_<TraversalCallback, PyTraversalCallback>(m, "TraversalCallback")
        .def(py::init<>());

    py::class_<Object, ref<Object>>(m, "Object", D(Object))
        .def(py::init<>(), D(Object, Object))
        .def(py::init<const Object &>(), D(Object, Object, 2))
        .def_method(Object, id)
        .def_method(Object, ref_count)
        .def_method(Object, inc_ref)
        .def_method(Object, dec_ref, "dealloc"_a = true)
        .def("expand", [](const Object &o) -> py::list {
            auto result = o.expand();
            py::list l;
            for (Object *o2: result)
                l.append(cast_object(o2));
            return l;
        }, D(Object, expand))
        .def_method(Object, traverse, "cb"_a)
        .def_method(Object, parameters_changed, "keys"_a = py::list())
        .def_property_readonly("ptr", [](Object *self) { return (uintptr_t) self; })
        .def("class_", &Object::class_, py::return_value_policy::reference, D(Object, class))
        .def("__repr__", &Object::to_string, D(Object, to_string));
}
