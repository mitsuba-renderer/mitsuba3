#pragma once

#include <mitsuba/core/object.h>
#include <vector>
#include <memory>

NAMESPACE_BEGIN(mitsuba)

/** /// XXX update
 * \brief The object factory is responsible for loading plugin modules and
 * instantiating object instances.
 *
 * Ordinarily, this class will be used by making repeated calls to
 * the \ref createObject() methods. The generated instances are then
 * assembled into a final object graph, such as a scene. One such
 * examples is the \ref SceneHandler class, which parses an XML
 * scene file by esentially translating the XML elements into calls
 * to \ref createObject().
 *
 * Since this kind of construction method can be tiresome when
 * dynamically building scenes from Python, this class has an
 * additional Python-only method \c create(), which works as follows:
 *
 * \code
 * from mitsuba.core import *
 *
 * pmgr = PluginManager.getInstance()
 * camera = pmgr.create({
 *     "type" : "perspective",
 *     "toWorld" : Transform.lookAt(
 *         Point(0, 0, -10),
 *         Point(0, 0, 0),
 *         Vector(0, 1, 0)
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
 * returns, the object hierarchy has already been assembled, and the
 * \ref ConfigurableObject::configure() methods of every object
 * has been called.
 */
class MTS_EXPORT_CORE PluginManager : public Object {
public:
    /// Return the global plugin manager
    static PluginManager *instance() { return m_instance; }

    /// Ensure that a plugin is loaded and ready
    void ensurePluginLoaded(const std::string &name);

    /// Return the list of loaded plugins
    std::vector<std::string> loadedPlugins() const;

    /**
     * \brief Instantiate a plugin, verify its type, and return the newly
     * created object instance.
     *
     * \param classType
     *     Expected type of the instance. An exception will be thrown if it
     *     turns out not to derive from this class.
     *
     * \param props
     *     A \ref Properties instance containing all information required to
     *     find and construct the plugin.
     */
    ref<Object> createObject(const Class *classType, const Properties &props);

    /**
     * \brief Instantiate a plugin and return the new instance (without
     * verifying its type).
     *
     * \param props
     *    A \ref Properties instance containing all information required to
     *    find and construct the plugin.
     */
    ref<Object> createObject(const Properties &props);

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
