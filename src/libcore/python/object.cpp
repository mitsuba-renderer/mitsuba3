#include <mitsuba/python/python.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/frame.h>

extern py::object py_cast(const std::type_info &type, void *ptr);
extern py::object py_cast_object(Object *o);

/* Trampoline for derived types implemented in Python */
class PyTraversalCallback : public TraversalCallback {
public:
    using TraversalCallback::TraversalCallback;

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
            overload(name, py_cast_object(obj));
        else
            Throw("TraversalCallback doesn't overload the method \"put_object\"");
    }
};

MTS_PY_EXPORT(Object) {
    MTS_IMPORT_CORE_TYPES()
    MTS_PY_CHECK_ALIAS(Class, m) {
        py::class_<Class>(m, "Class", D(Class));
    }

    MTS_PY_CHECK_ALIAS(TraversalCallback, m) {
        py::class_<TraversalCallback, PyTraversalCallback>(m, "TraversalCallback")
            .def(py::init<>());
    }

    #define GET_ATTR(T)                                                  \
        if (type == typeid(T))                                           \
        return py::cast((T *) ptr, py::return_value_policy::reference)

    #define SET_ATTR(T)                                                  \
        if (type == typeid(T)) {                                         \
            *((T *) ptr) = py::cast<T>(handle);                          \
            return;                                                      \
        }

    m.def("get_property", [](const void *ptr, void *type_) -> py::object {
        const std::type_info &type = *(const std::type_info *) type_;
        GET_ATTR(DynamicBuffer<Float>);
        GET_ATTR(DynamicBuffer<Int32>);
        GET_ATTR(DynamicBuffer<UInt32>);
        GET_ATTR(Vector2i);
        GET_ATTR(Vector2u);
        GET_ATTR(Color3f);
        GET_ATTR(Point3f);
        GET_ATTR(Vector3f);
        GET_ATTR(Normal3f);
        GET_ATTR(Frame3f);
        GET_ATTR(Matrix3f);
        GET_ATTR(Matrix4f);
        GET_ATTR(Transform3f);
        GET_ATTR(Transform4f);
        GET_ATTR(Mask);
        GET_ATTR(AnimatedTransform);

        if constexpr (!std::is_same_v<Float, ScalarFloat>) {
            GET_ATTR(ScalarFloat);
            GET_ATTR(ScalarInt32);
            GET_ATTR(ScalarUInt32);
            GET_ATTR(ScalarVector2i);
            GET_ATTR(ScalarVector2u);
            GET_ATTR(ScalarColor3f);
            GET_ATTR(ScalarPoint3f);
            GET_ATTR(ScalarVector3f);
            GET_ATTR(ScalarNormal3f);
            GET_ATTR(ScalarFrame3f);
            GET_ATTR(ScalarMatrix3f);
            GET_ATTR(ScalarMatrix4f);
            GET_ATTR(ScalarTransform3f);
            GET_ATTR(ScalarTransform4f);
            GET_ATTR(ScalarMask);
        }

        std::string name(type.name());
        py::detail::clean_type_id(name);
        Throw("get_property(): unsupported type \"%s\"!", name);
    }, "cpp_type"_a, "ptr"_a);

    m.def("set_property", [](const void *ptr, void *type_, py::handle handle) {
        const std::type_info &type = *(const std::type_info *) type_;

        SET_ATTR(DynamicBuffer<Float>);
        SET_ATTR(DynamicBuffer<Int32>);
        SET_ATTR(DynamicBuffer<UInt32>);
        SET_ATTR(Vector2i);
        SET_ATTR(Vector2u);
        SET_ATTR(Color3f);
        SET_ATTR(Point3f);
        SET_ATTR(Vector3f);
        SET_ATTR(Normal3f);
        SET_ATTR(Frame3f);
        SET_ATTR(Matrix3f);
        SET_ATTR(Matrix4f);
        SET_ATTR(Transform3f);
        SET_ATTR(Transform4f);
        SET_ATTR(Mask);

        if constexpr (!std::is_same_v<Float, ScalarFloat>) {
            SET_ATTR(ScalarFloat);
            SET_ATTR(ScalarInt32);
            SET_ATTR(ScalarUInt32);
            SET_ATTR(ScalarVector2i);
            SET_ATTR(ScalarVector2u);
            SET_ATTR(ScalarColor3f);
            SET_ATTR(ScalarPoint3f);
            SET_ATTR(ScalarVector3f);
            SET_ATTR(ScalarNormal3f);
            SET_ATTR(ScalarFrame3f);
            SET_ATTR(ScalarMatrix3f);
            SET_ATTR(ScalarMatrix4f);
            SET_ATTR(ScalarTransform3f);
            SET_ATTR(ScalarTransform4f);
            SET_ATTR(ScalarMask);
        }

        std::string name(type.name());
        py::detail::clean_type_id(name);
        Throw("set_property(): unsupported type \"%s\"!", name);
    });

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
            .def_property_readonly("ptr", [](Object *self){ return (uintptr_t) self; })
            .def("__repr__", &Object::to_string, D(Object, to_string));
    }

    if constexpr (is_cuda_array_v<Float>)
        pybind11_type_alias<UInt64, replace_scalar_t<Float, const Object *>>();
}
