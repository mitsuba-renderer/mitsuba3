#include <mitsuba/core/plugin.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fresolver.h>
#include <tsl/robin_map.h>
#include <mutex>

#if !defined(_WIN32)
#  include <dlfcn.h>
#else
#  include <windows.h>
#endif

NAMESPACE_BEGIN(mitsuba)

ref<PluginManager> PluginManager::m_instance = new PluginManager();

/// Map (name, variant) -> (constructor, payload)
using PluginMap = tsl::robin_map<
    std::pair<std::string, std::string>,
    std::pair<PluginInstantiateFn, void *>
>;

/// List of cleanup callbacks
using CleanupList = std::vector<
    std::pair<PluginReleaseFn, void *>
>;

struct PluginManager::PluginManagerPrivate {
    PluginMap plugins;
    CleanupList cleanup;
    std::mutex mutex;

    void load_shared(const char *name) {
        std::lock_guard<std::mutex> guard(mutex);
        void *handle, *func;

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
        handle = LoadLibraryW(path.native().c_str());
        if (!handle)
            Throw("Error while loading plugin \"%s\": %s", path.string(),
                  util::last_error());

        func = GetProcAddress(handle, "plugin_info");

        if (!func) {
            FreeLibrary(handle);
            Throw("Could resolve the entry point of plugin \"%s\": %s",
                  path.string(), util::last_error());
        }

        cleanup.emplace_back(handle, [](void *h) { FreeLibrary(h); });
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

        handle = dlopen(path.native().c_str(), flags);
        if (!handle)
            Throw("Error while loading plugin \"%s\": %s", path.string(),
                  dlerror());
        func = dlsym(handle, "plugin_info");

        if (!func) {
            dlclose(handle);
            Throw("Could not resolve the entry point of plugin \"%s\": %s",
                  path.string(), dlerror());
        }

        cleanup.emplace_back(handle, [](void *h) { dlclose(h); });
#endif

        PluginRegisterFn register_fn = [](const char *name_, const char *variant, PluginInstantiateFn fn) {
                PluginManager::PluginManagerPrivate *d = m_instance->d.get();
                auto [it, success] = d->plugins.emplace(
                    std::make_pair(name_, variant),
                    std::make_pair(fn, name_)
                );

                if (!success)
                    Throw("PluginManager: constructor already present for "
                          "plugin \"%s\", variant \"%s\"", name_, variant);
            };

        // Ask the plugin to register constructors for all supported variants
        ((PluginEntryFn) func)(name, register_fn);
    }
};

PluginManager::PluginManager() : d(new PluginManagerPrivate()) { }
PluginManager::~PluginManager() { reset(); }

void PluginManager::reset() {
    std::lock_guard<std::mutex> guard(d->mutex);
    for (auto [release_cb, payload] : d->cleanup)
        release_cb(payload);
    d->cleanup.clear();
    d->plugins.clear();
}

void PluginManager::register_plugin(PluginInfo info) {
    std::lock_guard<std::mutex> guard(d->mutex);
    d->plugins[std::make_pair(info.name, info.variant)] =
        std::make_pair(info.instantiate, info.payload);
    d->cleanup.push_back(std::make_pair(info.release, info.payload));
}

ref<Object> PluginManager::create_object(const char *variant,
                                         ObjectType type,
                                         const Properties &props) {
    const char *name = props.plugin_name();


    ref<Object> object;
    if (strlen(name) == 0)
        Throw("A plugin name must be specified!");

    for (uint32_t i = 0; i < 2; ++i) {
        std::lock_guard<std::mutex> guard(d->mutex);

        PluginMap::iterator it = d->plugins.find(
            std::make_pair(name, variant)
        );

        if (it != d->plugins.end()) {
            auto [instantiate, payload] = it->second;
            object = instantiate(payload, props);
        }

        if (i == 0)
            d->load_shared(name);
    }

    if (!object)
        Throw("Internal error: could not find way of constructing plugin "
              "\"%s\" with variant \"%s\"", name);

    ObjectType actual_type = object->type();
    if (type != actual_type) {
        Throw("Type mismatch when loading plugin \"%s\": constructed instance "
              "\"%s\" does not conform to the interface \"%s\"",
              props.plugin_name(), actual, expected);
    }


#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_CUDA)
    // Ensures queued side effects are consistently compiled into cacheable kernels
    if (string::starts_with(variant, "cuda_") ||
        string::starts_with(variant, "llvm_"))
        dr::eval();
#endif

    return object;
}

NAMESPACE_END(mitsuba)
