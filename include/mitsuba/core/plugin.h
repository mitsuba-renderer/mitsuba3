#pragma once

#include <drjit-core/nanostl.h>
#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)

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
    /// Return the global plugin manager
    static PluginManager *instance() { return m_instance; }

    /**
     * \brief Register a new plugin variant with the plugin manager
     *
     * Re-registering an already available (name, variant) pair is legal
     * and will release the previously registered variant.
     *
     * \param name
     *      The name of the plugin.
     *
     * \param variant
     *     The plugin variant (e.g., <tt>"scalar_rgb"</tt>). Separate
     *     plugin variants must be registered independently.
     *
     * \param instantiate
     *     A callback that creates an instance of the plugin.
     *
     * \param release
     *     A callback that releases any (global) plugin state.
     *     Will, e.g., be called by \ref PluginManager::reset().
     *
     * \param payload
     *     An opaque pointer parameter that will be forwarded
     *     to both \c instantiate and \c release.
     */
    void register_plugin(std::string_view name, std::string_view variant,
                         ObjectType type, PluginInstantiateFn instantiate,
                         PluginReleaseFn release, void *payload);

    /**
     * \brief Release registered plugins
     *
     * This calls the \ref PluginInfo::release callback of all registered
     * plugins, e.g., to enable garbage collection of classes in python. Note
     * dynamically loaded shared libraries of native plugins aren't unloaded
     * until the PluginManager class itself is destructed.
     */
    void release_all();

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
     *     here will produce an error message. Pass `ObjectType::Unknown`
     *     to disable this check.
     */
    ref<Object> create_object(const Properties &props,
                              std::string_view variant,
                              ObjectType type);

    /**
     * \brief Create a plugin object with the provided information
     *
     * This template function wraps the ordinary <tt>create_object()</tt>
     * function defined above. It automatically infers variant and object
     * type from the provided class 'T'.
     */
    template <typename T> ref<T> create_object(const Properties &props) {
        return ref<T>((T *) create_object(props, T::Variant, T::Type).get());
    }

    /// Get the type of a plugin by name, or return Object::Unknown if unknown.
    ObjectType plugin_type(std::string_view name);

    MI_DECLARE_CLASS(PluginManager)

protected:
    PluginManager();
    ~PluginManager();

private:
    struct PluginManagerPrivate;
    dr::unique_ptr<PluginManagerPrivate> d;
    static ref<PluginManager> m_instance;
};

/// Get the XML tag name for an ObjectType (e.g. "scene", "bsdf")
extern MI_EXPORT_LIB std::string_view plugin_type_name(ObjectType ot);

NAMESPACE_END(mitsuba)
