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
#include <mitsuba/mitsuba.h>
#include <nanobind/nanobind.h>

namespace nb = nanobind;

/**
 * On Windows the GIL is not held when we load DLLs due to potential deadlocks
 * with the Windows loader-lock.
 * (see https://github.com/python/cpython/issues/78076 that describes a similar
 * issue). Here, initialization of static variables is performed during DLL
 * loading.
 *
 * The issue is that the default constructor of nb::dict calls PyDict_New
 * however the safe use of the CPython API requires that the GIL is acquired.
 *
 * Hence we directly use CPython calls for our global dictionaries
**/

/// Mapping from variant name to module
PyObject *variant_modules = nullptr;

/// Additional reference to this module's `__dict__` attribute
PyObject *mi_dict = nullptr;

/// Current variant (string)
nb::object curr_variant;

/// Set of user-provided callback functions to call when switching variants
nb::object variant_change_callbacks;


nb::object import_with_deepbind_if_necessary(const char* name) {
#if defined(__clang__) && !defined(__APPLE__)
    nb::int_ backupflags;
    nb::object sys = nb::module_::import_("sys");
    if (!std::getenv("DRJIT_NO_RTLD_DEEPBIND")) {
        backupflags = nb::borrow<nb::int_>(sys.attr("getdlopenflags")());
        nb::object os = nb::module_::import_("os");
        nb::object rtld_lazy = os.attr("RTLD_LAZY");
        nb::object rtld_local = os.attr("RTLD_LOCAL");
        nb::object rtld_deepbind = os.attr("RTLD_DEEPBIND");

        sys.attr("setdlopenflags")(rtld_lazy | rtld_local | rtld_deepbind);
    }
#endif

    nb::object out = nb::module_::import_(name);

#if defined(__clang__) && !defined(__APPLE__)
    if (!std::getenv("DRJIT_NO_RTLD_DEEPBIND"))
        sys.attr("setdlopenflags")(backupflags);
#endif

    return out;
}

static inline void Safe_PyDict_SetItem(PyObject *p, PyObject *key, PyObject *val) {
    int rv = PyDict_SetItem(p, key, val);
    if (rv)
        nb::raise_python_error();
}

/// Return the Python module associated with a given variant name
static nb::object variant_module(nb::handle variant) {
    nb::object result = nb::borrow(PyDict_GetItem(variant_modules, variant.ptr()));
    if (!result.is_none())
        return result;

    nb::str module_name = nb::str("mitsuba.{}").format(variant);
    result = import_with_deepbind_if_necessary(module_name.c_str());
    Safe_PyDict_SetItem(variant_modules, variant.ptr(), result.ptr());

    return result;
}

/// Sets the variant
static void set_variant(nb::args args) {
    nb::list valid_variants{};
    for (const auto &arg : args) {
        if (PyDict_Contains(variant_modules, arg.ptr()) == 1) {
            // Variant is at least compiled, we can attempt to use it.
            valid_variants.append(arg);
        }
    }

    if (valid_variants.size() == 0) {
        // None of the requested variants are compiled.
        nb::object all_args(nb::str(", ").attr("join")(args));
        nb::object all_variants(
            nb::str(", ").attr("join")(nb::steal(PyDict_Keys(variant_modules))));
        throw nb::import_error(
            nb::str("Requested an unsupported variant \"{}\". The "
                    "following variants are available: {}.")
                .format(all_args, all_variants)
                .c_str()
        );
    }

    nb::object old_variant = curr_variant;
    // For each requested _and_ available variant, in order of preference.
    for (size_t i = 0; i < valid_variants.size(); ++i) {
        const auto &requested_variant = valid_variants[i];
        bool is_last = (i == valid_variants.size() - 1);

        if (requested_variant.equal(old_variant)) {
            // We're already using this variant, no need to do anything.
            break;
        }

        nb::object new_variant_module;
        try {
            new_variant_module = variant_module(requested_variant);
        } catch (const nb::python_error &e) {
            // The variant failed to import, this could happen e.g. if the
            // CUDA driver is installed, but there is no GPU available.
            // We only allow such failures as long as we have more variants to try.
            if (!is_last && e.matches(PyExc_ImportError)) {
                const auto mi = nb::module_::import_("mitsuba");
                mi.attr("Log")(
                    mi.attr("LogLevel").attr("Debug"),
                    nb::str("The requested variant \"{}\" could not be loaded, "
                            "attempting the next one. The exception was:\n{}\n")
                        .format(requested_variant, e.what()));
                continue;
            } else {
                throw e;
            }
        }

        nb::dict variant_dict = new_variant_module.attr("__dict__");
        for (const auto &k : variant_dict.keys())
            if (!nb::bool_(k.attr("startswith")("__")) &&
                !nb::bool_(k.attr("endswith")("__")))
                Safe_PyDict_SetItem(mi_dict, k.ptr(),
                    PyDict_GetItem(variant_dict.ptr(), k.ptr()));

        curr_variant = requested_variant;
        break;
    }


    if (!curr_variant.equal(old_variant)) {
        // Reload internal plugins
        if (curr_variant.attr("startswith")(nb::make_tuple("llvm_", "cuda_"))) {
            nb::module_ mi_python = nb::module_::import_("mitsuba.python.ad.integrators");
            nb::steal(PyImport_ReloadModule(mi_python.ptr()));
        }

        // Only invoke user-provided callbacks after Mitsuba plugins have reloaded,
        // as there may be a dependency
        const auto &callbacks = nb::borrow<nb::set>(variant_change_callbacks);
        for (const auto &cb : callbacks)
            cb(old_variant, curr_variant);
    }
}

/**
 * The given callable will be called each time the Mitsuba variable is changed.
 * Note that callbacks are kept in a set: a given callback function will only be
 * called once, even if it is added multiple times.
 *
 * `callback` will be called with the arguments `old_variant: str, new_variant: str`.
 */
static void add_variant_callback(const nb::callable &callback) {
    nb::borrow<nb::set>(variant_change_callbacks).add(callback);
}

/**
 * Removes the given `callback` callable from the list of callbacks to be called
 * when the Mitsuba variant changes.
 */
static void remove_variant_callback(const nb::callable &callback) {
    nb::borrow<nb::set>(variant_change_callbacks).discard(callback);
}

/**
 * Removes all callbacks to be called when the Mitsuba variant changes.
 */
static void clear_variant_callbacks() {
    nb::borrow<nb::set>(variant_change_callbacks).clear();
}

/// Fallback for when we're attempting to fetch variant-specific attribute
static nb::object get_attr(nb::handle key) {
    if (PyDict_Contains(variant_modules, key.ptr()) == 1)
        return variant_module(key);

    // If no variant is set, inform the user
    if (curr_variant.is_none() && PyUnicode_Check(key.ptr())) {
        const char* attr_name = PyUnicode_AsUTF8(key.ptr());
        if (attr_name) {
            return nb::steal(PyErr_Format(PyExc_AttributeError,
                "Cannot access '%s' before setting a variant. "
                "Please call `mitsuba.set_variant('variant_name')` first. "
                "For example: mitsuba.set_variant('scalar_rgb') or mitsuba.set_variant('cuda_ad_rgb'). "
                "Use mitsuba.variants() to see all available variants.",
                attr_name));
        }
    }

    return nb::steal(PyErr_Format(PyExc_AttributeError, "Module 'mitsuba' has no attribute %R", key.ptr()));
}

NB_MODULE(mitsuba_alias, m) {
    m.attr("__name__") = "mitsuba";
    m.attr("__version__")      = MI_VERSION;

    curr_variant = nb::none();
    variant_change_callbacks = nb::set();

    if (!variant_modules)
        variant_modules = PyDict_New();

    // Need to populate `__path__` we do it by using the `__file__` attribute
    // of a Python file which is located in the same directory as this module
    nb::module_ path = nb::module_::import_("os").attr("path");
    nb::module_ cfg = nb::module_::import_("mitsuba.config");
    nb::object cfg_path = path.attr("realpath")(cfg.attr("__file__"));
    nb::object mi_dir = path.attr("dirname")(cfg_path);
    nb::object mi_py_dir = path.attr("join")(mi_dir, "python");
    nb::list paths{};
    paths.append(nb::str(mi_dir));
    paths.append(nb::str(mi_py_dir));
    m.attr("__path__") = paths;

    // Replace sys.modules['mitsuba'] with this module, such that `mitsuba` is
    // resolved as `mitsuba_alias` in Python
    nb::module_::import_("sys").attr("modules")["mitsuba"] = m;

    nb::list all_variant_names(nb::str(MI_VARIANTS).attr("split")("\n"));
    size_t variant_count = nb::len(all_variant_names) - 1;
    for (size_t i = 0; i < variant_count; ++i) {
        // Fill with `None`, so we can also use `variant_modules.keys()` as the
        // set of variant names
        Safe_PyDict_SetItem(variant_modules,
            all_variant_names[i].ptr(), nb::none().ptr());
    }

    m.def("variant", []() { return curr_variant ? curr_variant : nb::none(); },
          nb::sig("def variant() -> typing.Optional[str]"));
    m.def("variants", []() { return nb::steal(PyDict_Keys(variant_modules)); },
          nb::sig("def variants() -> typing.List[str]"));
    m.def("set_variant", set_variant,
          nb::sig("def set_variant(*args: str) -> None"));
    /// Only used for variant-specific attributes e.g. mi.scalar_rgb.T
    m.def("__getattr__", [](nb::handle key) { return get_attr(key); });

    // Create the detail submodule
    nb::module_ mi_detail = nb::module_::import_("mitsuba.detail");
    mi_detail.def("add_variant_callback", add_variant_callback);
    mi_detail.def("remove_variant_callback", remove_variant_callback);
    mi_detail.def("clear_variant_callbacks", clear_variant_callbacks);
    m.attr("detail") = mi_detail;

    /// Fill `__dict__` with all objects in `mitsuba_ext` and `mitsuba.python`
    mi_dict = m.attr("__dict__").ptr();
    nb::object mi_ext = import_with_deepbind_if_necessary("mitsuba.mitsuba_ext");
    nb::dict mitsuba_ext_dict = mi_ext.attr("__dict__");
    for (const auto &k : mitsuba_ext_dict.keys()) {
        if (!nb::bool_(k.attr("startswith")("__")) &&
            !nb::bool_(k.attr("endswith")("__"))) {
            Safe_PyDict_SetItem(mi_dict, k.ptr(), mitsuba_ext_dict[k].ptr());
        }
    }

    // Import contents of `mitsuba.python` into top-level `mitsuba` module
    nb::object mi_python = nb::module_::import_("mitsuba.python");
    nb::dict mitsuba_python_dict = mi_python.attr("__dict__");
    for (const auto &k : mitsuba_python_dict.keys()) {
        if (!nb::bool_(k.attr("startswith")("__")) &&
            !nb::bool_(k.attr("endswith")("__"))) {
            Safe_PyDict_SetItem(mi_dict, k.ptr(), mitsuba_python_dict[k].ptr());
        }
    }

    /// Cleanup static variables, this is called when the interpreter is exiting
    auto atexit = nb::module_::import_("atexit");
    atexit.attr("register")(nb::cpp_function([]() {
        curr_variant.reset();

        PyDict_Clear(mi_dict);
        mi_dict = nullptr;

        variant_change_callbacks.reset();

        if (variant_modules) {
            Py_DECREF(variant_modules);
            variant_modules = nullptr;
        }
    }));
}
