#include <mitsuba/core/transform.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <drjit/dynamic.h>
#include <drjit/tensor.h>
#include <drjit/python.h>

using Caster = nb::object(*)(mitsuba::Object *);
extern Caster cast_object;

// Trampoline for derived types implemented in Python
class PyTraversalCallback : public TraversalCallback {
public:
    NB_TRAMPOLINE(TraversalCallback, 2);

    void put_parameter_impl(const std::string &name, void *ptr,
                            uint32_t flags, const std::type_info &type) override {
        nanobind::detail::ticket nb_ticket(nb_trampoline, "put_parameter", true);
        nb_trampoline.base().attr(nb_ticket.key)(name, ptr, flags, (void *) &type);
    }

    void put_object(const std::string &name, Object *obj,
                    uint32_t flags) override {
        nanobind::detail::ticket nb_ticket(nb_trampoline, "put_object", true);
        nb_trampoline.base().attr(nb_ticket.key)(name, cast_object(obj), flags);
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
    T(Transform4f); T(Transform4d); T(Transform4d); T(TensorXf); T(TensorXf16);\
    T(TensorXf32); T(TensorXf64);                                              \
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
/// FIXME: casted return should use rv_policy::reference_interal
#define GET_PROPERTY_T(T)                                                      \
    if (strcmp(type.name(), typeid(T).name()) == 0)                            \
      return nb::cast((T *) ptr, nb::rv_policy::reference_internal, parent);

/// Implementation detail of mitsuba.set_property
#define SET_PROPERTY_T(T)                                                      \
    if (strcmp(type.name(), typeid(T).name()) == 0) {                          \
        nb::handle h = nb::type<T>();                                          \
        *((T *) ptr) = nb::cast<T>(h.is_valid() ? h(value) : value);           \
        return;                                                                \
    }

/// Macro to iterate over types that can be passed to put_parameter_impl
#define PUT_PARAMETER_IMPL_T(T)                                                \
    if (nb::isinstance<T>(value)) {                                            \
        T v = nb::cast<T>(value);                                              \
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
        [](const void *ptr, void *type_, nb::handle parent) -> nb::object {
            (void) parent;
            const std::type_info &type = *(const std::type_info *) type_;
            APPLY_FOR_EACH(GET_PROPERTY_T);
            // FIXME: Unmangle type name
            Throw("get_property(): unsupported type \"%s\"!", type.name());
        },
        "ptr"_a, "type"_a, "parent"_a);

    m.def(
        "set_property",
        [](const void *ptr, void *type_, nb::handle value) {
            const std::type_info &type = *(const std::type_info *) type_;
            APPLY_FOR_EACH(SET_PROPERTY_T);
            // FIXME: Unmangle type name
            Throw("set_property(): unsupported type \"%s\"!", type.name());
        },
        "ptr"_a, "type"_a, "value"_a);

    m.def(
        "set_property",
        [](nb::handle dst, nb::handle src) {
            nb::handle h = dst.type();
            if (nb::type_check(h)) {
                nb::inst_replace_copy(dst, h(src));
                return;
            }
            Throw("set_property(): Target property type isn't a nanobind type!");
        },
        "dst"_a, "src"_a);

    if constexpr (dr::is_array_v<ObjectPtr>) {
        dr::ArrayBinding b;
        auto object_ptr = dr::bind_array_t<ObjectPtr>(b, m, "ObjectPtr");
    }

    MI_PY_CHECK_ALIAS(TraversalCallback, "TraversalCallback") {
        nb::class_<TraversalCallback, PyTraversalCallback>(
            m, "TraversalCallback", D(TraversalCallback))
            .def(nb::init<>())
            .def("put_parameter",
                 [] (TraversalCallback &self_, const std::string &name, nb::object &value, uint32_t flags) {
                    PublicistTraversalCallback *self = (PublicistTraversalCallback *) &self_;
                    APPLY_FOR_EACH(PUT_PARAMETER_IMPL_T);
                 },
                 "name"_a, "value"_a, "flags"_a, D(TraversalCallback, put_parameter))
            .def_method(TraversalCallback, put_object, "name"_a, "obj"_a, "flags"_a);
    }
}
