#include <drjit/tensor.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/xml.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>
#include <nanothread/nanothread.h>
#include <map>
#include <unordered_map>

#include <nanobind/stl/pair.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/string.h>

using Caster = nb::object(*)(mitsuba::Object *);
extern Caster cast_object;

struct DictInstance {
    Properties props;
    ref<Object> object = nullptr;
    std::vector<std::pair<std::string, std::string>> dependencies;
};

struct DictParseContext {
    ThreadEnvironment env;
    std::map<std::string, DictInstance> instances;
    std::map<std::string, std::string> aliases;
    bool parallel = false;
};

// Forward declaration
template <typename Float, typename Spectrum>
void parse_dictionary(
    DictParseContext &ctx,
    const std::string path,
    const nb::dict &dict
);
template <typename Float, typename Spectrum>
Task * instantiate_node(
    DictParseContext &ctx,
    const std::string path,
    std::unordered_map<std::string, Task *> &task_map
);

/// Shorthand notation for accessing the MI_VARIANT string
#define GET_VARIANT() mitsuba::detail::get_variant<Float, Spectrum>()

/// Depending on whether or not the input vector has size 1, returns the first
/// and only element of the vector or the entire vector as a list
MI_INLINE nb::object single_object_or_list(std::vector<ref<Object>> &objects) {
    if (objects.size() == 1)
        return cast_object(objects[0]);

    nb::list l;
    for (ref<Object> &obj : objects)
        l.append(cast_object(obj));

    return static_cast<nb::object>(l);
}

MI_PY_EXPORT(xml) {
    MI_PY_IMPORT_TYPES()

    m.def(
        "load_file",
        [](const std::string &name, bool update_scene, bool parallel, nb::kwargs kwargs) {
            xml::ParameterList param;
            if (kwargs) {
                for (auto [k, v] : kwargs)
                    param.emplace_back(
                        nb::str(k).c_str(),
                        nb::str(v).c_str(),
                        false
                    );
            }

            std::vector<ref<Object>> objects;
            {
                nb::gil_scoped_release release;
                objects = xml::load_file(name, GET_VARIANT(), param, update_scene, parallel);
            }

            return single_object_or_list(objects);
        },
        "path"_a, "update_scene"_a = false, "parallel"_a = true, "kwargs"_a,
        D(xml, load_file));

    m.def(
        "load_string",
        [](const std::string &name, bool parallel, nb::kwargs kwargs) {
            xml::ParameterList param;
            if (kwargs) {
                for (auto [k, v] : kwargs)
                    param.emplace_back(
                        nb::str(k).c_str(),
                        nb::str(v).c_str(),
                        false
                    );
            }

            std::vector<ref<Object>> objects;
            {
                nb::gil_scoped_release release;
                objects = xml::load_string(name, GET_VARIANT(), param, parallel);
            }

            return single_object_or_list(objects);
        },
        "string"_a, "parallel"_a = true, "kwargs"_a,
        D(xml, load_string));

    m.def(
        "load_dict",
        [](const nb::dict dict, bool parallel) {
            // Make a backup copy of the FileResolver, which will be restored after parsing
            ref<FileResolver> fs_backup = Thread::thread()->file_resolver();
            Thread::thread()->set_file_resolver(new FileResolver(*fs_backup));

            DictParseContext ctx;
            ctx.parallel = parallel;
            ctx.env = ThreadEnvironment();

            try {
                parse_dictionary<Float, Spectrum>(ctx, "__root__", dict);
                std::unordered_map<std::string, Task*> task_map;
                instantiate_node<Float, Spectrum>(ctx, "__root__", task_map);
                auto objects = mitsuba::xml::detail::expand_node(ctx.instances["__root__"].object);
                Thread::thread()->set_file_resolver(fs_backup.get());
                return single_object_or_list(objects);
            } catch(...) {
                Thread::thread()->set_file_resolver(fs_backup.get());
                throw;
            }
        },
        "dict"_a, "parallel"_a=true,
        R"doc(Load a Mitsuba scene or object from an Python dictionary

Parameter ``dict``:
    Python dictionary containing the object description

Parameter ``parallel``:
    Whether the loading should be executed on multiple threads in parallel

)doc");

    m.def(
        "xml_to_props",
        [](const std::string &path) {
            nb::gil_scoped_release release;

            auto result = std::vector<std::pair<std::string, PropertiesV<Float>>>();
            for (const auto& [k,v] : xml::detail::xml_to_properties(path, GET_VARIANT()))
                result.emplace_back(k, PropertiesV<Float>(v));

            return result;
        },
        "path"_a,
        R"doc(Get the names and properties of the objects described in a Mitsuba XML file)doc");
}

// Helper function to find a value of "type" in a Python dictionary
std::string get_type(const nb::dict &dict) {
    for (const auto &[key, value] : dict)
        if (nb::cast<std::string>(key) == "type")
            return nb::cast<std::string>(value);

    Throw("Missing key 'type' in dictionary: %s", nb::str(dict).c_str());
}

#define SET_PROPS(PyType, Type, Setter)            \
    if (nb::isinstance<PyType>(value)) {           \
        props.Setter(key, nb::cast<Type>(value));  \
        continue;                                  \
    }

/// Helper function to give the object a chance to recursively expand into sub-objects
void expand_and_set_object(Properties &props, const std::string &name, const ref<Object> &obj) {
    std::vector<ref<Object>> children = obj->expand();
    if (children.empty()) {
        props.set_object(name, obj);
    } else if (children.size() == 1) {
        props.set_object(name, children[0]);
    } else {
        int ctr = 0;
        for (auto c : children)
            props.set_object(name + "_" + std::to_string(ctr++), c);
    }
}

template <typename Float, typename Spectrum>
ref<Object> create_texture_from(const nb::dict &dict, bool within_emitter) {
    // Treat nested dictionary differently when their type is "rgb" or "spectrum"
    ref<Object> obj;
    std::string type = get_type(dict);
    if (type == "rgb") {
        if (dict.size() != 2) {
            Throw("'rgb' dictionary should always contain 2 entries "
                  "('type' and 'value'), got %u.", dict.size());
        }
        // Read info from the dictionary
        Properties::Color3f color(0.f);
        for (const auto& [k2, value2] : dict) {
            std::string key2 = nb::cast<std::string>(k2);
            using Color3f = Properties::Color3f;
            if (key2 == "value") {
                try {
                    color = nb::cast<Color3f>(value2);
                } catch (const nb::cast_error &) {
                    Throw("Could not convert %s into Color3f",
                        nb::str(value2).c_str());
                }
            } else if (key2 != "type")
                Throw("Unexpected key in rgb dictionary: %s", key2);
        }
        // Update the properties struct
        obj = mitsuba::xml::detail::create_texture_from_rgb(
                "rgb", color, GET_VARIANT(), within_emitter);
    } else if (type == "spectrum") {
        if (dict.size() != 2) {
            Throw("'spectrum' dictionary should always contain 2 "
                  "entries ('type' and 'value'), got %u.", dict.size());
        }
        // Read info from the dictionary
        Properties::Float const_value(1);
        std::vector<Properties::Float> wavelengths;
        std::vector<Properties::Float> values;
        for (const auto& [k2, value2] : dict) {
            std::string key2 = nb::cast<std::string>(k2);
            if (key2 == "filename") {
                spectrum_from_file(nb::cast<std::string>(value2), wavelengths, values);
            } else if (key2 == "value") {
                if (nb::isinstance<nb::float_>(value2) ||
                    nb::isinstance<nb::int_>(value2)) {
                    const_value = nb::cast<Properties::Float>(value2);
                } else if (nb::isinstance<nb::list>(value2)) {
                    nb::list list = nb::cast<nb::list>(value2);
                    wavelengths.resize(list.size());
                    values.resize(list.size());
                    for (size_t i = 0; i < list.size(); ++i) {
                        auto pair = nb::cast<nb::tuple>(list[i]);
                        wavelengths[i] = nb::cast<Properties::Float>(pair[0]);
                        values[i]      = nb::cast<Properties::Float>(pair[1]);
                    }
                } else {
                    Throw("Unexpected value type in 'spectrum' dictionary: %s",
                          nb::str(value2).c_str());
                }
            } else if (key2 != "type") {
                Throw("Unexpected key in spectrum dictionary: %s", key2);
            }
        }
        // Update the properties struct
        obj = mitsuba::xml::detail::create_texture_from_spectrum(
                "spectrum", const_value, wavelengths, values, GET_VARIANT(),
                within_emitter, is_spectral_v<Spectrum>,
                is_monochromatic_v<Spectrum>);
    }

    return obj;
}

template <typename Float, typename Spectrum>
void parse_dictionary(DictParseContext &ctx,
                      const std::string path,
                      const nb::dict &dict) {
    MI_IMPORT_CORE_TYPES()
    using ScalarArray3f = dr::Array<ScalarFloat, 3>;

    auto &inst = ctx.instances[path];

    std::string type = get_type(dict);
    bool is_scene = (type == "scene");
    bool is_root = string::starts_with(path, "__root__");

    if (type == "spectrum" || type == "rgb") {
        inst.object = create_texture_from<Float, Spectrum>(dict, false);
        return;
    }

    const Class *class_;
    if (is_scene)
        class_ = Class::for_name("Scene", GET_VARIANT());
    else
        class_ = PluginManager::instance()->get_plugin_class(type, GET_VARIANT())->parent();

    bool within_emitter = (!is_scene && class_->alias() == "emitter");

    Properties &props = inst.props;
    props.set_plugin_name(type);

    std::string id;

    // Temporarily disable nanobind's warnings for failed implicit casts
    // We wrap our dict value cast attempts in try-catch blocks so it's unnecessary
    struct scoped_nb_implicit_cast_warnings {
        bool implicit_cast_warnings;

        scoped_nb_implicit_cast_warnings(bool b) {
            implicit_cast_warnings = nb::implicit_cast_warnings();
            nb::set_implicit_cast_warnings(b);
        }

        ~scoped_nb_implicit_cast_warnings() {
            nb::set_implicit_cast_warnings(implicit_cast_warnings);
        }
    } scoped_nb_warning(false);

    for (const auto& [k, value] : dict) {
        std::string key = nb::cast<std::string>(k);

        if (key == "type")
            continue;

        if (key == "id") {
            id = nb::cast<std::string>(value);
            continue;
        }

        SET_PROPS(nb::bool_, bool, set_bool);
        SET_PROPS(nb::int_, int64_t, set_long);
        SET_PROPS(nb::float_, Properties::Float, set_float);
        SET_PROPS(nb::str, std::string, set_string);
        SET_PROPS(ScalarColor3f, ScalarColor3f, set_color);
        SET_PROPS(ScalarArray3f, ScalarArray3f, set_array3f);
        SET_PROPS(ScalarTransform3f, ScalarTransform3f, set_transform3f);
        SET_PROPS(ScalarTransform4f, ScalarTransform4f, set_transform);

        if (key.find('.') != std::string::npos) {
            Throw("The object key '%s' contains a '.' character, which is "
                  "already used as a delimiter in the object path in the scene."
                  " Please use '_' instead.", key);
        }

        // Parse nested dictionary
        if (nb::isinstance<nb::dict>(value)) {
            nb::dict dict2 = nb::cast<nb::dict>(value);
            std::string type2 = get_type(dict2);

            if (type2 == "spectrum" || type2 == "rgb") {
                props.set_object(key, create_texture_from<Float, Spectrum>(dict2, within_emitter));
                continue;
            }

            if (type2 == "resources") {
                ref<FileResolver> fs = Thread::thread()->file_resolver();
                std::string path_ = nb::cast<std::string>(dict2["path"]);
                fs::path resource_path(path_);
                if (!resource_path.is_absolute()) {
                    // First try to resolve it starting in the Python file directory
                    nb::module_ inspect = nb::module_::import_("inspect");
                    nb::object filename = inspect.attr("getfile")(inspect.attr("currentframe")());
                    fs::path current_file(nb::cast<std::string>(filename));
                    resource_path = current_file.parent_path() / resource_path;
                    // Otherwise try to resolve it with the FileResolver
                    if (!fs::exists(resource_path))
                        resource_path = fs->resolve(path_);
                }
                if (!fs::exists(resource_path))
                    Throw("path: folder %s not found", resource_path);
                fs->prepend(resource_path);
                continue;
            }

            // Nested dict with type == "ref" specify a reference to another
            // object previously instantiated
            if (type2 == "ref") {
                if (is_scene)
                    Throw("Reference found at the scene level: %s", key);

                for (const auto& kv2 : nb::cast<nb::dict>(value)) {
                    std::string key2 = nb::cast<std::string>(kv2.first);
                    if (key2 == "id") {
                        std::string id2 = nb::cast<std::string>(kv2.second);
                        std::string path2;
                        if (ctx.aliases.count(id2) == 1)
                            path2 = ctx.aliases[id2];
                        else
                            path2 = id2;
                        if (ctx.instances.count(path2) != 1)
                            Throw("Referenced id \"%s\" not found: %s", path2, path);
                        inst.dependencies.push_back({key, path2});
                    } else if (key2 != "type") {
                        Throw("Unexpected key in ref dictionary: %s", key2);
                    }
                }
            } else {
                std::string path2 = is_root ? key : path + "." + key;
                inst.dependencies.push_back({key, path2});
                parse_dictionary<Float, Spectrum>(ctx, path2, dict2);
            }
            continue;
        }

        // Try to cast entry to an object
        try {
            ref<Object> obj = nb::cast<Object*>(value);
            obj->set_id(key);
            expand_and_set_object(props, key, obj);
            continue;
        } catch (const nb::cast_error &) { }

        // Try to cast to Array3f (list, tuple, numpy.array, ...)
        try {
            props.set_array3f(key, nb::cast<Properties::Array3f>(value));
            continue;
        } catch (const nb::cast_error &) { }

        // Try to cast to TensorXf
        try {
            TensorXf tensor = nb::cast<TensorXf>(value);
            // To support parallel loading we have to ensure tensor has been evaluated
            // because tracking of side effects won't persist across different ThreadStates
            dr::eval(tensor);
            props.set_tensor_handle(key, std::make_shared<TensorXf>(tensor));
            continue;
        } catch (const nb::cast_error &) { }

        // Didn't match any of the other types above
        Throw("Unsupported value type: %s!\n", nb::str(value.type()).c_str());
    }

    // Set object id based on path in dictionary if no id is provided
    props.set_id(id.empty() ? string::tokenize(path, ".").back() : id);

    if (!id.empty()) {
        if (ctx.aliases.count(id) != 0)
            Throw("%s has duplicate id: %s", path, id);
        ctx.aliases[id] = path;
    }
}

template <typename Float, typename Spectrum>
Task *instantiate_node(DictParseContext &ctx,
                       std::string path,
                       std::unordered_map<std::string, Task *> &task_map) {
    if (task_map.find(path) != task_map.end())
        return task_map.find(path)->second;

    auto &inst = ctx.instances[path];
    bool is_root = path == "__root__";
    uint32_t backend = (uint32_t) dr::backend_v<Float>;

    // Early exit if the object was already instantiated
    if (inst.object)
        return nullptr;

    std::vector<Task *> deps;
    for (auto &[key2, path2] : inst.dependencies) {
        if (task_map.find(path2) == task_map.end()) {
            Task *task = instantiate_node<Float, Spectrum>(ctx, path2, task_map);
            task_map.insert({path2, task});
        }
        deps.push_back(task_map.find(path2)->second);
    }

    uint32_t scope = 0;
    if constexpr (dr::is_jit_v<Float>) {
        if (ctx.parallel) {
            jit_new_scope(dr::backend_v<Float>);
            scope = jit_scope(dr::backend_v<Float>);
        }
    }

    auto instantiate = [&ctx, path, scope, backend]() {
        ScopedSetThreadEnvironment set_env(ctx.env);
        mitsuba::xml::ScopedSetJITScope set_scope(ctx.parallel ? backend : 0u, scope);

        auto &inst = ctx.instances[path];
        Properties props = inst.props;
        std::string type = props.plugin_name();

        const Class *class_;
        if (type == "scene")
            class_ = Class::for_name("Scene", GET_VARIANT());
        else
            class_ = PluginManager::instance()->get_plugin_class(type, GET_VARIANT())->parent();

        for (auto &[key2, path2] : inst.dependencies) {
            if (ctx.instances.count(path2) == 1) {
                ref<Object> obj2 = ctx.instances[path2].object;
                if (obj2)
                    expand_and_set_object(props, key2, obj2);
                else
                    Throw("Dependence hasn't been instantiated yet: %s, %s -> %s", path, path2, key2);
            } else {
                Throw("Dependence path \"%s\" not found: %s", path2, path);
            }
        }

        // Construct the object with the parsed Properties
        inst.object = PluginManager::instance()->create_object(props, class_);

        if (!props.unqueried().empty())
            Throw("Unreferenced property \"%s\" in plugin of type \"%s\"!", props.unqueried()[0], type);
    };

    // Top node always instantiated on the main thread
    if (is_root) {
        std::exception_ptr eptr;
        for (auto& task : deps) {
            try {
                nb::gil_scoped_release gil_release{};
                task_wait(task);
            } catch (...) {
                if (!eptr)
                    eptr = std::current_exception();
            }
        }
        for (auto& kv : task_map)
            task_release(kv.second);
        if (eptr)
            std::rethrow_exception(eptr);
        instantiate();
#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_CUDA)
        if (backend && ctx.parallel)
            jit_new_scope((JitBackend) backend);
#endif
        return nullptr;
    } else {
        if (ctx.parallel) {
            // Instantiate object asynchronously
            return dr::do_async(instantiate, deps.data(), deps.size());
        } else {
            instantiate();
            return nullptr;
        }
    }
}

#undef SET_PROPS
