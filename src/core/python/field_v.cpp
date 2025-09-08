#include <mitsuba/core/fwd.h>

#include <mitsuba/core/object.h>

#include <mitsuba/core/field.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

namespace nb = nanobind;

template <typename DeviceType, typename HostType>
void bind_field(nb::module_ &m, const char *name) {
    using Field = mitsuba::field<DeviceType, HostType>;

    // If the type is already bound, retrieve the existing binding
    if (auto h = nb::type<Field>(); h.is_valid()) {
        m.attr(name) = h;
        return;
    }

    nb::class_<Field>(m, name, "Wrapper for a host and/or device-side field.")
        .def(nb::init<>(),
             "Default constructor: creates an uninitialized field.")
        .def(nb::init<const HostType &>(), "Constructs from a host-side value.")
        .def_prop_rw(
            "value", [](Field &f) -> DeviceType & { return f.value(); },
            [](Field &f, const DeviceType &v) { f = v; },
            nb::rv_policy::reference_internal,
            "Access the JIT-compiled device-side representation of the "
            "field.")
        .def_prop_ro("scalar", &Field::scalar,
                     nb::rv_policy::reference_internal,
                     "Access the host-side representation of the field.")
        .def_repr(Field);
}

MI_PY_EXPORT(Field) {
    MI_PY_IMPORT_TYPES()
    bind_field<Float, ScalarFloat>(m, "FieldFloat");
    bind_field<Point3f, ScalarPoint3f>(m, "FieldPoint3f");
    bind_field<AffineTransform4f, ScalarAffineTransform4f>(
        m, "FieldAffineTransform4f");
}
