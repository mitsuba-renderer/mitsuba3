#include <mitsuba/python/python.h>
#include <mitsuba/core/transform.h>

extern py::object py_cast(const std::type_info &type, void *ptr);
extern py::object py_cast_object(Object *o);

/* Trampoline for derived types implemented in Python */
class PyTraversalCallback : public TraversalCallback {
public:
    using TraversalCallback::TraversalCallback;

    void put_parameter_impl(const std::string &name, const std::type_info &type,
                            void *ptr) override {
        py::gil_scoped_acquire gil;
        // Try to look up the overloaded method on the Python side.
        py::function overload = py::get_overload(this, "put_parameter");

        if (overload)
            overload(name, py_cast(type, ptr));
        else
            Throw("TraversalCallback doesn't overload the method \"put_parameter\"");
    }

    void put_object(const std::string &name, Object *obj) override {
        py::gil_scoped_acquire gil;
        // Try to look up the overloaded method on the Python side.
        py::function overload = py::get_overload(this, "put_object");

        if (overload)
            overload(name, py_cast_object(obj));
        else
            Throw("TraversalCallback doesn't overload the method \"put_object\"");
    }
};

MTS_PY_EXPORT(Object) {
    MTS_PY_CHECK_ALIAS(Class, m) {
        py::class_<Class>(m, "Class", D(Class));
    }

    MTS_PY_CHECK_ALIAS(TraversalCallback, m) {
        py::class_<TraversalCallback, PyTraversalCallback>(m, "TraversalCallback")
            .def(py::init<>());
    }

    MTS_PY_CHECK_ALIAS(Object, m) {
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
                    l.append(py_cast_object(o2));
                return l;
            })
            .def_method(Object, traverse)
            .def_method(Object, parameters_changed)
            .def("__repr__", &Object::to_string, D(Object, to_string));
    }
}
