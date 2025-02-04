#include <mitsuba/core/plugin.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/hash.h>
#include <tsl/robin_map.h>
#include <mutex>

#if !defined(_WIN32)
#  include <dlfcn.h>
#else
#  include <windows.h>
#endif

NAMESPACE_BEGIN(mitsuba)

// Record for plugin modules (dynamically loaded shared libraries)
struct ModuleInfo {
    // Opaque module handle
    void *handle;

    // Entry function to register plugin variant
    PluginEntryFn entry;
};

// Record for plugin variants (specific entry points)
struct PluginInfo {
    // Instantiate a plugin with specified properties
    PluginInstantiateFn instantiate;

    // Release all resources of the plugin
    PluginReleaseFn release;

    // Opaque payload to be passed to the callbacks above
    void *payload;
};

/// Plugin name -> plugin module information
using ModuleMap = tsl::robin_map<std::string, ModuleInfo,
                                 std::hash<std::string_view>,
                                 std::equal_to<>>;

/// (name, variant) -> plugin variant information
using PluginMap = tsl::robin_map<
    std::pair<std::string, std::string>,
    PluginInfo, pair_hasher, pair_eq>;

ref<PluginManager> PluginManager::m_instance = new PluginManager();

struct PluginManager::PluginManagerPrivate {
    std::mutex mutex;
    PluginMap plugins;
    ModuleMap modules;

    ~PluginManagerPrivate() {
        release_all();
        unload_all();
    }

    void release_all() {
        for (auto& [_, plugin] : plugins) {
            if (plugin.release)
                plugin.release(plugin.payload);
        }
        plugins.clear();
    }

    void unload_all() {
        for (auto& [_, module] : modules) {
            #if defined(_WIN32)
                FreeLibrary(v.handle);
            #else
                dlclose(module.handle);
            #endif
        }
    }

    ModuleInfo load_module(std::string_view name) {
        ModuleMap::iterator it = modules.find(name);
        if (it != modules.end())
            return it->second;

        ModuleInfo module;

        // Build the full plugin file name
        fs::path filename = fs::path("plugins") / name;

        #if defined(_WIN32)
            filename.replace_extension(".dll");
        #elif defined(__APPLE__)
            filename.replace_extension(".dylib");
        #else
            filename.replace_extension(".so");
        #endif

        const FileResolver *resolver = Thread::thread()->file_resolver();
        fs::path path = resolver->resolve(filename);

        if (!fs::exists(path))
            Throw("Plugin \"%s\" not found!", name);

        Log(Debug, "Loading plugin \"%s\" ..", filename.string());

#if defined(_WIN32)
        module.handle = LoadLibraryW(path.native().c_str());
        if (!handle)
            Throw("Error while loading plugin \"%s\": %s", path.string(),
                  util::last_error());

        module.entry = (PluginEntryFn) GetProcAddress(handle, "plugin_info");

        if (!func) {
            FreeLibrary(module.handle);
            Throw("Could resolve the entry point of plugin \"%s\": %s",
                  path.string(), util::last_error());
        }

#else
        int flags = RTLD_LAZY | RTLD_LOCAL;

#if defined(__clang__) && !defined(__APPLE__)
        // When Mitsuba is built with libc++ and subsequently loaded into a
        // Python process containing extensions that previously imported
        // libstdc++ (with RTLD_GLOBAL), there is a possibility of ABI
        // violations (i.e. segfaults) resulting from symbol
        // cross-contamination. We can avoid this by forcing deep-bind linking.

        if (!std::getenv("DRJIT_NO_RTLD_DEEPBIND"))
            flags |= RTLD_DEEPBIND;
#endif

        module.handle = dlopen(path.native().c_str(), flags);
        if (!module.handle)
            Throw("Error while loading plugin \"%s\": %s", path.string(),
                  dlerror());
        module.entry = (PluginEntryFn) dlsym(module.handle, "plugin_info");

        if (!module.entry) {
            dlclose(module.handle);
            Throw("Could not resolve the entry point of plugin \"%s\": %s",
                  path.string(), dlerror());
        }

#endif

        modules[std::string(name)] = module;
        return module;
    }

    void register_plugin(std::string_view name, std::string_view variant,
                         PluginInstantiateFn instantiate,
                         PluginReleaseFn release, void *payload) {
        std::pair<std::string, std::string> key(name, variant);
        PluginMap::iterator it = plugins.find(key);
        if (it != plugins.end()) {
            PluginInfo info = it->second;
            info.release(info.payload);
        }

        plugins[std::move(key)] = PluginInfo{ instantiate, release, payload };
    }

    PluginInfo load_plugin(std::string_view name, std::string_view variant) {
        PluginMap::iterator it = plugins.find(std::make_pair(name, variant));

        if (it == plugins.end()) {
            ModuleInfo info = load_module(name);

            PluginRegisterFn register_fn = [](std::string_view name_,
                                              std::string_view variant_,
                                              PluginInstantiateFn instantiate) {
                m_instance->d->register_plugin(name_, variant_, instantiate,
                                               nullptr, nullptr);
            };

            info.entry(name, register_fn);
            it = plugins.find(std::make_pair(name, variant));

            if (it == plugins.end())
                Throw("Plugin \"%s\" (variant \"%s\") could not be found!");
        }

        return it->second;
    }
};

PluginManager::PluginManager() : d(new PluginManagerPrivate()) { }

void PluginManager::release_all() {
    std::lock_guard<std::mutex> guard(d->mutex);
    d->release_all();
}

void PluginManager::register_plugin(std::string_view name, std::string_view variant,
                                    PluginInstantiateFn instantiate,
                                    PluginReleaseFn release, void *payload) {
    std::lock_guard<std::mutex> guard(d->mutex);
    d->register_plugin(name, variant, instantiate, release, payload);
}

static std::string_view plugin_type_name(ObjectType ot) {
    switch (ot) {
        case ObjectType::Unknown: return "unknown";
        case ObjectType::Scene: return "scene";
        case ObjectType::Sensor: return "sensor";
        case ObjectType::Film: return "film";
        case ObjectType::Emitter: return "emitter";
        case ObjectType::Sampler: return "sampler";
        case ObjectType::Shape: return "shape";
        case ObjectType::Texture: return "texture";
        case ObjectType::Volume: return "volume";
        case ObjectType::Medium: return "medium";
        case ObjectType::BSDF: return "bsdf";
        case ObjectType::Integrator: return "integrator";
        case ObjectType::PhaseFunction: return "phase";
        case ObjectType::ReconstructionFilter: return "rfilter";
    }
}

ref<Object> PluginManager::create_object(const Properties &props,
                                         std::string_view variant,
                                         ObjectType type) {
    std::string_view name = props.plugin_name();

    ref<Object> object;
    if (name.empty())
        Throw("A plugin name must be specified!");

    PluginInfo info = d->load_plugin(name, variant);
    object = info.instantiate(info.payload, props);

    ObjectType actual_type = object->type();
    if (type != actual_type)
        Throw("Type mismatch: the instantiated plugin \"%s\" is of type \"%\", "
              "which does not match the expected type \"%s\".",
              props.plugin_name(), plugin_type_name(actual_type),
              plugin_type_name(type));

#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_CUDA)
    // Ensures queued side effects are consistently compiled into cacheable kernels
    if (string::starts_with(variant, "cuda_") ||
        string::starts_with(variant, "llvm_"))
        dr::eval();
#endif

    return object;
}

NAMESPACE_END(mitsuba)
