#pragma once

#if defined(_MSC_VER)
#  pragma warning (disable:5033) // 'register' is no longer a supported storage class
#endif

#include <mitsuba/mitsuba.h>
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

#define MTS_PY_DECLARE(name) \
    extern void python_export_##name(py::module &)

#define MTS_PY_IMPORT(name) \
    python_export_##name(m)

#define MTS_PY_EXPORT(name) \
    void python_export_##name(py::module &m)

#define MTS_PY_CLASS(Name, Base, ...) \
    py::class_<Name, Base, ref<Name>>(m, #Name, D(Name), ##__VA_ARGS__)

#define MTS_PY_TRAMPOLINE_CLASS(Trampoline, Name, Base, ...) \
    py::class_<Name, Base, ref<Name>, Trampoline>(m, #Name, D(Name), ##__VA_ARGS__)

#define MTS_PY_STRUCT(Name, ...) \
    py::class_<Name>(m, #Name, D(Name), ##__VA_ARGS__)

/// Shorthand notation for defining read_write members
#define rwdef(Class, Member) \
    def_readwrite(#Member, &Class::Member, D(Class, Member))

/// Shorthand notation for defining most kinds of methods
#define mdef(Class, Function, ...) \
    def(#Function, &Class::Function, D(Class, Function), ##__VA_ARGS__)

/// Shorthand notation for defining most kinds of static methods
#define sdef(Class, Function, ...) \
    def_static(#Function, &Class::Function, D(Class, Function), ##__VA_ARGS__)

/// Shorthand notation for defining __repr__ using operator<<
#define repr_def(Class) \
    def("__repr__", [](const Class &c) { std::ostringstream oss; oss << c; return oss.str(); } )

// #define MTS_VECTORIZE_WRAPPER(Function) \
    // enoki::vectorize_wrapper(Function)




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


template <typename Class, typename... Args, typename... Extra> auto bind_array(py::module &m, const char *name, const Extra&... extra) {
    return py::class_<Class, Args...>(m, name, extra...)
        .def("__len__", &Class::size)
        .repr_def(Class)
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__getitem__", [](const Class &a, size_t index) {
            if (index >= a.size())
                throw py::index_error();
            return a.coeff(index);
        });
}

template<typename Float, typename Func> auto mts_vectorize_wrapper(Func func) {
    if constexpr (is_array_v<Float> && !is_dynamic_v<Float>)
        return func;
    else
        return enoki::vectorize_wrapper(func);
}

#define MTS_VECTORIZE_WRAPPER mts_vectorize_wrapper<Float>