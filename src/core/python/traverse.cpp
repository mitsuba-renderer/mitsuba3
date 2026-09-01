#include <mitsuba/core/logger.h>
#include <mitsuba/core/traverse.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>
#include <drjit-core/hash.h>
#include <tsl/robin_set.h>
#include <algorithm>
#include <cstring>

extern nb::object cast_object(Object *o);

#define TRY_SCALAR_GET(T)                                                      \
    if (*type == typeid(T))                                                    \
        return nb::cast(*(T *) ptr)

#define TRY_SCALAR_SET(T)                                                      \
    if (*type == typeid(T)) {                                                  \
        *(T *) ptr = nb::cast<T>(src);                                         \
        return;                                                                \
    }

NAMESPACE_BEGIN(mitsuba)

/// Return a Python object for a value of type ``type`` at address ``ptr``,
/// tying its lifetime to ``parent`` when one is given
static nb::object get_property(void *ptr, const std::type_info *type,
                               nb::handle parent) {
    TRY_SCALAR_GET(float);
    TRY_SCALAR_GET(double);
    TRY_SCALAR_GET(bool);
    TRY_SCALAR_GET(uint32_t);
    TRY_SCALAR_GET(int32_t);

    nb::rv_policy rvp = parent.is_valid() ? nb::rv_policy::reference_internal_v
                                          : nb::rv_policy::reference_v;
    nb::detail::cleanup_list cleanup(parent.ptr());

    nb::object r = nb::steal(NB_CALL(nb_type_put)(
        NB_CTX, type, nullptr, ptr, rvp, &cleanup, nullptr));

    if (!r.is_valid())
        Throw("get_property(): unsupported type \"%s\"!", type->name());

    cleanup.release();
    return r;
}

/// Overwrite a value of type ``type`` at address ``ptr`` with the contents
/// of a compatible Python object
static void set_property(void *ptr, const std::type_info *type,
                         nb::object src) {
    TRY_SCALAR_SET(float);
    TRY_SCALAR_SET(double);
    TRY_SCALAR_SET(bool);
    TRY_SCALAR_SET(uint32_t);
    TRY_SCALAR_SET(int32_t);

    nb::object dst = get_property(ptr, type, nb::handle());
    if (!dst.type().is(src.type()))
        src = dst.type()(src);
    nb::inst_replace_copy(dst, src);
}

/// Return the Python wrapper of ``o`` or create it on demand.
static nb::object import_object(Object *o) {
    if (PyObject *py = o->self_py())
        return nb::borrow(nb::handle(py));
    return cast_object(o);
}

// =========================================================================

/// Collects the members of one object
struct ParameterTable::Builder : TraversalCallback {
    ParameterTable &table;

    /// Index of the node whose members are being collected, with its
    /// inherited flags cached out of the node array
    uint32_t node = 0;
    uint32_t node_flags = 0;

    /// Work list of ``build()``
    struct Item {
        Object *object;
        std::string path;
        uint32_t parent;
        uint32_t flags;
    };
    std::vector<Item> stack;

    /// Use count of a member name, kept in ``seen``
    struct Count {
        /// Node that the count belongs to, as a value of ``Builder::epoch``
        uint32_t epoch = 0;

        /// Times the name was used as a member of that node
        uint32_t count = 0;
    };

    /// Member names of the current node. ``decorate()`` resolves duplicates
    /// among these members only, since names below any other node already
    /// differ in their dotted prefix.
    tsl::robin_map<std::string, Count, StringHasher, std::equal_to<>> seen;

    /// Advancing this counter invalidates all entries of ``seen`` to avoid
    /// having to clear the map, which is slow.
    uint32_t epoch = 0;

    Builder(ParameterTable &table) : table(table) { }

    /// Use count of ``name`` within the current node, incremented by the call
    uint32_t bump(std::string_view name) {
        auto it = seen.find(name);
        if (it == seen.end())
            it = seen.emplace(std::string(name), Count()).first;
        Count &c = it.value();
        if (c.epoch != epoch) {
            c.epoch = epoch;
            c.count = 0;
        }
        return c.count++;
    }

    /**
     * Return a unique name for a member reported as ``name``
     *
     * The first occurrence of a name keeps it and later ones gain a numeric
     * suffix. An unnamed member is named after the class of ``obj`` and
     * always carries a suffix. A suffixed candidate that collides with an
     * explicitly written sibling of the same name is skipped.
     */
    std::string decorate(std::string_view name, const Object *obj) {
        bool named = !name.empty();
        if (!named && obj)
            name = obj->class_name();

        uint32_t ordinal = bump(name);
        if (named && ordinal == 0)
            return std::string(name);

        while (true) {
            std::string candidate =
                std::string(name) + '_' + std::to_string(ordinal);
            if (bump(candidate) == 0)
                return candidate;
            ordinal = bump(name);
        }
    }

    void put_value(std::string_view name, void *ptr, uint32_t flags,
                   const std::type_info &type) override {
        uint32_t merged = node_flags | flags;
        if (merged & (uint32_t) ParamFlags::NonDifferentiable)
            merged &= ~(uint32_t) ParamFlags::Discontinuous;

        // Compared by name, since the variant modules that report a value
        // and this module do not share a type_info instance
        bool python = strcmp(type.name(), typeid(PyObject *).name()) == 0;
        table.m_params.push_back(
            { node,
              decorate(name, nullptr),
              merged,
              python ? nullptr : ptr,
              python ? nullptr : &type,
              python ? nb::borrow(nb::handle((PyObject *) ptr)) : nb::object() });
    }

    void keep_alive(void *python_object) override {
        table.m_keep_alive.push_back(
            nb::borrow(nb::handle((PyObject *) python_object)));
    }

    void put_object(std::string_view name, Object *value,
                    uint32_t flags) override {
        if (!value)
            return;
        std::string path = table.m_nodes[node].path;
        if (!path.empty())
            path += '.';
        path += decorate(name, value);
        stack.push_back({ value, std::move(path), node, node_flags | flags });
    }
};

ParameterTable::ParameterTable(Object *node) { build(node); }

void ParameterTable::build(Object *root) {
    // Objects reached a second time are not described again
    tsl::robin_set<const Object *, PointerHasher> visited;

    Builder b(*this);
    b.stack.push_back({ root, std::string(), 0, 0 });

    while (!b.stack.empty()) {
        Builder::Item item = std::move(b.stack.back());
        b.stack.pop_back();

        if (!visited.emplace(item.object).second)
            continue;

        b.node = (uint32_t) m_nodes.size();
        b.node_flags = item.flags;
        b.epoch++;
        m_nodes.push_back({ ref<Object>(item.object), item.parent,
                            std::move(item.path), {} });

        size_t mark = b.stack.size();
        item.object->traverse(&b);
        std::reverse(b.stack.begin() + mark, b.stack.end());
    }
}

void ParameterTable::ensure_index() const {
    if (!m_index.empty() || m_params.empty())
        return;

    m_index.reserve(m_params.size());
    for (uint32_t i = 0; i < (uint32_t) m_params.size(); ++i)
        m_index.emplace(key(i), i);
}

std::string ParameterTable::key(uint32_t index) const {
    const Param &p = m_params[index];
    const std::string &path = m_nodes[p.node].path;
    if (path.empty())
        return p.name;

    std::string result;
    result.reserve(path.size() + 1 + p.name.size());
    result += path;
    result += '.';
    result += p.name;
    return result;
}

std::vector<std::string> ParameterTable::keys() const {
    std::vector<std::string> result;
    result.reserve(m_params.size());
    for (uint32_t i = 0; i < (uint32_t) m_params.size(); ++i)
        result.push_back(key(i));
    return result;
}

int64_t ParameterTable::lookup(std::string_view k) const {
    ensure_index();

    auto it = m_index.find(k);
    if (it == m_index.end())
        return -1;
    return it->second;
}

nb::object ParameterTable::owner(uint32_t index) {
    return import_object(m_nodes[m_params[index].node].object.get());
}

nb::object ParameterTable::get(uint32_t index) {
    const Param &p = m_params[index];
    if (!p.type)
        return p.value;

    return get_property(p.ptr, p.type, nb::find(this));
}

void ParameterTable::set(uint32_t index, nb::object value) {
    const Param &p = m_params[index];

    // A parameter that only exists in Python is not writable in place
    if (!p.type) {
        Log(Warn, "Parameter \"%s\" cannot be modified! This usually happens "
                  "when the parameter is not a Mitsuba type. Please use "
                  "non-scalar Mitsuba types in your custom plugins.",
            key(index));
        return;
    }

    set_property(p.ptr, p.type, std::move(value));
}

void ParameterTable::set_dirty(uint32_t index) {
    // Record a modified member name of a node, ignoring duplicates
    auto mark = [this](uint32_t node, std::string_view name) {
        std::vector<std::string> &dirty = m_nodes[node].dirty;
        for (const std::string &s : dirty)
            if (s == name)
                return;
        dirty.emplace_back(name);
    };

    const Param &p = m_params[index];
    mark(p.node, p.name);

    for (uint32_t n = p.node; n != 0; n = m_nodes[n].parent) {
        // The name under which 'n' is known to its parent is the last
        // segment of its path (rfind returns npos at the top level)
        const std::string &path = m_nodes[n].path;
        mark(m_nodes[n].parent,
             std::string_view(path).substr(path.rfind('.') + 1));
    }
}

nb::list ParameterTable::update() {
    nb::list result;
    for (size_t i = m_nodes.size(); i-- > 0; ) {
        Node &n = m_nodes[i];
        if (n.dirty.empty())
            continue;

        n.object->parameters_changed(n.dirty);

        nb::set names;
        for (const std::string &s : n.dirty)
            names.add(s);
        result.append(nb::make_tuple(import_object(n.object.get()), names));

        n.dirty.clear();
    }
    return result;
}

void ParameterTable::keep(const std::vector<uint32_t> &indices) {
    std::vector<Param> params;
    params.reserve(indices.size());
    for (uint32_t i : indices)
        params.push_back(m_params[i]);
    m_params = std::move(params);

    m_index.clear();
}

NAMESPACE_END(mitsuba)

MI_PY_EXPORT(ParameterTable) {
    nb::class_<ParameterTable>(m, "ParameterTable", D(ParameterTable))
        .def(nb::init<>(), D(ParameterTable, ParameterTable))
        .def(nb::init<Object *>(), "node"_a, D(ParameterTable, ParameterTable, 2))
        .def("__len__", &ParameterTable::size)
        .def("__copy__", [](const ParameterTable &t) {
            return ParameterTable(t);
        })
        .def("keys", &ParameterTable::keys, D(ParameterTable, keys))
        .def("lookup", &ParameterTable::lookup, "key"_a,
             D(ParameterTable, lookup))
        .def("get", &ParameterTable::get, "index"_a, D(ParameterTable, get))
        .def("set", &ParameterTable::set, "index"_a, "value"_a,
             D(ParameterTable, set))
        .def("owner", &ParameterTable::owner, "index"_a,
             D(ParameterTable, owner))
        .def("flags", &ParameterTable::flags, "index"_a,
             D(ParameterTable, flags))
        .def("set_dirty", &ParameterTable::set_dirty, "index"_a,
             D(ParameterTable, set_dirty))
        .def("update", &ParameterTable::update, D(ParameterTable, update))
        .def("keep", &ParameterTable::keep, "indices"_a,
             D(ParameterTable, keep));
}
