#include <mitsuba/core/transform.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/python/python.h>
#include <drjit/dynamic.h>
#include <drjit/tensor.h>

using Caster = py::object(*)(mitsuba::Object *);
extern Caster cast_object;

// Trampoline for derived types implemented in Python
class PyTraversalCallback : public TraversalCallback {
public:
    void put_parameter_impl(const std::string &name, void *ptr,
                            uint32_t flags, const std::type_info &type) override {
        py::gil_scoped_acquire gil;
        py::function overload = py::get_overload(this, "put_parameter");

        if (overload)
            overload(name, ptr, flags, (void *) &type);
        else
            Throw("TraversalCallback doesn't overload the method \"put_parameter\"");
    }

    void put_object(const std::string &name, Object *obj,
                    uint32_t flags) override {
        py::gil_scoped_acquire gil;
        py::function overload = py::get_overload(this, "put_object");

        if (overload)
            overload(name, cast_object(obj), flags);
        else
            Throw("TraversalCallback doesn't overload the method \"put_object\"");
    }
};

/// Defines a list of types that plugins can expose as tweakable/differentiable parameters
#define APPLY_FOR_EACH(T) \
    T(Float32); T(Float64); T(Int32); T(UInt32); T(DynamicBuffer<Float32>);    \
    T(DynamicBuffer<Float64>); T(DynamicBuffer<Int32>);                        \
    T(DynamicBuffer<UInt32>); T(Color1f); T(Color3f); T(Color1d); T(Color3d);  \
    T(Vector2i); T(Vector2u); T(Vector3i); T(Vector3u); T(Point2u); T(Point3u);\
    T(Point2f); T(Point3f); T(Vector2f); T(Vector3f); T(Vector4f); T(Normal3f);\
    T(Frame3f); T(Mask); T(Matrix3f); T(Matrix4f); T(Transform3f);             \
    T(Transform4f); T(Transform4d); T(Transform4d); T(TensorXf);               \
    if constexpr (!std::is_same_v<Float, ScalarFloat>) {                       \
        T(ScalarFloat32); T(ScalarFloat64); T(ScalarInt32); T(ScalarUInt32);   \
        T(ScalarColor1f); T(ScalarColor3f); T(ScalarColor1d);                  \
        T(ScalarColor3d); T(ScalarVector2i);                                   \
        T(ScalarVector2u); T(ScalarVector3i); T(ScalarVector3u);               \
        T(ScalarPoint2u); T(ScalarPoint3u); T(ScalarPoint2f); T(ScalarPoint3f);\
        T(ScalarVector2f); T(ScalarVector3f); T(ScalarVector4f);               \
        T(ScalarNormal3f); T(ScalarFrame3f); T(ScalarMatrix3f);                \
        T(ScalarMatrix4f); T(ScalarTransform3f); T(ScalarTransform4f);         \
        T(ScalarTransform3d); T(ScalarTransform4d); T(ScalarMask);             \
    }

/// Implementation detail of mitsuba.get_property
#define GET_PROPERTY_T(T)                                                      \
    if (strcmp(type.name(), typeid(T).name()) == 0)                            \
      return py::cast((T *) ptr, py::return_value_policy::reference_internal,  \
                      parent)

/// Implementation detail of mitsuba.set_property
#define SET_PROPERTY_T(T)                                                      \
    if (strcmp(type.name(), typeid(T).name()) == 0) {                          \
        *((T *) ptr) = py::cast<T>(value);                                     \
        return;                                                                \
    }

/// Macro to iterate over types that can be passed to put_parameter_impl
#define PUT_PARAMETER_IMPL_T(T)                                                \
    if (py::isinstance<T>(value)) {                                            \
        T v = py::cast<T>(value);                                              \
        self->put_parameter_impl(name, &v, flags, typeid(T));                  \
    }

/// Used to make the put_parameter_impl method accessible from the bindings
class PublicistTraversalCallback : public TraversalCallback {
public:
    using TraversalCallback::put_parameter_impl;
};

MI_PY_EXPORT(Object) {
    MI_PY_IMPORT_TYPES()

    m.def(
        "get_property",
        [](const void *ptr, void *type_, py::handle parent) -> py::object {
            const std::type_info &type = *(const std::type_info *) type_;
            APPLY_FOR_EACH(GET_PROPERTY_T);
            std::string name(type.name());
            py::detail::clean_type_id(name);
            Throw("get_property(): unsupported type \"%s\"!", name);
        },
        "ptr"_a, "type"_a, "parent"_a);

    m.def(
        "set_property",
        [](const void *ptr, void *type_, py::handle value) {
            const std::type_info &type = *(const std::type_info *) type_;
            APPLY_FOR_EACH(SET_PROPERTY_T);
            std::string name(type.name());
            py::detail::clean_type_id(name);
            Throw("set_property(): unsupported type \"%s\"!", name);
        },
        "ptr"_a, "type"_a, "value"_a);

    if constexpr (dr::is_array_v<ObjectPtr>) {
        py::object dr       = py::module_::import("drjit"),
                   dr_array = dr.attr("ArrayBase");
        py::class_<ObjectPtr> cls(m, "ObjectPtr", dr_array, py::module_local());
        bind_drjit_ptr_array(cls);
    }

    MI_PY_CHECK_ALIAS(TraversalCallback, "TraversalCallback") {
        py::class_<TraversalCallback, PyTraversalCallback>(
            m, "TraversalCallback", D(TraversalCallback))
            .def(py::init<>())
            .def("put_parameter",
                 [] (TraversalCallback &self_, const std::string &name, py::object &value, uint32_t flags) {
                    PublicistTraversalCallback *self = (PublicistTraversalCallback *) &self_;
                    APPLY_FOR_EACH(PUT_PARAMETER_IMPL_T);
                 },
                 "name"_a, "value"_a, "flags"_a, D(TraversalCallback, put_parameter))
            .def_method(TraversalCallback, put_object, "name"_a, "obj"_a, "flags"_a);
    }
}
