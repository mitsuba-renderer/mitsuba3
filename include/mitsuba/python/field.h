#pragma once

#include <mitsuba/core/field.h>
#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

// Nanobind type caster for mitsuba::field<...> types. The type caster "unwraps"
// the device value when going from C++ to Python, and conversely wraps it into
// field when going from Python to C++.
template <typename DeviceType, typename HostType>
struct type_caster<mitsuba::field<DeviceType, HostType>> {
    using Value                      = mitsuba::field<DeviceType, HostType>;
    using Caster                     = make_caster<DeviceType>;
    template <typename T> using Cast = Value;
    static constexpr auto Name       = Caster::Name;

    bool from_python(handle src, uint8_t flags,
                     cleanup_list *cleanup) noexcept {
        return caster.from_python(src, flags, cleanup);
    }

    template <typename T>
    static handle from_cpp(T *value, rv_policy policy, cleanup_list *cleanup) {
        if (!value)
            return none().release();
        return from_cpp(*value, policy, cleanup);
    }

    template <typename T>
    static handle from_cpp(T &&value, rv_policy policy,
                           cleanup_list *cleanup) noexcept {
        return Caster::from_cpp(forward_like_<T>(value.value()), policy,
                                cleanup);
    }

    template <typename T> bool can_cast() const noexcept {
        return caster.template can_cast<DeviceType>();
    }

    explicit operator Value() {
        Value result;
        result = caster.operator cast_t<DeviceType>();
        return result;
    }

    Caster caster;
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
