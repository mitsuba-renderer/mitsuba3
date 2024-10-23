#include <mitsuba/core/logger.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

extern nb::object cast_object(Object *o);

MI_PY_EXPORT(Object) {
    auto e = nb::enum_<ParamFlags>(m, "ParamFlags", nb::is_arithmetic(), D(ParamFlags))
        .def_value(ParamFlags, Differentiable)
        .def_value(ParamFlags, NonDifferentiable)
        .def_value(ParamFlags, Discontinuous);

    nb::class_<Class>(m, "Class", D(Class))
        .def_method(Class, name)
        .def_method(Class, variant)
        .def_method(Class, alias)
        .def_method(Class, parent, nb::rv_policy::copy);

    nb::class_<PluginManager>(
        m, "PluginManager", D(PluginManager))
        .def_static_method(PluginManager, instance, nb::rv_policy::reference)
        .def("get_plugin_class",
             [](PluginManager &pmgr, const std::string &name,
                const std::string &variant) {
                 try {
                     return pmgr.get_plugin_class(name, variant);
                 } catch (std::runtime_error &) {
                     return static_cast<const Class *>(nullptr);
                 }
             },
             "name"_a, "variant"_a, nb::rv_policy::copy,
             D(PluginManager, get_plugin_class))
        .def("create_object", [](PluginManager &pmgr, const Properties &props) {
            auto mi = nb::module_::import_("mitsuba");
            std::string variant = nb::cast<std::string>(mi.attr("variant")());
            const Class *class_ = pmgr.get_plugin_class(props.plugin_name(), variant);
            return cast_object(pmgr.create_object(props, class_));
        }, D(PluginManager, create_object));

    nb::class_<Object>(
            m, "Object",
            nb::intrusive_ptr<Object>(
                [](Object *o, PyObject *po) noexcept { o->set_self_py(po); }),
            D(Object)
        )
        .def(nb::init<>(), D(Object, Object))
        .def(nb::init<const Object &>(), D(Object, Object, 2))
        .def_method(Object, id)
        .def_method(Object, set_id, "id"_a)
        .def("expand", [](const Object &o) -> nb::list {
            auto result = o.expand();
            nb::list l;
            for (Object *o2: result)
                l.append(cast_object(o2));
            return l;
        }, D(Object, expand))
        .def_method(Object, traverse, "cb"_a)
        .def_method(Object, parameters_changed, "keys"_a = nb::list())
        .def_prop_ro("ptr", [](Object *self) { return (uintptr_t) self; })
        .def("class_", &Object::class_, nb::rv_policy::copy, D(Object, class))
        .def("__repr__", &Object::to_string, D(Object, to_string));
}
