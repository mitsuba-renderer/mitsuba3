#include <mitsuba/core/plugin.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fresolver.h>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#if !defined(_WIN32)
# include <dlfcn.h>
#else
# include <windows.h>
#endif

NAMESPACE_BEGIN(mitsuba)

class Plugin {
public:
    Plugin(const fs::path &path) : m_path(path) {
        #if defined(_WIN32)
            m_handle = LoadLibraryW(path.native().c_str());
            if (!m_handle)
                Throw("Error while loading plugin \"%s\": %s", path.string(),
                      util::last_error());
        #else
            #if defined(__clang__) && !defined(__APPLE__)
                if (std::getenv("DRJIT_NO_RTLD_DEEPBIND"))
                    m_handle = dlopen(path.native().c_str(), RTLD_LAZY | RTLD_LOCAL);
                else
                    m_handle = dlopen(path.native().c_str(), RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);
            #else
                m_handle = dlopen(path.native().c_str(), RTLD_LAZY | RTLD_LOCAL);
            #endif
            if (!m_handle)
                Throw("Error while loading plugin \"%s\": %s", path.string(),
                      dlerror());
        #endif

        try {
            using StringFunc = const char *(*)();
            plugin_name  = ((StringFunc) symbol("plugin_name"))();
            plugin_descr = ((StringFunc) symbol("plugin_descr"))();
        } catch (...) {
            this->~Plugin();
            throw;
        }
    }

    ~Plugin() {
        #if defined(_WIN32)
            FreeLibrary(m_handle);
        #else
            dlclose(m_handle);
        #endif
    }

private:
    void *symbol(const std::string &name) const {
        #if defined(_WIN32)
            void *ptr = GetProcAddress(m_handle, name.c_str());
            if (!ptr)
                Throw("Could not resolve symbol \"%s\" in \"%s\": %s", name,
                      m_path.string(), util::last_error());
        #else
            void *ptr = dlsym(m_handle, name.c_str());
            if (!ptr)
                Throw("Could not resolve symbol \"%s\" in \"%s\": %s", name,
                      m_path.string(), dlerror());
        #endif
        return ptr;
    }

public:
    const char *plugin_name  = nullptr;
    const char *plugin_descr = nullptr;

private:
    #if defined(_WIN32)
        HMODULE m_handle;
    #else
        void *m_handle;
    #endif
    fs::path m_path;
};

struct PluginManager::PluginManagerPrivate {
    std::unordered_map<std::string, Plugin *> m_plugins;
    std::unordered_set<std::string> m_python_plugins;
    std::mutex m_mutex;

    Plugin *plugin(const std::string &name) {
        std::lock_guard<std::mutex> guard(m_mutex);

        // Plugin already loaded?
        auto it = m_plugins.find(name);
        if (it != m_plugins.end())
            return it->second;

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
        fs::path resolved = resolver->resolve(filename);

        if (fs::exists(resolved)) {
            Log(Debug, "Loading plugin \"%s\" ..", filename.string());
            Plugin *plugin = new Plugin(resolved);
            // New classes must be registered within the class hierarchy
            Class::static_initialization();
            // Statistics::instance()->log_plugin(shortName, description()); XXX
            m_plugins[name] = plugin;
            return plugin;
        }

        // Plugin not found!
        Throw("Plugin \"%s\" not found!", name.c_str());
    }
};

ref<PluginManager> PluginManager::m_instance = new PluginManager();

PluginManager::PluginManager() : d(new PluginManagerPrivate()) { }

PluginManager::~PluginManager() {
    std::lock_guard<std::mutex> guard(d->m_mutex);
    for (auto &pair: d->m_plugins)
        delete pair.second;
}

void PluginManager::ensure_plugin_loaded(const std::string &name) {
    (void) d->plugin(name);
}

const Class *PluginManager::get_plugin_class(const std::string &name,
                                             const std::string &variant) {
    const Class *plugin_class;

    auto it = std::find(d->m_python_plugins.begin(), d->m_python_plugins.end(),
                        name + "@" + variant);
    if (it != d->m_python_plugins.end()) {
        plugin_class = Class::for_name(name, variant);
    } else {
        const Plugin *plugin = d->plugin(name);
        plugin_class = Class::for_name(plugin->plugin_name, variant);
    }

    return plugin_class;
}

std::vector<std::string> PluginManager::loaded_plugins() const {
    std::vector<std::string> list;
    std::lock_guard<std::mutex> guard(d->m_mutex);
    for (auto const &pair: d->m_plugins)
        list.push_back(pair.first);
    return list;
}

void PluginManager::register_python_plugin(const std::string &plugin_name,
                                           const std::string &variant) {
    d->m_python_plugins.insert(plugin_name + "@" + variant);
    Class::static_initialization();
}

ref<Object> PluginManager::create_object(const Properties &props,
                                         const Class *class_) {
    Assert(class_ != nullptr);
    if (class_->name() == "Scene")
       return class_->construct(props);

    std::string variant = class_->variant();
    const Class *plugin_class = get_plugin_class(props.plugin_name(), variant);

    /* Construct each plugin in its own scope to isolate them from each other.
     * This is important when plugins are created in parallel. */

    Assert(plugin_class != nullptr);
    ref<Object> object = plugin_class->construct(props);

#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_CUDA)
    // Ensures queued side effects are consistently compiled into cacheable kernels
    if (string::starts_with(variant, "cuda_") ||
        string::starts_with(variant, "llvm_"))
        dr::eval();
#endif

    if (!object->class_()->derives_from(class_)) {
        const Class *oc = object->class_();
        if (oc->parent())
            oc = oc->parent();

        Throw("Type mismatch when loading plugin \"%s\": Expected an instance "
              "of type \"%s\" (variant \"%s\"), got an instance of type \"%s\" "
              "(variant \"%s\")", props.plugin_name(), class_->name(),
              class_->variant(), oc->name(), oc->variant());
    }

   return object;
}

MI_IMPLEMENT_CLASS(PluginManager, Object)

NAMESPACE_END(mitsuba)
