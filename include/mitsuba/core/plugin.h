#pragma once

#include <drjit-core/nanostl.h>
#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)

// Metadata needed to register a new plugin with Mitsuba
struct PluginInfo {
    // Compact plugin name used in scene descriptions (e.g. 'spot')
    const char *name;

    // Supported variant (e.g. 'scalar_rgb')
    const char *variant;

    // Opaque payload to be passed to the callbacks below
    void *payload;

    // Instantiate a plugin with specified properties
    PluginInstantiateFn instantiate;

    // Release all resources of the plugin
    PluginReleaseFn release;
};

/**
 * \brief Plugin manager
 *
 * The plugin manager's main feature is the \ref create_object() function that
 * instantiates scene objects. To do its job, it loads external Mitsuba plugins
 * as needed.
 *
 * When used from Python, it is also possible to register external plugins so
 * that they can be instantiated analogously.
 */
class MI_EXPORT_LIB PluginManager : public Object {
public:
    /// Destruct and unload all plugins
    ~PluginManager();

    /// Return the global plugin manager
    static PluginManager *instance() { return m_instance; }

    /// Release all registered plugins
    void reset();

    /// Register a new plugin with the plugin manager
    void register_plugin(PluginInfo info);

    /**
     * \brief Instantiate a plugin, verify its type, and return the newly
     * created object instance.
     *
     * \param props
     *     A \ref Properties instance containing all information required to
     *     find and construct the plugin.
     */
    ref<Object> create_object(const char *variant,
                              ObjectType type,
                              const Properties &props);

    MI_DECLARE_CLASS(PluginManager)

protected:
    PluginManager();

    // Called when an instantiated plugin doesn't match the expected type
    [[noreturn]] void raise_type_error(const Properties &props,
                                       const char *actual,
                                       const char *expected);

private:
    struct PluginManagerPrivate;
    dr::unique_ptr<PluginManagerPrivate> d;
    static ref<PluginManager> m_instance;
};

NAMESPACE_END(mitsuba)
