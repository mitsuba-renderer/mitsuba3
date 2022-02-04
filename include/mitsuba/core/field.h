
#pragma once

#include <ostream>
#include <type_traits>

#include <drjit/array_router.h>
namespace dr = drjit;

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Convenience wrapper to simultaneously instantiate a host and a device
 * version of a type
 *
 * This class implements a simple wrapper that replicates instance attributes
 * on the host and device. This is only relevant when \c DeviceType is a
 * JIT-compiled Dr.Jit array (when compiling the renderer in CUDA/LLVM mode).
 *
 * Why is this needed? Mitsuba plugins represent their internal state using
 * attributes like position, intensity, etc., which are typically represented
 * using Dr.Jit arrays. For technical reasons, it is helpful if those fields are
 * both accessible on the host (in which case the lowest-level representation
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
struct field {};

template <typename DeviceType, typename HostType>
struct field<DeviceType, HostType,
             dr::enable_if_t<std::is_same_v<DeviceType, HostType>>> {
    field() {}
    field(const DeviceType &v) : m_scalar(v) { }
    field(DeviceType &&v) : m_scalar(v) { }
    const DeviceType& value()  const { return m_scalar; }
    const DeviceType& scalar() const { return m_scalar; }
    DeviceType* ptr() { return &m_scalar; }
    field& operator=(const field& f) {
        m_scalar = f.m_scalar;
        return *this;
    }
    field& operator=(field&& f) {
        m_scalar = std::move(f.m_scalar);
        return *this;
    }
    field& operator=(const DeviceType &v) {
        m_scalar = v;
        return *this;
    }
    field& operator=(DeviceType &&v) {
        m_scalar = v;
        return *this;
    }
private:
    DeviceType m_scalar;
};

template <typename DeviceType, typename HostType>
struct field<DeviceType, HostType,
             dr::enable_if_t<!std::is_same_v<DeviceType, HostType>>> {
    field() {}
    field(const HostType &v) : m_value(v), m_scalar(v) { }
    field(HostType &&v) : m_value(v), m_scalar(v) { }
    const DeviceType& value()  const { return m_value; }
    const HostType& scalar() const { return m_scalar; }
    DeviceType* ptr() { return &m_value; }
    field& operator=(const field& f) {
        m_value  = f.m_value;
        m_scalar = f.m_scalar;
        return *this;
    }
    field& operator=(field&& f) {
        m_value  = std::move(f.m_value);
        m_scalar = std::move(f.m_scalar);
        return *this;
    }
    field& operator=(const HostType &v) {
        m_value = m_scalar = v;
        return *this;
    }
    field& operator=(HostType &&v) {
        m_value = m_scalar = v;
        return *this;
    }
    field& operator=(const DeviceType &v) {
        m_value = v;
        m_scalar = dr::slice<HostType>(m_value);
        return *this;
    }
    field& operator=(DeviceType &&v) {
        m_value = v;
        m_scalar = dr::slice<HostType>(m_value);
        return *this;
    }
    void opaque_() { dr::make_opaque(m_value); }
private:
    DeviceType m_value;
    HostType m_scalar;
};

/// Prints the canonical string representation of a field
template <typename DeviceType, typename HostType>
std::ostream &operator<<(std::ostream &os,
                         const field<DeviceType, HostType> &f) {
    os << f.scalar();
    return os;
}

NAMESPACE_END(mitsuba)
