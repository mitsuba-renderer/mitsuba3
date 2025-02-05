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
     * \brief Create a plugin object with the provided information
     *
     * This function potentially loads an external plugin module (if not
     * already present), creates an instance, verifies its type, and finally
     * returns the newly created object instance.
     *
     * \param props
     *     A \ref Properties instance containing all information required to
     *     find and construct the plugin.
     *
     * \param variant
     *     The variant (e.g. 'scalar_rgb') of the plugin to instantiate
     *
     * \param type
     *     The expected interface of the instantiated plugin. Mismatches
     *     here will produce an error message.
     */
    ref<Object> create_object(const Properties &props,
                              const char *variant,
                              ObjectType type);

    /**
     * \brief Create a plugin object with the provided information
     *
     * This template function wraps the ordinary <tt>create_object()</tt>
     * function defined above. It automatically infers variant and object
     * type from the provided class 'T'.
     */
    template <typename T> ref<T> create_object(const Properties &props) {
        return create_object(
            props,
            detail::get_variant<typename T::Float, typename T::Spectrum>(),
            T::Type);
    }

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
