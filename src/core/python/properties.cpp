#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>

#define SET_ITEM_BINDING(Type, ...)                                            \
    def("__setitem__",                                                         \
        [](Properties &p, std::string_view key, const Type &value) {           \
            p.set(key, value, false);                                          \
        }, ##__VA_ARGS__)

// Python-specific storage helper for Any objects
class PythonObjectStorage : public Any::Base {
    nb::handle m_obj;
    const std::type_info *m_cpp_type_info;
    void *m_cpp_ptr;

public:
    PythonObjectStorage(nb::handle obj)
        : m_obj(obj),
          m_cpp_type_info(&nb::type_info(obj.type())),
          m_cpp_ptr(nb::inst_ptr<void>(obj)) {
        m_obj.inc_ref();
    }

    ~PythonObjectStorage() {
        nb::gil_scoped_acquire gil;
        m_obj.dec_ref();
    }

    const std::type_info &type() const override {
        return *m_cpp_type_info;
    }

    void *ptr() override {
        return m_cpp_ptr;
    }
};

extern nb::object cast_object(Object *o);

static nb::object get_property(const Properties& p, std::string_view key) {
    using namespace mitsuba;
    switch (p.type(key)) {
        case Properties::Type::Bool:        return nb::cast(p.get<bool>(key));
        case Properties::Type::Integer:     return nb::cast(p.get<int64_t>(key));
        case Properties::Type::Float:       return nb::cast(p.get<double>(key));
        case Properties::Type::String:      return nb::cast(p.get<std::string>(key));
        case Properties::Type::Reference:   return nb::cast(p.get<Properties::Reference>(key));
        case Properties::Type::Vector:      return nb::cast(p.get<dr::Array<double, 3>>(key));
        case Properties::Type::Color:       return nb::cast(p.get<Color<double, 3>>(key));
        case Properties::Type::Transform:   return nb::cast(p.get<Transform<Point<double, 4>>>(key));
        case Properties::Type::Object:      return cast_object(p.get<ref<Object>>(key));
        case Properties::Type::Any: {
            const Any &any_val = p.get<Any>(key);

            // Use nanobind's internal API to find an existing Python object
            // from the C++ pointer and type embedded within the Any instance.
            nb::handle py_obj = nanobind::detail::nb_type_put(
                &any_val.type(), const_cast<void*>(any_val.data()),
                nanobind::rv_policy::none, nullptr);

            if (!py_obj.is_valid())
                Throw("Property \"%s\" is not a known Python object.", key);

            return nb::steal(py_obj);
        }

        default:
            Throw("Unsupported property type");
    }
}

MI_PY_EXPORT(Properties) {
    using namespace mitsuba;
    using ScalarColor3f = Color<float, 3>;
    using ScalarColor3d = Color<double, 3>;
    using ScalarArray3f = dr::Array<float, 3>;
    using ScalarArray3d = dr::Array<double, 3>;
    using ScalarTransform3f = Transform<Point<float, 3>>;
    using ScalarTransform3d = Transform<Point<double, 3>>;
    using ScalarTransform4f = Transform<Point<float, 4>>;
    using ScalarTransform4d = Transform<Point<double, 4>>;

    auto properties_class = nb::class_<Properties>(m, "Properties")
        // Constructors
        .def(nb::init<>(), D(Properties, Properties))
        .def(nb::init<const std::string &>(), D(Properties, Properties, 2))
        .def(nb::init<const Properties &>(), D(Properties, Properties, 3))

        // Methods
        .def("has_property", [](const Properties& p, std::string_view key) {
            // Issue deprecation warning
            PyErr_WarnEx(PyExc_DeprecationWarning, 
                        "has_property() is deprecated, use 'key in props' instead", 1);
            return p.has_property(key);
        }, "key"_a, "Deprecated: use 'key in props' instead")
        .def("remove_property", [](Properties& p, std::string_view key) {
            // Issue deprecation warning
            PyErr_WarnEx(PyExc_DeprecationWarning, 
                        "remove_property() is deprecated, use 'del props[key]' instead", 1);
            return p.remove_property(key);
        }, "key"_a, "Deprecated: use 'del props[key]' instead")
        .def("property_names", [](const Properties& p) {
            // Issue deprecation warning
            PyErr_WarnEx(PyExc_DeprecationWarning, 
                        "property_names() is deprecated, use 'props.keys()' instead", 1);
            return p.property_names();
        }, "Deprecated: use 'props.keys()' instead")
        .def_method(Properties, mark_queried)
        .def_method(Properties, was_queried)
        .def_method(Properties, plugin_name)
        .def_method(Properties, set_plugin_name)
        .def_method(Properties, id)
        .def_method(Properties, set_id)
        .def_method(Properties, unqueried)
        .def_method(Properties, merge)
        .def_method(Properties, type)

        .def("references", [](const Properties& p) {
            std::vector<std::pair<std::string, std::string>> res;
            for(auto [name, ref]: p.references())
                res.push_back({name, (std::string) ref});
            return res;
        })
        .def("objects", [](const Properties& p, bool mark_queried) {
            return p.objects(mark_queried);
        }, "mark_queried"_a = true, "Return all object properties")

        // Getters & setters: used as if it were a simple map
        .SET_ITEM_BINDING(bool)
        .SET_ITEM_BINDING(long)
        .SET_ITEM_BINDING(double)
        .SET_ITEM_BINDING(std::string)
        .SET_ITEM_BINDING(ScalarArray3f)
        .SET_ITEM_BINDING(ScalarArray3d)
        .SET_ITEM_BINDING(ScalarColor3f)
        .SET_ITEM_BINDING(ScalarColor3d)
        .SET_ITEM_BINDING(ScalarTransform3f)
        .SET_ITEM_BINDING(ScalarTransform3d)
        .SET_ITEM_BINDING(ScalarTransform4f)
        .SET_ITEM_BINDING(ScalarTransform4d)
        .def("__setitem__",
            [](Properties &p, std::string_view key, Object *value) {
                p.set(key, ref<Object>(value), false);
            })
        .def("__setitem__",
            [](Properties &p, std::string_view key, nb::fallback value) {
                if (nb::inst_check(value)) {
                    // Create PythonObjectStorage and explicitly cast to Any::Base*
                    // to ensure the correct Any constructor is used
                    Any::Base *storage = new PythonObjectStorage(value);
                    p.set(key, Any(storage), false);
                } else {
                    Throw("Only nanobind-bound C++ objects are supported for generic storage");
                }
            })
        .def("__getitem__", [](const Properties& p, const std::string &key) {
            return get_property(p, key);
        }, "key"_a, "Retrieve an existing property given its name")
        .def("get", [](const Properties& p, std::string_view key, const nb::object &def_val) {
            if (p.has_property(key))
                return get_property(p, key);
            else
                return def_val;
        },
        "key"_a, "def_value"_a = nb::none(),
        D(Properties, get))
        .def("__contains__", [](const Properties& p, std::string_view key) {
            return p.has_property(key);
        })
        .def("__delitem__", [](Properties& p, std::string_view key) {
            return p.remove_property(key);
        })
        .def("keys", [](const Properties& p) {
            return p.property_names();
        }, "Return a list of property names")
        .def("items", [](const Properties& p) {
            nb::list result;
            for (const std::string &key : p.property_names()) {
                nb::tuple item = nb::make_tuple(key, get_property(p, key));
                result.append(item);
            }
            return result;
        }, "Return a list of (key, value) tuples")
        .def("as_string",
            [](const Properties& p, const std::string& key) { return p.as_string(key); },
            "key"_a,
            D(Properties, as_string))
        // Operators
        .def(nb::self == nb::self, D(Properties, operator_eq))
        .def(nb::self != nb::self)
        .def_repr(Properties);

    // Add Type enum as a nested attribute of Properties
    properties_class.attr("Type") = nb::enum_<Properties::Type>(properties_class, "Type")
        .value("Bool", Properties::Type::Bool)
        .value("Integer", Properties::Type::Integer)
        .value("Float", Properties::Type::Float)
        .value("Vector", Properties::Type::Vector)
        .value("Transform", Properties::Type::Transform)
        .value("Color", Properties::Type::Color)
        .value("String", Properties::Type::String)
        .value("Reference", Properties::Type::Reference)
        .value("Object", Properties::Type::Object)
        .value("Any", Properties::Type::Any);
}
