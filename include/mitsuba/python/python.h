#pragma once

#if defined(_MSC_VER)
#  pragma warning (disable:5033) // 'register' is no longer a supported storage class
#endif

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/object.h>
#include <mitsuba/render/fwd.h>
#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <enoki/stl.h>

#if MTS_VARIANT_VECTORIZE == 1
#  include <enoki/dynamic.h>
#endif

#include "docstr.h"

PYBIND11_DECLARE_HOLDER_TYPE(T, mitsuba::ref<T>, true);

#define D(...) DOC(mitsuba, __VA_ARGS__)

#define MTS_PY_CLASS(Name, Base, ...) \
    py::class_<Name, Base, ref<Name>>(m, #Name, D(Name), ##__VA_ARGS__)

#define MTS_PY_TRAMPOLINE_CLASS(Trampoline, Name, Base, ...) \
    py::class_<Name, Base, ref<Name>, Trampoline>(m, #Name, D(Name), ##__VA_ARGS__)

#define MTS_PY_STRUCT(Name, ...) \
    py::class_<Name>(m, #Name, D(Name), ##__VA_ARGS__)

/// Shorthand notation for defining read_write members
#define def_field(Class, Member, ...) \
    def_readwrite(#Member, &Class::Member, ##__VA_ARGS__)

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

/// Shorthand notation for defining object registration routine for trampoline objects
#define MTS_PY_REGISTER_OBJECT(Function, Name)                                                     \
    m.def(Function,                                                                                \
        [](const std::string &name, std::function<py::object(const Properties &)> &constructor) {  \
            (void) new Class(name, #Name, ::mitsuba::detail::get_variant<Float, Spectrum>(),       \
                            [=](const Properties &p) {                                             \
                                return constructor(p).release().cast<ref<Name>>();                 \
                            },                                                                     \
                            nullptr);                                                              \
            PluginManager::instance()->register_python_plugin(name);                               \
        });                                                                                        \

using namespace mitsuba;

namespace py = pybind11;

using namespace py::literals;

//extern py::dtype dtype_for_struct(const Struct *s);

//template <typename VectorClass, typename ScalarClass, typename ClassBinding>
//void bind_slicing_operators(ClassBinding &c) {
    //using Float = typename VectorClass::Float;
    //if constexpr (is_dynamic_v<Float> && !is_cuda_array_v<Float>) {
        //c.def(py::init([](size_t n) -> VectorClass {
            //return zero<VectorClass>(n);
        //}))
        //.def("__getitem__", [](VectorClass &r, size_t i) {
            //if (i >= slices(r))
                //throw py::index_error();
            //return ScalarClass(enoki::slice(r, i));
        //})
        //.def("__setitem__", [](VectorClass &r, size_t i, const ScalarClass &r2) {
            //if (i >= slices(r))
                //throw py::index_error();
            //enoki::slice(r, i) = r2;
        //})
        //.def("__len__", [](const VectorClass &r) {
            //return slices(r);
        //});
    //}
//}

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

#define MTS_PY_DECLARE(name) extern void python_export_##name(py::module &m)
#define MTS_PY_EXPORT(name) void python_export_##name(py::module &m)
#define MTS_PY_IMPORT(name) python_export_##name(m)

#define MTS_MODULE_NAME_1(lib, variant) lib##_##variant##_ext
#define MTS_MODULE_NAME(lib, variant) MTS_MODULE_NAME_1(lib, variant)

#define MTS_PY_IMPORT_TYPES()                                                                      \
    using Float    = MTS_VARIANT_FLOAT;                                                            \
    using Spectrum = MTS_VARIANT_SPECTRUM;                                                         \
    MTS_IMPORT_TYPES()                                                                             \
    MTS_IMPORT_OBJECT_TYPES()

#define MTS_PY_IMPORT_TYPES_DYNAMIC()                                                              \
    using Float = std::conditional_t<is_static_array_v<MTS_VARIANT_FLOAT>,                         \
                                     make_dynamic_t<MTS_VARIANT_FLOAT>, MTS_VARIANT_FLOAT>;        \
                                                                                                   \
    using Spectrum =                                                                               \
        std::conditional_t<is_static_array_v<MTS_VARIANT_FLOAT>,                                   \
                           make_dynamic_t<MTS_VARIANT_SPECTRUM>, MTS_VARIANT_SPECTRUM>;            \
                                                                                                   \
    MTS_IMPORT_TYPES()                                                                             \
    MTS_IMPORT_OBJECT_TYPES()

template <typename Func>
decltype(auto) vectorize(const Func &func) {
#if MTS_VARIANT_VECTORIZE == 1
    return enoki::vectorize_wrapper(func);
#else
    return func;
#endif
}
