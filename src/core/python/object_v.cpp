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
    NB_TRAMPOLINE(TraversalCallback, 2);

    void put_value(std::string_view name, void *ptr,
                   uint32_t flags, const std::type_info &type) override {
        nanobind::detail::ticket nb_ticket(nb_trampoline, "put", true);
        nb_trampoline.base().attr(nb_ticket.key)(name, ptr, flags, (void *) &type);
    }

    void put_object(std::string_view name, Object *obj, uint32_t flags) override {
        nanobind::detail::ticket nb_ticket(nb_trampoline, "put", true);
        nb_trampoline.base().attr(nb_ticket.key)(name, cast_object(obj), flags);
    }
};

/// Used to make the put_value and put_object methods accessible from the bindings
class TraversalCallbackPublicist : public TraversalCallback {
public:
    using TraversalCallback::put_value;
    using TraversalCallback::put_object;
};

#define TRY_SCALAR_GET(T)                                                      \
    if (*type == typeid(T))                                                    \
        return nb::cast(*(T *) ptr)

#define TRY_SCALAR_SET(T)                                                      \
    if (*type == typeid(T)) {                                                  \
        *(T *) ptr = nb::cast<T>(src);                                         \
        return;                                                                \
    }

/// Return a Python object from a specific memory address as raw reference or safe reference
static nb::object get_property(void *ptr, void *type_, nb::handle parent) {
    const std::type_info *type = (const std::type_info *) type_;

    TRY_SCALAR_GET(float);
    TRY_SCALAR_GET(double);
    TRY_SCALAR_GET(bool);
    TRY_SCALAR_GET(uint32_t);
    TRY_SCALAR_GET(int32_t);

    nb::rv_policy rvp = parent.is_valid() ? nb::rv_policy::reference_internal
                                          : nb::rv_policy::reference;
    nb::detail::cleanup_list cleanup(parent.ptr());

    nb::object r =
        nb::steal(nb::detail::nb_type_put(type, ptr, rvp, &cleanup, nullptr));

    if (!r.is_valid())
        Throw("get_property(): unsupported type \"%s\"!", type->name());

    cleanup.release();
    return r;
}

/// "Overwrite an object at a specific memory address with the contents of a compatible Python object"
static void set_property(void *ptr, void *type_, nb::object src) {
    const std::type_info *type = (const std::type_info *) type_;

    TRY_SCALAR_SET(float);
    TRY_SCALAR_SET(double);
    TRY_SCALAR_SET(bool);
    TRY_SCALAR_SET(uint32_t);
    TRY_SCALAR_SET(int32_t);

    nb::object dst = get_property(ptr, type_, nb::handle());
    nb::handle tp  = dst.type();

    if (!nb::type_check(tp))
        nb::raise_type_error("set_property(): Target property type "
                             "(%s) is not a nanobind type!",
                             nb::type_name(tp).c_str());

    if (!tp.is(src.type()))
        src = tp(src);

    nb::inst_replace_copy(dst, src);
}

MI_PY_EXPORT(Object) {
    MI_PY_IMPORT_TYPES()
    // Define ObjectPtr for DrJit array binding based on current Float type
    using ObjectPtr = dr::replace_scalar_t<Float, const Object *>;

    m.def("get_property", &get_property, "ptr"_a, "type"_a, "parent"_a,
          "Return a Python object from a specific memory address as raw "
          "reference or safe reference");

    // Overwrite a Python object at a specific memory address with 'value'
    m.def("set_property", &set_property, "ptr"_a, "type"_a, "src"_a,
          "Overwrite an object at a specific memory address with the contents "
          "of a compatible Python object");

    m.def("set_property",
          [](nb::handle dst, nb::object src) {
              nb::handle tp = dst.type();
              if (!nb::type_check(tp))
                  nb::raise_type_error("set_property(): Target property type "
                                       "(%s) is not a nanobind type!",
                                       nb::type_name(tp).c_str());

              if (!tp.is(src.type()))
                  src = tp(src);

              nb::inst_replace_copy(dst, src);
          },
          "dst"_a, "src"_a);

    MI_PY_CHECK_ALIAS(TraversalCallback, "TraversalCallback") {
        nb::class_<TraversalCallback, PyTraversalCallback>(
            m, "TraversalCallback", D(TraversalCallback))
            .def(nb::init<>())

            // Unified put() function that handles both objects and values
            .def("put",
                 [] (TraversalCallback &self_, std::string_view name, nb::handle value, uint32_t flags) {
                    TraversalCallbackPublicist *self = (TraversalCallbackPublicist *) &self_;
                    if (!nb::inst_check(value))
                        nb::raise_type_error(
                            "TraversalCallback::put(): type '%s' was not "
                            "registered by nanobind.",
                            nb::inst_name(value).c_str());

                    Object *o = nullptr;
                    if (nb::try_cast(value, o)) {
                        self->put_object(name, o, flags);
                    } else {
                        nb::handle tp = value.type();
                        const std::type_info &tpi = nb::type_info(tp);
                        self->put_value(name, nb::inst_ptr<void>(value), flags, tpi);
                    }
                 },
                 "name"_a, "value"_a, "flags"_a,
                 "Unified method to register both objects and values with the traversal callback")
            // Deprecated put_value - forwards to put()
            .def("put_value",
                 [] (nb::handle self, std::string_view name, nb::handle value, uint32_t flags) {
                    if (PyErr_WarnEx(PyExc_DeprecationWarning,
                                     "TraversalCallback.put_value() is deprecated, use put() instead", 1) < 0)
                        nb::raise_python_error();

                    // Forward to put() method
                    self.attr("put")(name, value, flags);
                 },
                 "name"_a, "value"_a, "flags"_a,
                 "Register a value with the traversal callback.\n\n"
                 ".. deprecated:: 3.7.0\n"
                 "   Use :py:meth:`~mitsuba.TraversalCallback.put` instead.")
            // Deprecated put_object - forwards to put()
            .def("put_object",
                 [] (nb::handle self, std::string_view name, Object *obj, uint32_t flags) {
                    if (PyErr_WarnEx(PyExc_DeprecationWarning,
                                     "TraversalCallback.put_object() is deprecated, use put() instead", 1) < 0)
                        nb::raise_python_error();

                    // Forward to put() method
                    self.attr("put")(name, cast_object(obj), flags);
                 },
                 "name"_a, "obj"_a, "flags"_a,
                 "Register an object with the traversal callback.\n\n"
                 ".. deprecated:: 3.7.0\n"
                 "   Use :py:meth:`~mitsuba.TraversalCallback.put` instead.");
    }
}
