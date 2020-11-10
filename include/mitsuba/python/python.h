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
#include <enoki/packet.h>
#include <enoki-jit/jit.h>

#include "docstr.h"

PYBIND11_DECLARE_HOLDER_TYPE(T, mitsuba::ref<T>, true);

#define D(...) DOC(mitsuba, __VA_ARGS__)

/// Shorthand notation for defining a data structure
#define MTS_PY_STRUCT(Name, ...) \
    py::class_<Name>(m, #Name, D(Name), ##__VA_ARGS__)

/// Shorthand notation for defining a Mitsuba class deriving from a base class
#define MTS_PY_CLASS(Name, Base, ...) \
    py::class_<Name, Base, ref<Name>>(m, #Name, D(Name), ##__VA_ARGS__)

/// Shorthand notation for defining a Mitsuba class that can be extended in Python
#define MTS_PY_TRAMPOLINE_CLASS(Trampoline, Name, Base, ...) \
    py::class_<Name, Base, ref<Name>, Trampoline>(m, #Name, D(Name), ##__VA_ARGS__)

/// Shorthand notation for defining attributes with read-write access
#define def_field(Class, Member, ...) \
    def_readwrite(#Member, &Class::Member, ##__VA_ARGS__)

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

/// Shorthand notation for defining object registration routine for trampoline objects
/// WARNING: this will leak out memory as the constructed py::object will never be destroyed
#define MTS_PY_REGISTER_OBJECT(Function, Name)                                                     \
    m.def(Function,                                                                                \
        [](const std::string &name, std::function<py::object(const Properties *)> &constructor) {  \
            (void) new Class(name, #Name, ::mitsuba::detail::get_variant<Float, Spectrum>(),       \
                            [=](const Properties &p) {                                             \
                                py::object o = constructor(&p);                                    \
                                return o.release().cast<ref<Name>>();                              \
                            },                                                                     \
                            nullptr);                                                              \
            PluginManager::instance()->register_python_plugin(name);                               \
        });                                                                                        \

using namespace mitsuba;

namespace py = pybind11;

using namespace py::literals;

template <typename T>
py::handle type_of() {
    if constexpr (std::is_integral_v<T>)
        return py::handle((PyObject *) &PyLong_Type);
    else if constexpr (std::is_floating_point_v<T>)
        return py::handle((PyObject *) &PyFloat_Type);
    else if constexpr (std::is_pointer_v<T>)
        return py::type::of<std::decay_t<std::remove_pointer_t<T>>>();
    else
        return py::type::of<T>();
}

#define MTS_PY_ENOKI_STRUCT_BIND_FIELD(x) \
    fields[#x] = type_of<decltype(struct_type_().x)>();

#define MTS_PY_ENOKI_STRUCT(cls, Type, ...)                \
    py::dict fields;                                       \
    using struct_type_ = Type;                             \
    ENOKI_MAP(MTS_PY_ENOKI_STRUCT_BIND_FIELD, __VA_ARGS__) \
    cls.attr("ENOKI_STRUCT") = fields;

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

#define MTS_PY_DECLARE(Name) extern void python_export_##Name(py::module_ &m)
#define MTS_PY_EXPORT(Name) void python_export_##Name(py::module_ &m)
#define MTS_PY_IMPORT(Name) python_export_##Name(m)
#define MTS_PY_IMPORT_SUBMODULE(Name) python_export_##Name(Name)

#define MTS_MODULE_NAME_1(lib, variant) lib##_##variant##_ext
#define MTS_MODULE_NAME(lib, variant) MTS_MODULE_NAME_1(lib, variant)

#define MTS_PY_IMPORT_TYPES(...)                                                                   \
    using Float    = MTS_VARIANT_FLOAT;                                                            \
    using Spectrum = MTS_VARIANT_SPECTRUM;                                                         \
    MTS_IMPORT_TYPES(__VA_ARGS__)                                                                  \
    MTS_IMPORT_OBJECT_TYPES()

inline py::module_ create_submodule(py::module_ &m, const char *name) {
    std::string full_name = std::string(PyModule_GetName(m.ptr())) + "." + name;
    py::module_ module = py::reinterpret_steal<py::module_>(PyModule_New(full_name.c_str()));
    m.attr(name) = module;
    return module;
}

template <typename Array> void bind_enoki_ptr_array(py::class_<Array> &cls) {
    using Type = std::decay_t<std::remove_pointer_t<ek::value_t<Array>>>;

    cls.attr("eq_") = py::none();
    cls.attr("neq_") = py::none();
    cls.attr("index") = py::none();

    cls.def(py::init<>())
       .def(py::init<Type *>())
       .def("entry_", [](Array a, size_t i) { return a.entry(i); })
       .def("eq_", &Array::eq_)
       .def("neq_", &Array::neq_)
       .def("__len__", [](Array a) { return a.size(); });

    cls.attr("zero_") = py::cpp_function(&Array::zero_);
    cls.attr("Type") = VarType::Pointer;
    cls.attr("Value") = py::type::of<Type>();
    cls.attr("IsJIT") = ek::is_jit_array_v<Array>;
    cls.attr("Depth") = ek::array_depth_v<Array>;
    cls.attr("Size")  = ek::array_size_v<Array>;
    cls.attr("IsDiff") = false;
    cls.attr("IsQuaternion") = false;
    cls.attr("IsComplex") = false;
    cls.attr("IsMatrix") = false;
    cls.attr("IsEnoki") = true;

    if constexpr (ek::is_jit_array_v<Array>)
        cls.def("index", [](Array a) { return a.index(); });
}

#define MTS_PY_CHECK_ALIAS(Type, Name)                \
    if (auto h = get_type_handle<Type>(); h) {        \
        m.attr(Name) = h;                             \
    }                                                 \
    else

