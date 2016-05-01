#pragma once

#include <mitsuba/core/platform.h>
#include <cstdint>
#include <cstring>

NAMESPACE_BEGIN(mitsuba)

/// Cast between types that have an identical binary representation.
template<typename T, typename U> T memcpy_cast(const U &val) {
	static_assert(sizeof(T) == sizeof(U), "memcpy_cast: sizes did not match!");
	T result;
    std::memcpy(&result, &val, sizeof(T));
    return result;
}

NAMESPACE_END(mitsuba)
