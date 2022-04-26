#include <mitsuba/core/transform.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/python/python.h>
#include <drjit/dynamic.h>
#include <drjit/tensor.h>

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

/// Implementation detail of mitsuba.core.get_property
#define GET_PROPERTY_T(T)                                                      \
    if (strcmp(type.name(), typeid(T).name()) == 0)                            \
      return py::cast((T *) ptr, py::return_value_policy::reference_internal,  \
                      parent)

#define SET_PROPERTY_T(T)                                                      \
    if (strcmp(type.name(), typeid(T).name()) == 0) {                          \
        *((T *) ptr) = py::cast<T>(value);                                     \
        return;                                                                \
    }

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
}
