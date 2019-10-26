#include <mitsuba/core/plugin.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fresolver.h>
#include <mutex>
#include <unordered_map>

#if !defined(__WINDOWS__)
# include <dlfcn.h>
#else
# include <windows.h>
#endif

NAMESPACE_BEGIN(mitsuba)

extern "C" {
    typedef Object *(*CreateObjectFunctor)(const Properties &props);
};

class Plugin {
public:
    Plugin(const fs::path &path) : m_path(path) {
        #if defined(__WINDOWS__)
            m_handle = LoadLibraryW(path.native().c_str());
            if (!m_handle)
                Throw("Error while loading plugin \"%s\": %s", path.string(),
                      util::last_error());
        #else
            m_handle = dlopen(path.native().c_str(), RTLD_LAZY | RTLD_LOCAL);
            if (!m_handle)
                Throw("Error while loading plugin \"%s\": %s", path.string(),
                      dlerror());
        #endif

        try {
            create_object = (CreateObjectFunctor) symbol("CreateObject");
#if defined(__AVX512ER__) && defined(__LINUX__)
            static bool knl_warning_once = true;
            auto p0 = (uintptr_t) create_object     & 0xffffffff00000000ull;
            auto p1 = (uintptr_t) &util::core_count & 0xffffffff00000000ull;
            if (p0 != p1 && knl_warning_once) {
                if (getenv("LD_PREFER_MAP_32BIT_EXEC") == nullptr) {
                    std::cerr << "Warning: It is strongly recommended that you set the LD_PREFER_MAP_32BIT_EXEC" << std::endl
                              << "environment variable on Xeon Phi machines to avoid misprediction penalties" << std::endl
                              << "involving function calls across 64 bit boundaries, e.g. to Mitsuba plugins." << std::endl
                              << "To do so, enter" << std::endl << std::endl
                              << "   $ export LD_PREFER_MAP_32BIT_EXEC = 1" << std::endl << std::endl
                              << "before launching Mitsuba (you'll want to put this into your .bashrc as well)." << std::endl << std::endl;
                } else {
                    std::cerr << "Warning: Your version of ld.so doesn't respect the LD_PREFER_MAP_32BIT_EXEC" << std::endl
                              << "flag -- potentially it is too old? (version >= 2.23 is needed)" << std::endl;
                }
                knl_warning_once = false;
            }
#endif
        } catch (...) {
            this->~Plugin();
            throw;
        }
    }

    ~Plugin() {
        #if defined(__WINDOWS__)
            FreeLibrary(m_handle);
        #else
            dlclose(m_handle);
        #endif
    }

    void *symbol(const std::string &name) const {
        #if defined(__WINDOWS__)
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

    CreateObjectFunctor create_object = nullptr;

private:
    #if defined(__WINDOWS__)
        HMODULE m_handle;
    #else
        void *m_handle;
    #endif
    fs::path m_path;
};

struct PluginManager::PluginManagerPrivate {
    std::unordered_map<std::string, Plugin *> m_plugins;
    std::mutex m_mutex;

    Plugin *plugin(const std::string &name) {
        std::lock_guard<std::mutex> guard(m_mutex);

        /* Plugin already loaded? */
        auto it = m_plugins.find(name);
        if (it != m_plugins.end())
            return it->second;

        /* Build the full plugin file name */
        fs::path filename = fs::path("plugins") / name;

        #if defined(__WINDOWS__)
            filename.replace_extension(".dll");
        #elif defined(__OSX__)
            filename.replace_extension(".dylib");
        #else
            filename.replace_extension(".so");
        #endif

        const FileResolver *resolver = Thread::thread()->file_resolver();
        fs::path resolved = resolver->resolve(filename);

        if (fs::exists(resolved)) {
            Log(EInfo, "Loading plugin \"%s\" ..", filename.string());
            Plugin *plugin = new Plugin(resolved);
            /* New classes must be registered within the class hierarchy */
            Class::static_initialization();
            ///Statistics::instance()->log_plugin(shortName, description()); XXX
            m_plugins[name] = plugin;
            return plugin;
        }

        /* Plugin not found! */
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

ref<Object> PluginManager::create_object(const Properties &props, const Class *class_) {
   if (class_->name() == "Scene")
       return class_->construct(props);

   const Plugin *plugin = d->plugin(props.plugin_name());
   ref<Object> object = plugin->create_object(props);
   if (!object->class_()->derives_from(class_)) {
        const Class *oc = object->class_();
        if (oc->parent())
            oc = oc->parent();

        Throw("Type mismatch when loading plugin \"%s\": Expected "
              "an instance of type \"%s\", got an instance of type \"%s\"",
              props.plugin_name(), class_->name(), oc->name());
    }

   return object;
}

ref<Object> PluginManager::create_object(const Properties &props) {
    const Plugin *plugin = d->plugin(props.plugin_name());
    return plugin->create_object(props);
}

std::vector<std::string> PluginManager::loaded_plugins() const {
    std::vector<std::string> list;
    std::lock_guard<std::mutex> guard(d->m_mutex);
    for (auto const &pair: d->m_plugins)
        list.push_back(pair.first);
    return list;
}

void PluginManager::ensure_plugin_loaded(const std::string &name) {
    (void) d->plugin(name);
}

NAMESPACE_END(mitsuba)
