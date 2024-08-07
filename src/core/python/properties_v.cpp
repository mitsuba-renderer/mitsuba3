#include <drjit/tensor.h>
#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>

#include <nanobind/ndarray.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>

using Caster = nb::object(*)(mitsuba::Object *);
extern Caster cast_object;

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wduplicate-decl-specifier" // warning: duplicate 'const' declaration specifier
#endif

#define SET_ITEM_BINDING(Name, Type, ...)                              \
    def("__setitem__", [](PropertiesV & p,                             \
                          const std::string &key, const Type &value) { \
        p.set_##Name(key, value, false);                               \
    }, D(Properties, set_##Name), ##__VA_ARGS__)

#define GET_ITEM_DEFAULT_BINDING(Name, DName, Type)         \
    def(#Name, [](PropertiesV& p, const std::string &key,   \
                  const Type &def_val) {                    \
        return nb::cast(p.Name(key, def_val));              \
    }, D(Properties, DName, 2))


template <typename Float>
nb::object properties_get(const PropertiesV<Float>& p, const std::string &key) {
    using PFloat = Properties::Float;
    using TensorHandle = typename Properties::TensorHandle;
    using TensorXf = dr::Tensor<mitsuba::DynamicBuffer<Float>>;

    // We need to ask for type information to return the right cast
    auto type = p.type(key);
    if (type == Properties::Type::Bool)
        return nb::cast(p.template get<bool>(key));
    else if (type == Properties::Type::Long)
        return nb::cast(p.template get<int64_t>(key));
    else if (type == Properties::Type::Float)
        return nb::cast(p.template get<PFloat>(key));
    else if (type == Properties::Type::String)
        return nb::cast(p.string(key));
    else if (type == Properties::Type::NamedReference)
        return nb::cast((std::string) p.named_reference(key));
    else if (type == Properties::Type::Color)
        return nb::cast(p. template get<Color<PFloat, 3>>(key));
    else if (type == Properties::Type::Array3f)
        return nb::cast(p.template get<dr::Array<PFloat, 3>>(key));
    else if (type == Properties::Type::Transform3f)
        return nb::cast(p.template get<Transform<Point<PFloat, 3>>>(key));
    else if (type == Properties::Type::Transform4f)
        return nb::cast(p.template get<Transform<Point<PFloat, 4>>>(key));
    // else if (type == Properties::Type::AnimatedTransform)
        // return nb::cast(p.animated_transform(key));
    else if (type == Properties::Type::Tensor)
        return nb::cast(*(p.template tensor<TensorXf>(key)));
    else if (type == Properties::Type::Object)
        return cast_object((ref<Object>)p.object(key));
    else if (type == Properties::Type::Pointer)
        return nb::cast(const_cast<void*>(p.pointer(key)));
    else
        throw std::runtime_error("Unsupported property type");
}

MI_PY_EXPORT(Properties) {
    MI_PY_CHECK_ALIAS(Properties, "_Properties") {
        auto p = nb::class_<Properties>(m, "_Properties");
    }

    using Float = MI_VARIANT_FLOAT;
    using Color3f = Color<float, 3>;
    using Color3d = Color<double, 3>;
    using TensorHandle = typename Properties::TensorHandle;
    using TensorXf = dr::Tensor<mitsuba::DynamicBuffer<Float>>;
    using PropertiesV = PropertiesV<Float>;

    MI_PY_CHECK_ALIAS(PropertiesV, "Properties") {
        auto p = nb::class_<PropertiesV, Properties>(m, "Properties")
            // Constructors
            .def(nb::init<>(), D(Properties, Properties))
            .def(nb::init<const std::string &>(), D(Properties, Properties, 2))
            .def(nb::init<const PropertiesV &>(), D(Properties, Properties, 3))
            // Methods
            .def_method(Properties, has_property)
            .def_method(Properties, remove_property)
            .def_method(Properties, mark_queried)
            .def_method(Properties, was_queried)
            .def_method(Properties, plugin_name)
            .def_method(Properties, set_plugin_name)
            .def_method(Properties, id)
            .def_method(Properties, set_id)
            .def_method(Properties, copy_attribute)
            .def_method(Properties, property_names)
            .def_method(Properties, unqueried)
            .def_method(Properties, merge)
            .def_method(Properties, type)
            .def("named_references", [](const PropertiesV& p){
                std::vector<std::pair<std::string, std::string>> res;
                for(auto [name, ref]: p.named_references())
                    res.push_back({name, (std::string) ref});
                return res;
            })
            // Getters & setters: used as if it were a simple map
            .def("__setitem__", [](Properties& p,
                                   const std::string &key,
                                   const nb::float_ &value) {
                p.set_float(key, (double) value, false);
            }, D(Properties, set_float))
            .SET_ITEM_BINDING(bool, bool)
            .SET_ITEM_BINDING(long, int64_t)
            .SET_ITEM_BINDING(string, std::string)
            .SET_ITEM_BINDING(color, Color3f, nb::arg(), nb::arg().noconvert())
            .SET_ITEM_BINDING(color, Color3d, nb::arg(), nb::arg().noconvert())
            .SET_ITEM_BINDING(array3f, typename Properties::Array3f)
            .SET_ITEM_BINDING(transform3f, typename Properties::Transform3f)
            .SET_ITEM_BINDING(transform, typename Properties::Transform4f)
            // .SET_ITEM_BINDING(animated_transform, ref<AnimatedTransform>)
            .SET_ITEM_BINDING(object, ref<Object>)
            .GET_ITEM_DEFAULT_BINDING(string, string, std::string)
            // .GET_ITEM_DEFAULT_BINDING(animated_transform, animated_transform, ref<AnimatedTransform>)
            .def("__setitem__",[](PropertiesV& p, const std::string &key, const TensorXf &value) {
                p.set_tensor_handle(key, TensorHandle(std::make_shared<TensorXf>(value)), false);
            })
            .def("__getitem__", [](const PropertiesV& p, const std::string &key) {
                return properties_get(p, key);
            }, "key"_a, "Retrieve an existing property given its name")
            .def("get", [](const PropertiesV& p, const std::string &key,
                           const nb::object &def_val) {
                if (p.has_property(key))
                    return properties_get(p, key);
                else
                    return def_val;
            },
            "key"_a, "def_value"_a = nb::none(),
            "Return the value for the specified key it exists, otherwise return default value")
            .def("__contains__", [](const PropertiesV& p, const std::string &key) {
                return p.has_property(key);
            })
            .def("__delitem__", [](PropertiesV& p, const std::string &key) {
                return p.remove_property(key);
            })
            .def("as_string",
                nb::overload_cast<const std::string&>(&Properties::as_string, nb::const_),
                D(Properties, as_string))
            // Operators
            .def(nb::self == nb::self, D(Properties, operator_eq))
            .def(nb::self != nb::self, D(Properties, operator_ne))
            .def_repr(PropertiesV);

        // FIXME: Binding this enumeration leaks. Defining an internal enum to
        // an arbitrary class is fine, this seems to be specifically an issue
        // with defining an internal enum in Properties
        //nb::enum_<Properties::Type>(p, "Type");
            //.value("Bool",              Properties::Type::Bool,           D(Properties, Type, Bool))
            //.value("Long",              Properties::Type::Long,           D(Properties, Type, Long))
            //.value("Float",             Properties::Type::Float,          D(Properties, Type, Float))
            //.value("Array3f",           Properties::Type::Array3f,        D(Properties, Type, Array3f))
            //.value("Transform3f",       Properties::Type::Transform3f,    D(Properties, Type, Transform3f))
            //.value("Transform4f",       Properties::Type::Transform4f,    D(Properties, Type, Transform4f))
            //// .value("AnimatedTransform", Properties::Type::AnimatedTransform, D(Properties, Type, AnimatedTransform))
            //.value("TensorHandle",      Properties::Type::Tensor,         D(Properties, Type, Tensor))
            //.value("Color",             Properties::Type::Color,          D(Properties, Type, Color))
            //.value("String",            Properties::Type::String,         D(Properties, Type, String))
            //.value("NamedReference",    Properties::Type::NamedReference, D(Properties, Type, NamedReference))
            //.value("Object",            Properties::Type::Object,         D(Properties, Type, Object))
            //.value("Pointer",           Properties::Type::Pointer,        D(Properties, Type, Pointer))
            //.export_values();
    }
}
