#pragma once

#include <mitsuba/core/object.h>
#include <nanobind/nanobind.h>
#include <tsl/robin_map.h>
#include <string>
#include <string_view>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/**
 * Flattened scene graph backing the `SceneParameters` Python class
 *
 * The constructor of this class takes an ``Object*`` pointer and traverses the
 * graph rooted at this object via ``Object::traverse()`` in DFS order, creating
 * a map that exposes the standard dictionary interface (key enumeration,
 * attribute lookup), as well as extra ones such as obtaining the object owning
 * a particular key.
 */
class ParameterTable {
public:
    /// Create an empty table
    ParameterTable() = default;

    /// Traverse the scene graph rooted at ``node``
    ParameterTable(Object *node);

    /// Number of parameters
    size_t size() const { return m_params.size(); }

    /// Dotted keys of all parameters, in traversal order
    std::vector<std::string> keys() const;

    /// Index of the parameter with the given key, or -1
    int64_t lookup(std::string_view key) const;

    /// Read a parameter
    nanobind::object get(uint32_t index);

    /// Write a parameter
    void set(uint32_t index, nanobind::object value);

    /// Object that reported a parameter, imported into Python on demand
    nanobind::object owner(uint32_t index);

    /// Flags of a parameter
    uint32_t flags(uint32_t index) const { return m_params[index].flags; }

    /**
     * Mark a parameter and its ancestors as modified
     *
     * Each object is recorded together with the name under which the change
     * reached it, which ``update()`` passes on to ``parameters_changed()``.
     */
    void set_dirty(uint32_t index);

    /**
     * Notify the modified objects, children first, and return them
     *
     * Depth-first order places an object before its descendants, hence
     * visiting the node array from the back notifies children first.
     */
    nanobind::list update();

    /// Discard all parameters except those at the given indices
    void keep(const std::vector<uint32_t> &indices);

private:
    /// Hasher for maps with string keys and ``string_view`` lookups
    struct StringHasher {
        using is_transparent = void;
        size_t operator()(std::string_view s) const {
            return std::hash<std::string_view>()(s);
        }
    };

    struct Node {
        /// Traversed object, whose presence here keeps it alive
        ref<Object> object;

        /// Index of the parent node, zero for the root
        uint32_t parent;

        /// Dotted path from the root, empty for the root itself
        std::string path;

        /// Member names modified since the last ``update()``
        std::vector<std::string> dirty;
    };

    struct Param {
        /// Index of the node that reported this parameter
        uint32_t node;

        /// Member name, decorated with an ordinal when siblings collide
        std::string name;

        uint32_t flags;

        /// Address of the parameter within its owner, null for a Python value
        void *ptr;

        /// C++ type of the parameter, null for a Python value
        const std::type_info *type;

        /// A parameter that only exists in Python, reported as ``PyObject *``
        nanobind::object value;
    };

    struct Builder;

    void build(Object *root);

    /// Dotted key of the parameter at ``index``
    std::string key(uint32_t index) const;

    /// Build ``m_index`` on first use
    void ensure_index() const;

    /// Traversed objects, in depth-first order
    std::vector<Node> m_nodes;

    /// Parameters grouped by node, in traversal order
    std::vector<Param> m_params;

    /// Maps a dotted key to its parameter index, built by ``ensure_index()``
    /// when a lookup first needs it
    mutable tsl::robin_map<std::string, uint32_t, StringHasher,
                           std::equal_to<>> m_index;

    /// Values reported by plugins written in Python, which ``Param::ptr``
    /// entries point into
    std::vector<nanobind::object> m_keep_alive;
};

NAMESPACE_END(mitsuba)
