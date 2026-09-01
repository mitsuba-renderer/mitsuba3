#include <mitsuba/core/transform.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string_view.h>
#include <drjit/python.h>

using Caster = nb::object(*)(mitsuba::Object *);
extern Caster cast_object;

// Trampoline for derived types implemented in Python
class PyTraversalCallback : public TraversalCallback {
public:
    NB_TRAMPOLINE(TraversalCallback);

    void put_value(std::string_view name, void *ptr,
                   uint32_t flags, const std::type_info &type) override {
        constexpr uint64_t nb_hash = nanobind::detail::str_hash("put");
        nanobind::detail::ticket nb_ticket(nb_trampoline, "put", nb_hash, true);
        nb_trampoline.base().attr(nb_ticket.key)(name, ptr, flags, (void *) &type);
    }

    void put_object(std::string_view name, Object *obj, uint32_t flags) override {
        constexpr uint64_t nb_hash = nanobind::detail::str_hash("put");
        nanobind::detail::ticket nb_ticket(nb_trampoline, "put", nb_hash, true);
        nb_trampoline.base().attr(nb_ticket.key)(name, cast_object(obj), flags);
    }
};

/// Used to make the put_value and put_object methods accessible from the bindings
class TraversalCallbackPublicist : public TraversalCallback {
public:
    using TraversalCallback::put_value;
    using TraversalCallback::put_object;
    using TraversalCallback::keep_alive;
};

MI_PY_EXPORT(Object) {
    MI_PY_IMPORT_TYPES()
    // Define ObjectPtr for DrJit array binding based on current Float type
    using ObjectPtr = dr::replace_scalar_t<Float, const Object *>;

    MI_PY_CHECK_ALIAS(TraversalCallback, "TraversalCallback") {
        auto put = [](TraversalCallback &self_, nb::str name_,
                      nb::handle value, uint32_t flags) {
            TraversalCallbackPublicist *self = (TraversalCallbackPublicist *) &self_;
            std::string_view name = name_.c_str();

            if (!nb::inst_check(value)) {
                // A value without bindings, such as a scalar Python float object
                self->put_value(name, value.ptr(), flags, typeid(PyObject *));
                return;
            }

            Object *o = nullptr;
            if (nb::try_cast(value, o)) {
                self->put_object(name, o, flags);
            } else {
                // The value may be a temporary of the Python plugin that
                // reported it, see TraversalCallback::keep_alive()
                self->keep_alive(value.ptr());
                nb::handle tp = value.type();
                const std::type_info &tpi = nb::type_info(tp);
                self->put_value(name, nb::inst_ptr<void>(value), flags, tpi);
            }
        };

        nb::class_<TraversalCallback, PyTraversalCallback>(
            m, "TraversalCallback", D(TraversalCallback))
            .def(nb::init<>())

            // Unified put() function that handles both objects and values
            .def("put", put, "name"_a, "value"_a, "flags"_a,
                 "Unified method to register both objects and values with the traversal callback")
            .def("put_object", put, "name"_a, "value"_a, "flags"_a,
                 "Alias of put(), retained for plugins written against the "
                 "older interface")
            .def("put_value", put, "name"_a, "value"_a, "flags"_a,
                 "Alias of put(), retained for plugins written against the "
                 "older interface");
    }
}
