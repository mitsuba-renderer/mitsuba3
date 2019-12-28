#include <mitsuba/python/python.h>
#include <mitsuba/core/logger.h>

using Caster = py::object(*)(mitsuba::Object *, py::handle parent);

static std::vector<void *> casters;

py::object cast_object(Object *o, py::handle parent) {
    for (auto caster : casters) {
        py::object po = ((Caster) caster)(o, parent);
        if (po)
            return po;
    }

    py::return_value_policy rvp = parent ? py::return_value_policy::reference_internal
                                         : py::return_value_policy::take_ownership;

    return py::cast(o, rvp, parent);
}

// Trampoline for derived types implemented in Python
class PyTraversalCallback : public TraversalCallback {
public:
    PyTraversalCallback(py::object object) : m_object(object) { }

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
            overload(name, cast_object(obj, m_object));
        else
            Throw("TraversalCallback doesn't overload the method \"put_object\"");
    }

private:
    py::object m_object;
};

MTS_PY_EXPORT(Object) {
    py::class_<Class>(m, "Class", D(Class));

    py::class_<TraversalCallback, PyTraversalCallback>(m, "TraversalCallback")
        .def(py::init<py::object>());

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
                l.append(cast_object(o2, py::handle()));
            return l;
        }, D(Object, expand))
        .def_method(Object, traverse, "cb"_a)
        .def_method(Object, parameters_changed)
        .def_property_readonly("ptr", [](Object *self){ return (uintptr_t) self; })
        .def("class_", &Object::class_, py::return_value_policy::reference, D(Object, class))
        .def("__repr__", &Object::to_string, D(Object, to_string));

    m.attr("casters") = py::cast((void *) &casters);
}
