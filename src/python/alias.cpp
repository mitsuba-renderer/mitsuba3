/**
 * Defines the `mitsuba_alias` Python module
 *
 * This module is reponsible for handling the extra aliasing provided by the
 * main mitsuba package. Its goal is to push many of the submodules symbols
 * to the top. It will namely crunch the variant-specifc submodules and the
 * python submodule, i.e:
 *     - `mitsuba.Float` is an alias of `mitsuba.variant.Float` where the
 *        variant is defined by `set_variant()`
 *     - `mitsuba.submodule.foo` is an alias of `mitsuba.python.submodule.foo`
 *
 * In addition, it is a direct alias for the entire `mitsuba_ext` package.
 * So, any `mitsuba_ext.symbol` has an alias in `mitsuba_alias.symbol`.
 *
 * Finally, the `mitsuba_alias` module registers itself in Python's
 * `sys.modules` as "mitsuba". This effectively allows it to masquerade as the
 * main Mitsuba package.
 */

#include <mitsuba/core/config.h>
#include <nanobind/nanobind.h>

namespace nb = nanobind;

/// Python module for `mitsuba_ext`
static nb::object mitsuba_ext{};
/// Python module for `mitsuba.python`
static nb::object mitsuba_python{};
/// Mapping from variant name to module
static nb::dict variant_modules{};
/// Current variant (string)
static nb::object curr_variant = nb::none();
/// Current variant (module)
static nb::object curr_variant_module = nb::none();
/// Cache for `__getattr__`, its keys are tuples: (variant_name, key)
static nb::dict cache{};

/// Return the Python module associated with a given variant name
static nb::object variant_module(nb::handle variant) {
    nb::object result = variant_modules[variant];
    if (!result.is_none())
        return result;

    nb::str module_name = nb::str("mitsuba.{}").format(variant);
    result = nb::module_::import_(module_name.c_str());
    variant_modules[variant] = nb::borrow<nb::module_>(result);

    return result;
}

/// Sets the variant
static void set_variant(nb::args args) {
    nb::object new_variant{};
    for (auto arg : args) {
        // Find the first valid & compiled variant in the arguments
        if (variant_modules.contains(arg)) {
            new_variant = nb::borrow(arg);
            break;
        }
    }

    if (!new_variant) {
        nb::object all_args(nb::str(", ").attr("join")(args));
        nb::object all_variants(nb::str(", ").attr("join")(variant_modules.keys()));
        throw nb::import_error(
            nb::str("Requested an unsupported variant \"{}\". The "
                    "following variants are available: {}.")
                .format(all_args, all_variants)
                .c_str()
        );
    }

    curr_variant = new_variant;
    curr_variant_module = variant_module(curr_variant);

    // FIXME: Reload python integrators if we're setting a JIT enabled variant
};

/// Main aliasing mechanism to fetch attributes from `mitsuba`
static nb::object get_attr(nb::handle key) {
    // Lookup cache
    nb::tuple cache_key = nb::make_tuple(curr_variant, key);

    nb::object result = nb::borrow(PyDict_GetItem(cache.ptr(), key.ptr()));
    if (result)
        return result;

    #define MI_PY_GET_ATTR(obj, key)                                           \
    {                                                                          \
        result = nb::steal(PyObject_GetAttr(obj.ptr(), key.ptr()));            \
        if (result) {                                                          \
            cache[cache_key] = result;                                         \
            return result;                                                     \
        }                                                                      \
        PyErr_Clear();                                                         \
    }

    // Accessing variant-specifc C++ code
    if (curr_variant)
        MI_PY_GET_ATTR(curr_variant_module, key);

    // Accessing non-variant C++ code
    MI_PY_GET_ATTR(mitsuba_ext, key);

    // Accessing Python code ("mi.python.foo" should be equivalent to "mi.foo")
    MI_PY_GET_ATTR(mitsuba_python, key);

    #undef MI_PY_GET_ATTR

    // Explicitly accessing a variant (i.e `mi.scalar_rgb.something`)
    if (variant_modules.contains(key))
        return variant_module(key);

    throw nb::attribute_error(
        nb::str("module 'mitsuba' has no attribute '{}'").format(key).c_str());
}

NB_MODULE(mitsuba_alias, m) {
    // Temporarily change the module name (for pydoc)
    m.attr("__name__") = "mitsuba";

    // Need to populate `__path__` we do it by using the `__file__` attribute
    // of a Python file which is located in the same directory as this module
    nb::module_ os = nb::module_::import_("os");
    nb::module_ cfg = nb::module_::import_("mitsuba.config");
    nb::object cfg_path = os.attr("path").attr("realpath")(cfg.attr("__file__"));
    nb::object mi_dir = os.attr("path").attr("dirname")(cfg_path);
    nb::object mi_py_dir = os.attr("path").attr("join")(mi_dir, "python");
    nb::list paths{};
    paths.append(nb::str(mi_dir));
    paths.append(nb::str(mi_py_dir));
    m.attr("__path__") = paths;

    // Replace sys.modules['mitsuba'] with this module, such that `mitsuba` is
    // resolved as `mitsuba_alias` in Python
    nb::module_::import_("sys").attr("modules")["mitsuba"] = m;

    nb::list all_variant_names(nb::str(MI_VARIANTS).attr("split")("\n"));
    size_t variant_count = nb::len(all_variant_names) - 1;
    for (size_t i = 0; i < variant_count; ++i)
        // Fill with `None`, so we can also use `variant_modules.keys()` as the
        // set of variant names
        variant_modules[all_variant_names[i]] = nb::none();

    m.def("variant", []() {
        return curr_variant ? curr_variant : nb::none();
    });
    m.def("variants", []() { return variant_modules.keys(); });
    m.def("set_variant", set_variant);
    m.def("__getattr__", [](nb::handle key) { return get_attr(key); });

    // Load non-variant-specific bindings
    mitsuba_ext = nb::steal(nb::module_::import_("mitsuba.mitsuba_ext"));
    // Load additional python definitions
    mitsuba_python = nb::steal(nb::module_::import_("mitsuba.python"));

    /* Cleanup all static variables, this is called when the interpreter is
     * exiting */
    auto atexit = nb::module_::import_("atexit");
    atexit.attr("register")(nb::cpp_function([]() {
        mitsuba_ext.reset();
        mitsuba_python.reset();
        curr_variant.reset();
        curr_variant_module.reset();
        variant_modules.reset();
        cache.reset();
    }));

    // Change module name back to correct value
    m.attr("__name__") = "mitsuba_alias";
}
