#pragma once

/**
 * Mitsuba's structured data conversion facilities are provided by the
 * standalone *struct-jit* library (see ``ext/struct-jit``). This header
 * pulls in that library and exposes a small number of Mitsuba-side
 * conveniences:
 *
 *  - the namespace alias ``sj`` for ``struct_jit``,
 *  - the ``struct_type_v`` variable template (a compile-time C++ type to
 *    ``struct_jit::Type`` mapping that also understands ``drjit::half``), and
 *  - the ``type_id`` specialization that teaches struct-jit about ``drjit::half``.
 *
 * The former ``mitsuba::Struct`` and ``mitsuba::StructConverter`` classes have
 * been removed; use ``struct_jit::Struct``, ``struct_jit::Converter`` and
 * ``struct_jit::make_converter()`` instead (conventionally via the ``sj`` alias).
 */

#include <struct-jit/struct-jit.h>
#include <drjit/array.h>
#include <drjit-core/half.h>

NAMESPACE_BEGIN(struct_jit)
/// Teach struct-jit's compile-time type trait about Dr.Jit's half type
template <> struct type_id<drjit::half> {
    static constexpr Type value = Type::Float16;
};
NAMESPACE_END(struct_jit)

NAMESPACE_BEGIN(mitsuba)

/// Convenient short alias for the struct-jit namespace
namespace sj = struct_jit;

/**
 * Compile-time mapping from a C++ scalar type to the corresponding
 * ``struct_jit::Type`` (e.g. ``struct_type_v<float>`` == ``sj::Type::Float32``).
 */
template <typename T>
constexpr sj::Type struct_type_v = sj::type_v<drjit::scalar_t<T>>;

NAMESPACE_END(mitsuba)
