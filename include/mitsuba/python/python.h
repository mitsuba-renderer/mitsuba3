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
#include <drjit/packet.h>
#include <drjit-core/jit.h>

#include "docstr.h"

PYBIND11_DECLARE_HOLDER_TYPE(T, mitsuba::ref<T>, true);

#define D(...) DOC(mitsuba, __VA_ARGS__)

/// Shorthand notation for defining a data structure
#define MI_PY_STRUCT(Name, ...) \
    py::class_<Name>(m, #Name, D(Name), ##__VA_ARGS__)

/// Shorthand notation for defining a Mitsuba class deriving from a base class
#define MI_PY_CLASS(Name, Base, ...) \
    py::class_<Name, Base, ref<Name>>(m, #Name, D(Name), ##__VA_ARGS__)

/// Shorthand notation for defining a Mitsuba class that can be extended in Python
#define MI_PY_TRAMPOLINE_CLASS(Trampoline, Name, Base, ...) \
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
#define MI_PY_REGISTER_OBJECT(Function, Name)                                  \
    m.def(                                                                     \
        Function,                                                              \
        [](const std::string &name,                                            \
           std::function<py::object(const Properties *)> &constructor) {       \
            auto variant = ::mitsuba::detail::get_variant<Float, Spectrum>();  \
            (void) new Class(                                                  \
                name, #Name, variant,                                          \
                [=](const Properties &p) {                                     \
                    /* The thread-local python variant information might not
                    have been set on this thread */                            \
                    py::gil_scoped_acquire gil;                                \
                    py::module mi = py::module::import("mitsuba");             \
                    py::object py_variant = mi.attr("variant")();              \
                    if (py_variant.is(py::none()) ||                           \
                        py_variant.cast<std::string>() != variant)             \
                        mi.attr("set_variant")(variant);                       \
                                                                               \
                    py::object o;                                              \
                    {                                                          \
                        py::gil_scoped_release release;                        \
                        o = constructor(&p);                                   \
                    }                                                          \
                                                                               \
                    ref<Name> o2 = o.cast<Name*>();                            \
                    o.release();                                               \
                    return o2;                                                 \
                },                                                             \
                nullptr);                                                      \
            PluginManager::instance()->register_python_plugin(name, variant);  \
        });

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

#define MI_PY_DRJIT_STRUCT_BIND_FIELD(x) \
    fields[#x] = type_of<decltype(struct_type_().x)>();

#define MI_PY_DRJIT_STRUCT(cls, Type, ...)                                    \
    py::dict fields;                                                           \
    using struct_type_ = Type;                                                 \
    DRJIT_MAP(MI_PY_DRJIT_STRUCT_BIND_FIELD, __VA_ARGS__)                     \
    cls.attr("DRJIT_STRUCT") = fields;                                         \
    cls.def("assign", [](Type &a, const Type &b) {                             \
        if (&a != &b)                                                          \
            a = b;                                                             \
    });                                                                        \
    cls.def("__setitem__",                                                     \
            [](Type &type, const Mask &mask, const Type &value) {              \
                type = dr::select(mask, value, type);                          \
            });

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

#define MI_PY_DECLARE(Name) extern void python_export_##Name(py::module_ &m)
#define MI_PY_EXPORT(Name) void python_export_##Name(py::module_ &m)
#define MI_PY_IMPORT(Name) python_export_##Name(m)
#define MI_PY_IMPORT_SUBMODULE(Name) python_export_##Name(Name)

#define MI_MODULE_NAME_1(lib, variant) lib##_##variant##_ext
#define MI_MODULE_NAME(lib, variant) MI_MODULE_NAME_1(lib, variant)

#define MI_PY_IMPORT_TYPES(...)                                                                   \
    using Float    = MI_VARIANT_FLOAT;                                                            \
    using Spectrum = MI_VARIANT_SPECTRUM;                                                         \
    MI_IMPORT_TYPES(__VA_ARGS__)                                                                  \
    MI_IMPORT_OBJECT_TYPES()

inline py::module_ create_submodule(py::module_ &m, const char *name) {
    std::string full_name = std::string(PyModule_GetName(m.ptr())) + "." + name;
    py::module_ module = py::reinterpret_steal<py::module_>(PyModule_New(full_name.c_str()));
    m.attr(name) = module;
    return module;
}

template <typename Array> void bind_drjit_ptr_array(py::class_<Array> &cls) {
    using Type = std::decay_t<std::remove_pointer_t<dr::value_t<Array>>>;
    using UInt32 = dr::uint32_array_t<Array>;
    using Mask = dr::mask_t<UInt32>;

    cls.attr("eq_") = py::none();
    cls.attr("neq_") = py::none();
    cls.attr("gather_") = py::none();
    cls.attr("select_") = py::none();
    cls.attr("set_label_") = py::none();
    cls.attr("label_") = py::none();
    cls.attr("index") = py::none();
    cls.attr("assign") = py::none();

    cls.def(py::init<>())
       .def(py::init<Type *>())
       .def("assign", [](Array &a, const Array &b) {
           if (&a != &b)
               a = b;
       })
       .def("entry_", [](const Array &a, size_t i) { return a.entry(i); })
       .def("eq_", &Array::eq_)
       .def("neq_", &Array::neq_)
       .def("__setitem__", [](Array &a, const dr::mask_t<Array> &m, const Array &b) {
           a[m] = b;
       })
       .def("__len__", [](Array a) { return a.size(); });

    cls.attr("zero_") = py::cpp_function(&Array::zero_);
    cls.attr("Type") = VarType::Pointer;
    cls.attr("Value") = py::type::of<Type>();
    cls.attr("MaskType") = py::type::of<Mask>();
    cls.attr("IsScalar") = false;
    cls.attr("IsJIT") = dr::is_jit_v<Array>;
    cls.attr("IsLLVM") = dr::is_llvm_v<Array>;
    cls.attr("IsCUDA") = dr::is_cuda_v<Array>;
    cls.attr("Depth") = dr::array_depth_v<Array>;
    cls.attr("Size")  = dr::array_size_v<Array>;
    cls.attr("IsDiff") = false;
    cls.attr("IsQuaternion") = false;
    cls.attr("IsComplex") = false;
    cls.attr("IsMatrix") = false;
    cls.attr("IsTensor") = false;
    cls.attr("IsDrJit") = true;
    cls.attr("Prefix") = "Array";
    cls.attr("Shape") = py::make_tuple(dr::Dynamic);

    if constexpr (dr::is_jit_v<Array>) {
        cls.def_property_readonly("index", &Array::index);
        cls.def("label_", [](const Array &a) { return a.label_(); });
        cls.def("set_label_", [](Array &a, const char *label) { a.set_label_(label); });
        cls.def("set_index_", [](Array &a, uint32_t index) { *dr::detach(a).index_ptr() = index; });
    }

    cls.def_static("gather_", [](const Array &source, const UInt32 &index,
                                 const Mask &mask, bool permute) {
        if (permute)
            return dr::gather<Array, true>(source, index, mask);
        else
            return dr::gather<Array, false>(source, index, mask);
    }, "source"_a, "index"_a, "mask"_a, "permute"_a=false);

    cls.def_static("select_",
                   [](const Mask &m, const Array &t, const Array &f) {
                       return dr::select(m, t, f);
                   });

    cls.def_static("reinterpret_array_",
                   [](const dr::uint32_array_t<Array> &a) {
                       return dr::reinterpret_array<Array>(a);
                   });

    if constexpr (dr::is_jit_v<Array>) {
        py::module module = py::module::import("drjit");
        if constexpr (dr::is_cuda_v<Array>)
            module = module.attr("cuda");
        else
            module = module.attr("llvm");
        if constexpr (dr::is_diff_v<Array>)
            module = module.attr("ad");
        using UInt32 = dr::uint32_array_t<Array>;
        py::class_<UInt32> uint32_obj = py::class_<UInt32>(module.attr("UInt32"));
        uint32_obj.def_static("reinterpret_array_",
                [](const Array &a) {
                    return dr::reinterpret_array<UInt32>(a);
                });
    }

    if constexpr (dr::is_jit_v<Array>) {
        cls.def_static("registry_get_max_", []() {
            return jit_registry_get_max(dr::backend_v<Array>, Array::CallSupport::Domain);
        });

        cls.def_static("registry_get_ptr_", [](uint32_t i) -> py::object {
            void *ptr = jit_registry_get_ptr(dr::backend_v<Array>, Array::CallSupport::Domain, i);
            if (ptr) {
                py::object mitsuba_ext = py::module::import("mitsuba.mitsuba_ext");
                using Caster = py::object(*)(mitsuba::Object *);
                Caster cast_object = (Caster) (void *)((py::capsule) mitsuba_ext.attr("cast_object"));
                return cast_object((mitsuba::Object *) ptr);
            } else {
                return py::none();
            }
        });
    }
}

#define MI_PY_CHECK_ALIAS(Type, Name)                \
    if (auto h = get_type_handle<Type>(); h) {        \
        m.attr(Name) = h;                             \
    }                                                 \
    else

#define MI_PY_DECLARE_ENUM_OPERATORS(Type, m)        \
    m.def(py::self == py::self)                       \
     .def(py::self | py::self)                        \
     .def(int() | py::self)                           \
     .def(py::self &py::self)                         \
     .def(int() & py::self)                           \
     .def(+py::self)                                  \
     .def(~py::self)                                  \
     .def("__pos__", [](const Type &f) {              \
            return static_cast<uint32_t>(f);          \
        }, py::is_operator());

