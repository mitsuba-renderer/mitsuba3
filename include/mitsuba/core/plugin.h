#pragma once

#include <drjit-core/nanostl.h>
#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Plugin manager
 *
 * The plugin manager's main feature is the `create_object()` function that
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
     * Register a new plugin variant with the plugin manager
     *
     * Re-registering an already available (name, variant) pair is legal
     * and will release the previously registered variant.
     *
     * Args:
     *     name: The name of the plugin.
     *
     *     variant: The plugin variant (e.g., ``"scalar_rgb"``). Separate
     *         plugin variants must be registered independently.
     *
     *     instantiate: A callback that creates an instance of the plugin.
     *
     *     release: A callback that releases any (global) plugin state.
     *         Will, e.g., be called by `PluginManager.release_all()`.
     *
     *     payload: An opaque pointer parameter that will be forwarded
     *         to both ``instantiate`` and ``release``.
     */
    void register_plugin(std::string_view name, std::string_view variant,
                         ObjectType type, PluginInstantiateFn instantiate,
                         PluginReleaseFn release, void *payload);

    /**
     * Release registered plugins
     *
     * This calls the ``release`` callback of all registered
     * plugins, e.g., to enable garbage collection of classes in python. Note
     * dynamically loaded shared libraries of native plugins aren't unloaded
     * until the PluginManager class itself is destructed.
     */
    void release_all();

    /**
     * Create a plugin object with the provided information
     *
     * This function potentially loads an external plugin module (if not
     * already present), creates an instance, verifies its type, and finally
     * returns the newly created object instance.
     *
     * Args:
     *     props: A `Properties` instance containing all information required to
     *         find and construct the plugin.
     */
    ref<Object> create_object(const Properties &props,
                              std::string_view variant,
                              ObjectType type);

    /**
     * Create a plugin object with the provided information
     *
     * This template function wraps the ordinary ``create_object()``
     * function defined above. It automatically infers variant and object
     * type from the provided class ``T``.
     */
    template <typename T> ref<T> create_object(const Properties &props) {
        ref<Object> object = create_object(props, T::Variant, T::Type);
        T *result = dynamic_cast<T *>(object.get());
        if (!result)
            throw std::runtime_error(
                "Type mismatch: instantiated plugin does not implement the "
                "requested C++ interface.");
        return ref<T>(result);
    }

    /// Get the type of a plugin by name, or return `ObjectType.Unknown` if unknown.
    ObjectType plugin_type(std::string_view name);

    /// Get the type of a plugin by name and variant, or return Object::Unknown if unknown.
    ObjectType plugin_type(std::string_view name, std::string_view variant);

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
