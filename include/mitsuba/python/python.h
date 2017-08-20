#pragma once

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

#define D(...) DOC(mitsuba, __VA_ARGS__)

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

/// Shorthand notation for defining most kinds of methods
#define mdef(Class, Function, ...) \
    def(#Function, &Class::Function, D(Class, Function), ##__VA_ARGS__)

/// Shorthand notation for defining most kinds of static methods
#define sdef(Class, Function, ...) \
    def_static(#Function, &Class::Function, D(Class, Function), ##__VA_ARGS__)

#define MTS_PY_IMPORT_MODULE(Name, ModuleName) \
    auto Name = py::module::import(ModuleName); (void) m;

using namespace mitsuba;

namespace py = pybind11;

using namespace py::literals;

extern py::dtype dtype_for_struct(const Struct *s);

template <typename VectorClass, typename ScalarClass, typename ClassBinding>
void bind_slicing_operators(ClassBinding &c) {
    c.def("__init__", [](VectorClass &r, size_t n) {
        new (&r) VectorClass();
        set_slices(r, n);
    })
    .def("__getitem__", [](VectorClass &r, size_t i) {
        if (i >= slices(r))
            throw py::index_error();
        return ScalarClass(enoki::slice(r, i));
    })
    .def("__setitem__", [](VectorClass &r, size_t i, const ScalarClass &r2) {
        if (i >= slices(r))
            throw py::index_error();
        enoki::slice(r, i) = r2;
    });
}

