#pragma once

#if defined(_MSC_VER)
#  pragma warning (disable:5033) // 'register' is no longer a supported storage class
#endif

#include <mitsuba/mitsuba.h>
#include <mitsuba/python/config.h>
#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <tbb/tbb.h>
#include "docstr.h"
#include <mitsuba/core/object.h>
#include <enoki/python.h>
#include <enoki/stl.h>

PYBIND11_DECLARE_HOLDER_TYPE(T, mitsuba::ref<T>, true);

#define D(...) "doc disabled" // TODO re-enable this DOC(mitsuba, __VA_ARGS__)

#define MTS_PY_CLASS(Name, Base, ...) \
    py::class_<Name, Base, ref<Name>>(m, #Name, D(Name), ##__VA_ARGS__)

#define MTS_PY_TRAMPOLINE_CLASS(Trampoline, Name, Base, ...) \
    py::class_<Name, Base, ref<Name>, Trampoline>(m, #Name, D(Name), ##__VA_ARGS__)

#define MTS_PY_STRUCT(Name, ...) \
    py::class_<Name>(m, #Name, D(Name), ##__VA_ARGS__)

/// Shorthand notation for defining read_write members
#define def_field(Class, Member) \
    def_readwrite(#Member, &Class::Member, D(Class, Member))

/// Shorthand notation for defining most kinds of methods
#define def_method(Class, Function, ...) \
    def(#Function, &Class::Function, D(Class, Function), ##__VA_ARGS__)

/// Shorthand notation for defining most kinds of static methods
#define def_static_method(Class, Function, ...) \
    def_static(#Function, &Class::Function, D(Class, Function), ##__VA_ARGS__)

/// Shorthand notation for defining __repr__ using operator<<
#define def_repr(Class) \
    def("__repr__", [](const Class &c) { std::ostringstream oss; oss << c; return oss.str(); } )

#define MTS_PY_IMPORT_MODULE(Name, ModuleName) \
    auto Name = py::module::import(ModuleName); (void) m;

using namespace mitsuba;

namespace py = pybind11;

using namespace py::literals;

extern py::dtype dtype_for_struct(const Struct *s);

template <typename VectorClass, typename ScalarClass, typename ClassBinding>
void bind_slicing_operators(ClassBinding &c) {
    c.def(py::init([](size_t n) -> VectorClass {
        return zero<VectorClass>(n);
    }))
    .def("__getitem__", [](VectorClass &r, size_t i) {
        if (i >= slices(r))
            throw py::index_error();
        return ScalarClass(enoki::slice(r, i));
    })
    .def("__setitem__", [](VectorClass &r, size_t i, const ScalarClass &r2) {
        if (i >= slices(r))
            throw py::index_error();
        enoki::slice(r, i) = r2;
    })
    .def("__len__", [](const VectorClass &r) {
        return slices(r);
    })
    ;
}

template <typename Source, typename Target> void pybind11_type_alias() {
    auto &types = pybind11::detail::get_internals().registered_types_cpp;
    auto it = types.find(std::type_index(typeid(Source)));
    if (it == types.end())
        throw std::runtime_error("pybind11_type_alias(): source type not found!");
    types[std::type_index(typeid(Target))] = it->second;
}

template <typename Type> pybind11::handle get_type_handle() {
    return pybind11::detail::get_type_handle(typeid(Type), false);
}

#define MTS_PY_CHECK_ALIAS(Name, Module)              \
    if (auto h = get_type_handle<Name>(); h) {        \
        Module.attr(#Name) = h;                       \
    }                                                 \
    else

template<typename Float, typename Func>
auto vectorize(Func func) {
    if constexpr (is_array_v<Float> && !is_dynamic_v<Float>)
        return enoki::vectorize_wrapper(func);
    else
        return func;
}
