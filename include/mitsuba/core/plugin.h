#pragma once

#include <mitsuba/core/object.h>
#include <memory>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief The object factory is responsible for loading plugin modules and
 * instantiating object instances.
 *
 * Ordinarily, this class will be used by making repeated calls to
 * the \ref create_object() methods. The generated instances are then
 * assembled into a final object graph, such as a scene. One such
 * examples is the \ref SceneHandler class, which parses an XML
 * scene file by essentially translating the XML elements into calls
 * to \ref create_object().
 */
class MI_EXPORT_LIB PluginManager : public Object {
public:

    /// Return the global plugin manager
    static PluginManager *instance() { return m_instance; }

    /// Ensure that a plugin is loaded and ready
    void ensure_plugin_loaded(const std::string &name);

    /// Return the class corresponding to a plugin for a specific variant
    const Class *get_plugin_class(const std::string &name,
                                  const std::string &variant);

    /// Return the list of loaded plugins
    std::vector<std::string> loaded_plugins() const;

    /// Register a Python plugin
    void register_python_plugin(const std::string &plugin_name,
                                const std::string &variant);

    /**
     * \brief Instantiate a plugin, verify its type, and return the newly
     * created object instance.
     *
     * \param props
     *     A \ref Properties instance containing all information required to
     *     find and construct the plugin.
     *
     * \param class_type
     *     Expected type of the instance. An exception will be thrown if it
     *     turns out not to derive from this class.
     */
    ref<Object> create_object(const Properties &props, const Class *class_);

    /// Convenience template wrapper around \ref create_object()
    template <typename T> ref<T> create_object(const Properties &props) {
        return static_cast<T *>(create_object(props, MI_CLASS(T)).get());
    }

    MI_DECLARE_CLASS()
protected:
    PluginManager();

    /// Destruct and unload all plugins
    ~PluginManager();
private:
    struct PluginManagerPrivate;
    std::unique_ptr<PluginManagerPrivate> d;
    static ref<PluginManager> m_instance;
};

NAMESPACE_END(mitsuba)
