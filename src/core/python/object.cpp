#include <mitsuba/core/logger.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>

extern nb::object cast_object(Object *o);

MI_PY_EXPORT(Object) {
    auto e = nb::enum_<ParamFlags>(m, "ParamFlags", nb::is_arithmetic(), D(ParamFlags))
        .def_value(ParamFlags, Differentiable)
        .def_value(ParamFlags, NonDifferentiable)
        .def_value(ParamFlags, Discontinuous)
        .def_value(ParamFlags, ReadOnly);

    nb::enum_<ObjectType>(m, "ObjectType", D(ObjectType))
        .def_value(ObjectType, Unknown)
        .def_value(ObjectType, Scene)
        .def_value(ObjectType, Sensor)
        .def_value(ObjectType, Film)
        .def_value(ObjectType, Emitter)
        .def_value(ObjectType, Sampler)
        .def_value(ObjectType, Shape)
        .def_value(ObjectType, Texture)
        .def_value(ObjectType, Volume)
        .def_value(ObjectType, Medium)
        .def_value(ObjectType, BSDF)
        .def_value(ObjectType, Integrator)
        .def_value(ObjectType, PhaseFunction)
        .def_value(ObjectType, ReconstructionFilter);

    nb::class_<PluginManager>(
        m, "PluginManager", D(PluginManager))
        .def_static_method(PluginManager, instance, nb::rv_policy::reference)
        .def("create_object", [](PluginManager &pmgr, const Properties &props) {
            auto mi = nb::module_::import_("mitsuba");
            std::string variant = nb::cast<std::string>(mi.attr("variant")());
            return cast_object(pmgr.create_object(props, variant, ObjectType::Unknown).get());
        }, "props"_a, D(PluginManager, create_object))
        .def("plugin_type", &PluginManager::plugin_type, "name"_a,
             "Get the ObjectType of a plugin by name");

    nb::class_<Object, drjit::TraversableBase>(
        m, "Object",
        nb::intrusive_ptr<Object>([](Object *o, PyObject *po) noexcept {
            PyObject *other = o->self_py();
            if (!other) {
                o->set_self_py(po);
            } else {
                nb::detail::keep_alive(po, other);
                nb::detail::nb_inst_set_state(po, true, false);
            }
        }),
        D(Object))
        .def(nb::init<>(), D(Object, Object))
        .def(nb::init<const Object &>(), D(Object, Object, 2))
        .def_method(Object, id)
        .def_method(Object, set_id, "id"_a)
        .def_method(Object, class_name)
        .def("variant_name", &Object::variant_name, D(Object, variant_name))
        .def("expand", [=](const Object &o) -> nb::list {
            auto result = o.expand();
            nb::list l;
            for (auto o2: result)
                l.append(cast_object(o2.get()));
            return l;
        }, D(Object, expand))
        .def_method(Object, traverse, "cb"_a)
        .def_method(Object, parameters_changed, "keys"_a = nb::list())
        .def_prop_ro("ptr", [](Object *self) { return (uintptr_t) self; })
        .def("__repr__", &Object::to_string, D(Object, to_string));
}
