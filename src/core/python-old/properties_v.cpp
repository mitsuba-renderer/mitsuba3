#include <drjit/tensor.h>

#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>

using Caster = py::object(*)(mitsuba::Object *);
extern Caster cast_object;

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wduplicate-decl-specifier" // warning: duplicate 'const' declaration specifier
#endif

#define SET_ITEM_BINDING(Name, Type, ...)                              \
    def("__setitem__", [](Properties& p,                               \
                          const std::string &key, const Type &value) { \
        p.set_##Name(key, value, false);                               \
    }, D(Properties, set_##Name), ##__VA_ARGS__)


#define GET_ITEM_DEFAULT_BINDING(Name, DName, Type)       \
    def(#Name, [](Properties& p, const std::string &key,  \
                  const Type &def_val) {                  \
        return py::cast(p.Name(key, def_val));            \
    }, D(Properties, DName, 2))


py::object properties_get(const Properties& p, const std::string &key) {
    using PFloat = Properties::Float;
    using TensorHandle = typename Properties::TensorHandle;
    using Float = MI_VARIANT_FLOAT;
    using TensorXf = dr::Tensor<mitsuba::DynamicBuffer<Float>>;

    // We need to ask for type information to return the right cast
    auto type = p.type(key);
    if (type == Properties::Type::Bool)
        return py::cast(p.get<bool>(key));
    else if (type == Properties::Type::Long)
        return py::cast(p.get<int64_t>(key));
    else if (type == Properties::Type::Float)
        return py::cast(p.get<PFloat>(key));
    else if (type == Properties::Type::String)
        return py::cast(p.string(key));
    else if (type == Properties::Type::NamedReference)
        return py::cast((std::string) p.named_reference(key));
    else if (type == Properties::Type::Color)
        return py::cast(p.get<Color<PFloat, 3>>(key));
    else if (type == Properties::Type::Array3f)
        return py::cast(p.get<dr::Array<PFloat, 3>>(key));
    else if (type == Properties::Type::Transform3f)
        return py::cast(p.get<Transform<Point<PFloat, 3>>>(key));
    else if (type == Properties::Type::Transform4f)
        return py::cast(p.get<Transform<Point<PFloat, 4>>>(key));
    // else if (type == Properties::Type::AnimatedTransform)
        // return py::cast(p.animated_transform(key));
    else if (type == Properties::Type::Tensor)
        return py::cast(*(p.tensor<TensorXf>(key)));
    else if (type == Properties::Type::Object)
        return cast_object((ref<Object>)p.object(key));
    else if (type == Properties::Type::Pointer)
        return py::cast(p.pointer(key));
    else
        throw std::runtime_error("Unsupported property type");
}


MI_PY_EXPORT(Properties) {
    MI_PY_CHECK_ALIAS(Properties, "Properties") {
        using Color3f = Color<float, 3>;
        using Color3d = Color<double, 3>;
        using TensorHandle = typename Properties::TensorHandle;
        using Float = MI_VARIANT_FLOAT;
        using TensorXf = dr::Tensor<mitsuba::DynamicBuffer<Float>>;

        auto p = py::class_<Properties>(m, "Properties", D(Properties))
            // Constructors
            .def(py::init<>(), D(Properties, Properties))
            .def(py::init<const std::string &>(), D(Properties, Properties, 2))
            .def(py::init<const Properties &>(), D(Properties, Properties, 3))
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
            .def("named_references", [](const Properties& p){
                std::vector<std::pair<std::string, std::string>> res;
                for(auto [name, ref]: p.named_references())
                    res.push_back({name, (std::string) ref});
                return res;
            })
            // Getters & setters: used as if it were a simple map
            .SET_ITEM_BINDING(float, py::float_)
            .SET_ITEM_BINDING(bool, bool)
            .SET_ITEM_BINDING(long, int64_t)
            .SET_ITEM_BINDING(string, std::string)
            .SET_ITEM_BINDING(color, Color3f, py::arg(), py::arg().noconvert())
            .SET_ITEM_BINDING(color, Color3d, py::arg(), py::arg().noconvert())
            .SET_ITEM_BINDING(array3f, typename Properties::Array3f)
            .SET_ITEM_BINDING(transform3f, typename Properties::Transform3f)
            .SET_ITEM_BINDING(transform, typename Properties::Transform4f)
            // .SET_ITEM_BINDING(animated_transform, ref<AnimatedTransform>)
            .SET_ITEM_BINDING(object, ref<Object>)
            .GET_ITEM_DEFAULT_BINDING(string, string, std::string)
            // .GET_ITEM_DEFAULT_BINDING(animated_transform, animated_transform, ref<AnimatedTransform>)
            .def("__setitem__",[](Properties& p, const std::string &key, const TensorXf &value) {
                p.set_tensor_handle(key, TensorHandle(std::make_shared<TensorXf>(value)), false);
            })
            .def("__getitem__", [](const Properties& p, const std::string &key) {
                return properties_get(p, key);
            }, "key"_a, "Retrieve an existing property given its name")
            .def("get", [](const Properties& p, const std::string &key,
                           const py::object &def_val) {
                if (p.has_property(key))
                    return properties_get(p, key);
                else
                    return def_val;
            },
            "key"_a, "def_value"_a = py::none(),
            "Return the value for the specified key it exists, otherwise return default value")
            .def("__contains__", [](const Properties& p, const std::string &key) {
                return p.has_property(key);
            })
            .def("__delitem__", [](Properties& p, const std::string &key) {
                return p.remove_property(key);
            })
            .def("as_string",
                py::overload_cast<const std::string&>(&Properties::as_string, py::const_),
                D(Properties, as_string))
            // Operators
            .def(py::self == py::self, D(Properties, operator_eq))
            .def(py::self != py::self, D(Properties, operator_ne))
            .def_repr(Properties);

        py::enum_<Properties::Type>(p, "Type")
            .value("Bool",              Properties::Type::Bool)
            .value("Long",              Properties::Type::Long)
            .value("Float",             Properties::Type::Float)
            .value("Array3f",           Properties::Type::Array3f)
            .value("Transform3f",       Properties::Type::Transform3f)
            .value("Transform4f",       Properties::Type::Transform4f)
            // .value("AnimatedTransform", Properties::Type::AnimatedTransform)
            .value("TensorHandle",      Properties::Type::Tensor)
            .value("Color",             Properties::Type::Color)
            .value("String",            Properties::Type::String)
            .value("NamedReference",    Properties::Type::NamedReference)
            .value("Object",            Properties::Type::Object)
            .value("Pointer",           Properties::Type::Pointer);
    }
}
