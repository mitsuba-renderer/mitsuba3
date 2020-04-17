#include <mitsuba/python/python.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/frame.h>

#define GET_ATTR(T)                                                                                \
    if (strcmp(type.name(), typeid(T).name()) == 0)                                                \
       return py::cast((T *) ptr, py::return_value_policy::reference_internal, parent)

#define SET_ATTR(T)                                                                                \
    if (strcmp(type.name(), typeid(T).name()) == 0) {                                              \
        *((T *) ptr) = py::cast<T>(handle);                                                        \
        return;                                                                                    \
    }

MTS_PY_EXPORT(Object) {
    MTS_PY_IMPORT_TYPES_DYNAMIC()

    m.def("get_property", [](const void *ptr, void *type_, py::handle parent) -> py::object {
        const std::type_info &type = *(const std::type_info *) type_;
        GET_ATTR(Float);
        GET_ATTR(Int32);
        GET_ATTR(UInt32);
        GET_ATTR(DynamicBuffer<Float>);
        GET_ATTR(DynamicBuffer<Int32>);
        GET_ATTR(DynamicBuffer<UInt32>);
        GET_ATTR(Color1f);
        GET_ATTR(Color3f);
        GET_ATTR(Vector2i);
        GET_ATTR(Vector2u);
        GET_ATTR(Point2u);
        GET_ATTR(Point3u);
        GET_ATTR(Point2f);
        GET_ATTR(Point3f);
        GET_ATTR(Vector2f);
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
            GET_ATTR(ScalarColor1f);
            GET_ATTR(ScalarColor3f);
            GET_ATTR(ScalarVector2i);
            GET_ATTR(ScalarVector2u);
            GET_ATTR(ScalarPoint2u);
            GET_ATTR(ScalarPoint3u);
            GET_ATTR(ScalarPoint2f);
            GET_ATTR(ScalarPoint3f);
            GET_ATTR(ScalarVector2f);
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
    }, "cpp_type"_a, "ptr"_a, "parent"_a);

    m.def("set_property", [](const void *ptr, void *type_, py::handle handle) {
        const std::type_info &type = *(const std::type_info *) type_;

        SET_ATTR(DynamicBuffer<Float>);
        SET_ATTR(DynamicBuffer<Int32>);
        SET_ATTR(DynamicBuffer<UInt32>);
        SET_ATTR(Color1f);
        SET_ATTR(Color3f);
        SET_ATTR(Vector2i);
        SET_ATTR(Vector2u);
        SET_ATTR(Point2u);
        SET_ATTR(Point3u);
        SET_ATTR(Point2f);
        SET_ATTR(Point3f);
        SET_ATTR(Vector2f);
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
            SET_ATTR(ScalarColor1f);
            SET_ATTR(ScalarColor3f);
            SET_ATTR(ScalarVector2i);
            SET_ATTR(ScalarVector2u);
            SET_ATTR(ScalarPoint2u);
            SET_ATTR(ScalarPoint3u);
            SET_ATTR(ScalarPoint2f);
            SET_ATTR(ScalarPoint3f);
            SET_ATTR(ScalarVector2f);
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
}
