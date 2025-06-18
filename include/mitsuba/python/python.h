#pragma once

#if defined(_MSC_VER)
#  pragma warning (disable:5033) // 'register' is no longer a supported storage class
#endif

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/object.h>
#include <mitsuba/render/fwd.h>
#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/function.h>
#include <drjit/packet.h>
#include <drjit-core/jit.h>

#include "docstr.h"

#define D(...) DOC(mitsuba, __VA_ARGS__)

/// Shorthand notation for defining a data structure
#define MI_PY_STRUCT(Name, ...) \
    nb::class_<Name>(m, #Name, D(Name), ##__VA_ARGS__)

/// Shorthand notation for defining a Mitsuba class deriving from a base class
#define MI_PY_CLASS(Name, Base, ...) \
    nb::class_<Name, Base>(m, #Name, D(Name), ##__VA_ARGS__)

/// Shorthand notation for defining a Mitsuba class that can be extended in Python
#define MI_PY_TRAMPOLINE_CLASS(Trampoline, Name, Base, ...) \
    nb::class_<Name, Base, Trampoline>(m, #Name, D(Name), ##__VA_ARGS__)

/// Shorthand notation for defining attributes with read-write access
#define def_field(Class, Member, ...) \
    def_rw(#Member, &Class::Member, ##__VA_ARGS__)

/// Shorthand notation for defining enum members
#define def_value(Class, Value, ...) \
    value(#Value, Class::Value, D(Class, Value), ##__VA_ARGS__)

/// Shorthand notation for defining most kinds of methods
#define def_method(Class, Function, ...) \
    def(#Function, &Class::Function, D(Class, Function), ##__VA_ARGS__)

/// Shorthand notation for defining most kinds of static methods
#define def_static_method(Class, Function, ...) \
    def_static(#Function, &Class::Function, D(Class, Function), ##__VA_ARGS__)

/// Shorthand notation for defining __repr__ using operator<<
#define def_repr(Class) \
    def("__repr__", [](const Class &c) { std::ostringstream oss; oss << c; return oss.str(); } )

using namespace mitsuba;
namespace nb = nanobind;
using namespace nb::literals;

template <typename T>
nb::handle type_of() {
    if constexpr (std::is_integral_v<T>)
        return nb::handle((PyObject *) &PyLong_Type);
    else if constexpr (std::is_floating_point_v<T>)
        return nb::handle((PyObject *) &PyFloat_Type);
    else if constexpr (std::is_pointer_v<T>)
        return nb::type<std::decay_t<std::remove_pointer_t<T>>>();
    else
        return nb::type<T>();
}

#define MI_PY_DRJIT_STRUCT_BIND_FIELD(x) \
    fields[#x] = type_of<decltype(struct_type_().x)>();

#define MI_PY_DRJIT_STRUCT(cls, Type, ...)                                     \
    nb::dict fields;                                                           \
    using struct_type_ = Type;                                                 \
    DRJIT_MAP(MI_PY_DRJIT_STRUCT_BIND_FIELD, __VA_ARGS__)                      \
    cls.attr("DRJIT_STRUCT") = fields;                                         \
    cls.def("assign", [](Type &a, const Type &b) {                             \
        if (&a != &b)                                                          \
            a = b;                                                             \
    });                                                                        \
    cls.def("__setitem__",                                                     \
            [](Type &type, const Mask &mask, const Type &value) {              \
                type = dr::select(mask, value, type);                          \
            });

#define MI_PY_DECLARE(Name) extern void python_export_##Name(nb::module_ &m)
/// We forward bindings code to a templated to ensure that branches not taken
/// of ``if constexpr`` do not get instantiated by the compiler.
#define MI_PY_EXPORT(Name)                                                     \
    template <int = 0> void python_export_impl_##Name(nb::module_ &m);         \
    void python_export_##Name(nb::module_ &m) {                                \
        python_export_impl_##Name<>(m);                                        \
    }                                                                          \
    template <int> void python_export_impl_##Name(nb::module_ &m)
#define MI_PY_IMPORT(Name) python_export_##Name(m)
#define MI_PY_IMPORT_SUBMODULE(Name) python_export_##Name(Name)

#define MI_PY_IMPORT_TYPES(...)                                                \
    using Float    = MI_VARIANT_FLOAT;                                         \
    using Spectrum = MI_VARIANT_SPECTRUM;                                      \
    MI_IMPORT_TYPES(__VA_ARGS__)                                               \
    MI_IMPORT_OBJECT_TYPES()

inline nb::module_ create_submodule(nb::module_ &m, const char *name) {
    std::string full_name = std::string(PyModule_GetName(m.ptr())) + "." + name;
    nb::module_ module = nb::steal<nb::module_>(PyModule_New(full_name.c_str()));
    m.attr(name) = module;
    return module;
}

#define MI_PY_CHECK_ALIAS(Type, Name)                 \
    if (auto h = nb::type<Type>(); h) {               \
        m.attr(Name) = h;                             \
    }                                                 \
    else
