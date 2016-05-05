#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/eigen.h>
#include <pybind11/stl.h>
#include "docstr.h"
#include <mitsuba/core/object.h>

PYBIND11_DECLARE_HOLDER_TYPE(T, mitsuba::ref<T>);

#define D(...) DOC(__VA_ARGS__)
#define DM(...) DOC(mitsuba, __VA_ARGS__)

#define MTS_PY_DECLARE(name) \
    extern void python_export_##name(py::module &)

#define MTS_PY_IMPORT(name) \
    python_export_##name(m)

#define MTS_PY_EXPORT(name) \
    void python_export_##name(py::module &m)

#define MTS_PY_CLASS(Name, Base, ...) \
    py::class_<Name, ref<Name>>(m, #Name, DM(Name), py::base<Base>(), ##__VA_ARGS__)

#define MTS_PY_TRAMPOLINE_CLASS(Trampoline, Name, Base, ...) \
    py::class_<Trampoline, ref<Trampoline>>(m, #Name, DM(Name), py::base<Base>(), ##__VA_ARGS__) \
       .alias<Name>()

#define MTS_PY_STRUCT(Name, ...) \
    py::class_<Name>(m, #Name, DM(Name), ##__VA_ARGS__)

/// Shorthand notation for defining most kinds of methods
#define mdef(Class, Function, ...) \
    def(#Function, &Class::Function, DM(Class, Function), ##__VA_ARGS__)

/// Shorthand notation for defining most kinds of static methods
#define sdef(Class, Function, ...) \
    def_static(#Function, &Class::Function, DM(Class, Function), ##__VA_ARGS__)

using namespace mitsuba;

namespace py = pybind11;
