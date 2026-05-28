#pragma once

#include <ostream>
#include <type_traits>
#include <utility>

#include <mitsuba/core/object.h>
#include <drjit/array.h>
#include <drjit/array_traverse.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Convenience wrapper that keeps scalar host and Dr.Jit device values
 * synchronized
 *
 * This class implements a simple wrapper that replicates instance attributes
 * on the host and device. This is only relevant when \c DeviceType is a
 * JIT-compiled Dr.Jit array (when compiling the renderer in CUDA/LLVM mode).
 *
 * Why is this needed? Mitsuba plugins represent their internal state using
 * attributes like position, intensity, etc., which are typically represented
 * using Dr.Jit arrays. For technical reasons, it is helpful if those attributes
 * are accessible on the host (in which case the lowest-level representation
 * builds on standard C++ types like float or int, for example Point<float, 3>)
 * or the device, whose types invoke the JIT compiler
 * (e.g., Point<CUDAArray<float>, 3>). Copying this data back and forth can be
 * costly if both host and device require simultaneous access. Even if all code
 * effectively runs on the host (e.g. in LLVM mode), accessing "LLVM device"
 * arrays still requires traversal of JIT compiler data structures, which was a
 * severe bottleneck e.g. when Embree calls shape-specific intersection routines.
 */
template <typename DeviceType,
          typename HostType =
              std::decay_t<decltype(dr::slice(std::declval<DeviceType>()))>,
          typename SFINAE = int>
struct synced {};

template <typename DeviceType, typename HostType>
struct synced<DeviceType, HostType,
              dr::enable_if_t<std::is_same_v<DeviceType, HostType>>> {
    synced() {}
    synced(const DeviceType &v) : m_scalar(v) { }
    synced(DeviceType &&v) : m_scalar(std::move(v)) { }
    synced(const synced &) = default;
    synced(synced &&) = default;

    const DeviceType &value() const { return m_scalar; }
    DeviceType &value() { return m_scalar; }
    const DeviceType &scalar() const { return m_scalar; }
    DeviceType *ptr() { return &m_scalar; }
    synced &operator=(const synced &f) {
        m_scalar = f.m_scalar;
        return *this;
    }
    synced &operator=(synced &&f) {
        m_scalar = std::move(f.m_scalar);
        return *this;
    }
    synced &operator=(const DeviceType &v) {
        m_scalar = v;
        return *this;
    }
    synced &operator=(DeviceType &&v) {
        m_scalar = std::move(v);
        return *this;
    }
private:
    DeviceType m_scalar;

public:
    void traverse_1_cb_ro(void * /*payload*/,
                          drjit::detail::traverse_callback_ro) const {}
    void traverse_1_cb_rw(void * /*payload*/,
                          drjit::detail::traverse_callback_rw) {}
};

template <typename DeviceType, typename HostType>
struct synced<DeviceType, HostType,
              dr::enable_if_t<!std::is_same_v<DeviceType, HostType>>> {
    synced() {}
    synced(const HostType &v) : m_value(v), m_scalar(v) { }
    synced(HostType &&v) : m_value(v), m_scalar(std::move(v)) { }
    synced(const synced &) = default;
    synced(synced &&) = default;

    const DeviceType &value() const { return m_value; }
    DeviceType &value() { return m_value; }
    const HostType &scalar() const { return m_scalar; }
    DeviceType *ptr() { return &m_value; }
    synced &operator=(const synced &f) {
        m_value  = f.m_value;
        m_scalar = f.m_scalar;
        return *this;
    }
    synced &operator=(synced &&f) {
        m_value  = std::move(f.m_value);
        m_scalar = std::move(f.m_scalar);
        return *this;
    }
    synced &operator=(const HostType &v) {
        m_value = m_scalar = v;
        return *this;
    }
    synced &operator=(HostType &&v) {
        m_value = v;
        m_scalar = std::move(v);
        return *this;
    }
    synced &operator=(const DeviceType &v) {
        m_value = v;
        m_scalar = dr::slice<HostType>(m_value);
        return *this;
    }
    synced &operator=(DeviceType &&v) {
        m_value = std::move(v);
        m_scalar = dr::slice<HostType>(m_value);
        return *this;
    }
    bool schedule_force_() { return dr::detail::schedule_force(m_value); }
private:
    DeviceType m_value;
    HostType m_scalar;

public:
    void traverse_1_cb_ro(void *payload,
                          drjit::detail::traverse_callback_ro fn) const {
        drjit::traverse_1_fn_ro(m_value, payload, fn);
    }
    void traverse_1_cb_rw(void *payload,
                          drjit::detail::traverse_callback_rw fn) {
        drjit::traverse_1_fn_rw(m_value, payload, fn);
    }
};

/// Prints the canonical string representation of a synced value
template <typename DeviceType, typename HostType>
std::ostream &operator<<(std::ostream &os,
                         const synced<DeviceType, HostType> &f) {
    os << f.scalar();
    return os;
}

// Implementation of TraversalCallback::put() for synced<> objects.
// This is defined here to avoid circular dependencies.
template <typename DeviceType, typename HostType, typename SFINAE,
          typename Flags>
void TraversalCallback::put(
    std::string_view name,
    mitsuba::synced<DeviceType, HostType, SFINAE> &value,
    Flags flags) {

    // Use the device version
    put(name, value.value(), flags);
}

NAMESPACE_END(mitsuba)
