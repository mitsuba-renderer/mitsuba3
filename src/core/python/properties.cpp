#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>
#include "any.h"

extern nb::object cast_object(Object *o);

// Template function for texture retrieval bindings
template <bool Emissive, bool Unbounded>
static nb::object get_texture_binding(const Properties& p, std::string_view name) {
    nb::object mi = nb::module_::import_("mitsuba");
    nb::str variant = nb::cast<nb::str>(mi.attr("variant")());
    return cast_object(p.get_texture_impl(name, variant.c_str(), Emissive, Unbounded).get());
}

template <bool Emissive, bool Unbounded>
static nb::object get_texture_binding_default(const Properties& p, std::string_view name, double def) {
    nb::object mi = nb::module_::import_("mitsuba");
    nb::str variant = nb::cast<nb::str>(mi.attr("variant")());
    return cast_object(p.get_texture_impl(name, variant.c_str(), Emissive, Unbounded, def).get());
}

using ScalarColor3d = Color<double, 3>;
using ScalarColor3f = Color<float, 3>;
using ScalarArray3d = dr::Array<double, 3>;
using ScalarAffineTransform3f = AffineTransform<Point<float, 3>>;
using ScalarAffineTransform3d = AffineTransform<Point<double, 3>>;
using ScalarAffineTransform4f = AffineTransform<Point<float, 4>>;
using ScalarAffineTransform4d = AffineTransform<Point<double, 4>>;

#define SET_ITEM_BINDING(Type, ...)                                            \
    def("__setitem__",                                                         \
        [](Properties &p, std::string_view key, const Type &value) {           \
            p.set(key, value, false);                                          \
        }, ##__VA_ARGS__)


extern nb::object cast_object(Object *o);

static nb::object get_property(const Properties& p, std::string_view key) {
    switch (p.type(key)) {
        case Properties::Type::Bool:              return nb::cast(p.get<bool>(key));
        case Properties::Type::Integer:           return nb::cast(p.get<int64_t>(key));
        case Properties::Type::Float:             return nb::cast(p.get<double>(key));
        case Properties::Type::String:            return nb::cast(p.get<std::string>(key));
        case Properties::Type::Reference:         return nb::cast(p.get<Properties::Reference>(key));
        case Properties::Type::ResolvedReference: return nb::cast(p.get<Properties::ResolvedReference>(key));
        case Properties::Type::Vector:            return nb::cast(p.get<dr::Array<double, 3>>(key));
        case Properties::Type::Color:             return nb::cast(p.get<Color<double, 3>>(key));
        case Properties::Type::Transform:         return nb::cast(p.get<AffineTransform<Point<double, 4>>>(key));
        case Properties::Type::Object:            return cast_object(p.get<ref<Object>>(key).get());
        case Properties::Type::Spectrum:          return nb::cast(p.get<Properties::Spectrum>(key));
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
    auto properties_class = nb::class_<Properties>(m, "Properties")
        // Constructors
        .def(nb::init<>(), D(Properties, Properties))
        .def(nb::init<std::string_view>(), D(Properties, Properties, 2))
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
            std::vector<std::string> names;
            for (const auto &key : p)
                names.push_back(std::string(key.name()));
            return names;
        }, "Deprecated: use 'props.keys()' instead")
        .def("mark_queried",
             nb::overload_cast<std::string_view, bool>(&Properties::mark_queried, nb::const_),
             "key"_a, "value"_a = true,
             D(Properties, mark_queried))
        .def_method(Properties, was_queried)
        .def_method(Properties, plugin_name)
        .def_method(Properties, set_plugin_name)
        .def_method(Properties, id)
        .def_method(Properties, set_id)
        .def_method(Properties, unqueried)
        .def_method(Properties, merge)
        .def_method(Properties, type)

        .def("references", [](const Properties& p) -> nb::typed<nb::list, std::pair<std::string_view, std::string_view>> {
            nb::list result;
            for (const auto &prop : p.filter(Properties::Type::Reference))
                result.append(nb::make_tuple(prop.name(), p.get<Properties::Reference>(prop.name()).id()));
            return result;
        }, "Return all reference properties")

        .def("objects", [](const Properties& p, bool mark_queried) -> nb::typed<nb::list, std::pair<std::string_view, ref<Object>>> {
            nb::list result;
            for (const auto &prop : p.filter(Properties::Type::Object)) {
                ref<Object> obj = p.get<ref<Object>>(prop.name());
                result.append(nb::make_tuple(prop.name(), cast_object(obj.get())));
                if (!mark_queried)
                    p.mark_queried(prop.name(), false);
            }
            return result;
        }, "mark_queried"_a = true, "Return all object properties")

        // Getters & setters: used as if it were a simple map
        .SET_ITEM_BINDING(bool)
        .SET_ITEM_BINDING(int64_t)
        .SET_ITEM_BINDING(double)
        .SET_ITEM_BINDING(std::string)
        .SET_ITEM_BINDING(ScalarArray3d)
        .SET_ITEM_BINDING(ScalarColor3f)
        .SET_ITEM_BINDING(ScalarColor3d)
        .SET_ITEM_BINDING(ScalarAffineTransform3f)
        .SET_ITEM_BINDING(ScalarAffineTransform3d)
        .SET_ITEM_BINDING(ScalarAffineTransform4f)
        .SET_ITEM_BINDING(ScalarAffineTransform4d)
        .SET_ITEM_BINDING(Properties::Spectrum)
        .SET_ITEM_BINDING(Properties::ResolvedReference)
        .SET_ITEM_BINDING(Properties::Reference)
        .def("__setitem__",
            [](Properties &p, std::string_view key, const fs::path &value) {
                p.set(key, value.string(), false);
            })
        .def("__setitem__",
            [](Properties &p, std::string_view key, Object *value) {
                p.set(key, ref<Object>(value), false);
            })
        // NumPy scalar handler
        .def("__setitem__",
            [](Properties &p, std::string_view key, nb::handle value) {
                if (!nb::hasattr(value.type(), "__module__"))
                    throw nb::next_overload();

                const char *mod =
                    nb::borrow<nb::str>(value.type().attr("__module__")).c_str();

                if (strcmp(mod, "numpy") == 0) {
                    nb::object np = nb::module_::import_("numpy");

                    if (nb::bool_(np.attr("isscalar")(value))) {
                        const char *dtype =
                            nb::borrow<nb::str>(value.attr("dtype").attr("kind")).c_str();

                        if (dtype[0] == 'b') {
                            p.set(key, nb::cast<bool>(value.attr("item")()), false);
                            return;
                        } else if (dtype[0] == 'i' || dtype[0] == 'u') {
                            p.set(key, nb::cast<int64_t>(value), false);
                            return;
                        } else if (dtype[0] == 'f') {
                            p.set(key, nb::cast<double>(value), false);
                            return;
                        } else {
                            Throw(
                                "Properties.__setitem__(): numpy scalars with "
                                "a '%s' data type are not supported!", dtype);
                        }
                    }
                }

                throw nb::next_overload();
            })
        .def("__setitem__",
            [](Properties &p, std::string_view key, nb::fallback value) {
                try {
                    p.set(key, any_wrap(value), false);
                } catch (...) {
                    Throw("Properties.__setitem__(): could not assign an "
                          "object of type '%s' to key '%s', as the value was "
                          "not convertible any of the supported property types "
                          "(e.g. int/float/color/vector/...). It also could be "
                          "stored as an property of type 'any', as the "
                          "underlying type was not exposed using the nanobind "
                          "library. Did you mean to cast this value into a "
                          "Dr.Jit array before assigning it?",
                          nb::inst_name(value).c_str(), key);
                }
            })
        .def("__getitem__", [](const Properties& p, std::string_view key) {
            return get_property(p, key);
        }, "key"_a, "Retrieve an existing property given its name")
        .def("get", [](const Properties& p, std::string_view key, nb::handle def_val) {
            if (p.has_property(key))
                return get_property(p, key);
            else
                return nb::borrow(def_val);
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
            nb::list names;
            for (const auto &key : p)
                names.append(key.name());
            return names;
        }, "Return a list of property names")
        .def("__iter__", [](nb::handle self) {
            return nb::iter(self.attr("keys")());
        }, "Return a list of property names")
        .def("items", [](const Properties& p) {
            nb::list result;
            for (const auto &key : p)
                result.append(nb::make_tuple(key.name(), get_property(p, key.name())));
            return result;
        }, "Return a list of (key, value) tuples")
        .def("as_string",
            [](const Properties& p, std::string_view key) { return p.as_string(key); },
            "key"_a,
            D(Properties, as_string))

        // Texture retrieval methods
        .def("get_texture", &get_texture_binding<false, false>,
            "name"_a, "Retrieve a texture parameter")
        .def("get_texture", &get_texture_binding_default<false, false>,
            "name"_a, "default"_a, "Retrieve a texture parameter with default value")
        .def("get_emissive_texture", &get_texture_binding<true, false>,
            "name"_a, "Retrieve an emissive texture parameter")
        .def("get_emissive_texture", &get_texture_binding_default<true, false>,
            "name"_a, "default"_a, "Retrieve an emissive texture parameter with default value")
        .def("get_unbounded_texture", &get_texture_binding<false, true>,
            "name"_a, "Retrieve an unbounded texture parameter")
        .def("get_unbounded_texture", &get_texture_binding_default<false, true>,
            "name"_a, "default"_a, "Retrieve an unbounded texture parameter with default value")

        // Operators
        .def(nb::self == nb::self, D(Properties, operator_eq))
        .def(nb::self != nb::self)
        .def_repr(Properties);

    // Add Type enum as a nested attribute of Properties
    properties_class.attr("Type") = nb::enum_<Properties::Type>(properties_class, "Type")
        .value("Bool", Properties::Type::Bool, D(Properties, Type, Bool))
        .value("Integer", Properties::Type::Integer, D(Properties, Type, Integer))
        .value("Float", Properties::Type::Float, D(Properties, Type, Float))
        .value("Vector", Properties::Type::Vector, D(Properties, Type, Vector))
        .value("Transform", Properties::Type::Transform, D(Properties, Type, Transform))
        .value("Color", Properties::Type::Color, D(Properties, Type, Color))
        .value("Spectrum", Properties::Type::Spectrum, D(Properties, Type, Spectrum))
        .value("String", Properties::Type::String, D(Properties, Type, String))
        .value("Reference", Properties::Type::Reference, D(Properties, Type, Reference))
        .value("ResolvedReference", Properties::Type::ResolvedReference, D(Properties, Type, ResolvedReference))
        .value("Object", Properties::Type::Object, D(Properties, Type, Object))
        .value("Any", Properties::Type::Any, D(Properties, Type, Any));

    // Bind Reference class
    nb::class_<Properties::Reference>(properties_class, "Reference")
        .def(nb::init<std::string_view>(), "name"_a)
        .def("id", &Properties::Reference::id)
        .def("__eq__", &Properties::Reference::operator==)
        .def("__ne__", &Properties::Reference::operator!=)
        .def("__repr__", [](const Properties::Reference &r) {
            return tfm::format("Reference[%s]", r.id());
        });

    // Bind ResolvedReference class
    nb::class_<Properties::ResolvedReference>(properties_class, "ResolvedReference")
        .def(nb::init<size_t>(), "index"_a)
        .def("index", &Properties::ResolvedReference::index)
        .def("__eq__", &Properties::ResolvedReference::operator==)
        .def("__ne__", &Properties::ResolvedReference::operator!=)
        .def("__repr__", [](const Properties::ResolvedReference &r) {
            return tfm::format("ResolvedReference[%zu]", r.index());
        });

    // Bind Spectrum class
    nb::class_<Properties::Spectrum>(m, "Spectrum")
        .def(nb::init<double>(), "value"_a,
             "Construct a uniform spectrum with a single value")
        .def(nb::init<std::vector<double>, std::vector<double>>(),
             "wavelengths"_a, "values"_a,
             "Construct a spectrum from wavelength-value pairs")
        .def_ro("wavelengths", &Properties::Spectrum::wavelengths,
                "List of wavelengths (in nanometers)")
        .def_ro("values", &Properties::Spectrum::values,
                "List of corresponding values")
        .def("is_uniform", &Properties::Spectrum::is_uniform,
             "Check if this is a uniform spectrum (single value, no wavelengths)")
        .def("__eq__", &Properties::Spectrum::operator==)
        .def("__repr__", [](const Properties::Spectrum &s) {
            if (s.is_uniform())
                return tfm::format("Properties.Spectrum[uniform=%g]", s.values[0]);
            else
                return tfm::format("Properties.Spectrum[%zu samples]", s.wavelengths.size());
        });
}
