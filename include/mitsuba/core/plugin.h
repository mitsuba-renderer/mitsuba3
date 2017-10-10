#pragma once

#include <mitsuba/core/object.h>
#include <vector>
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
 * scene file by esentially translating the XML elements into calls
 * to \ref create_object().
 *
 * Since this kind of construction method can be tiresome when
 * dynamically building scenes from Python, this class has an
 * additional Python-only method \c create(), which works as follows:
 *
 * \code
 * from mitsuba.core import *
 *
 * pmgr = PluginManager.instance()
 * camera = pmgr.create({
 *     "type" : "perspective",
 *     "to_world" : Transform.look_at(
 *         Point3f(0, 0, -10),
 *         Point3f(0, 0, 0),
 *         Vector3f(0, 1, 0)
 *     ),
 *     "film" : {
 *         "type" : "ldrfilm",
 *         "width" : 1920,
 *         "height" : 1080
 *     }
 * })
 * \endcode
 *
 * The above snippet constructs a \ref Camera instance from a
 * plugin named \c perspective.so/dll/dylib and adds a child object
 * named \c film, which is a \ref Film instance loaded from the
 * plugin \c ldrfilm.so/dll/dylib. By the time the function
 * returns, the object hierarchy has already been assembled.
 */
class MTS_EXPORT_CORE PluginManager : public Object {
public:
    /// Return the global plugin manager
    static PluginManager *instance() { return m_instance; }

    /// Ensure that a plugin is loaded and ready
    void ensure_plugin_loaded(const std::string &name);

    /// Return the list of loaded plugins
    std::vector<std::string> loaded_plugins() const;

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
        return static_cast<T *>(create_object(props, MTS_CLASS(T)).get());
    }

    /**
     * \brief Instantiate a plugin and return the new instance (without
     * verifying its type).
     *
     * \param props
     *    A \ref Properties instance containing all information required to
     *    find and construct the plugin.
     */
    ref<Object> create_object(const Properties &props);

    MTS_DECLARE_CLASS()
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
