#pragma once

/**
 * \file struct.h
 *
 * Mitsuba's structured data conversion facilities are provided by the
 * standalone <em>struct-jit</em> library (see \c ext/struct-jit). This header
 * pulls in that library and exposes a small number of Mitsuba-side
 * conveniences:
 *
 *  - the namespace alias \c sj for \c struct_jit,
 *  - the \ref struct_type_v variable template (a compile-time C++ type to
 *    \c struct_jit::Type mapping that also understands \c drjit::half), and
 *  - the \c type_id specialization that teaches struct-jit about \c drjit::half.
 *
 * The former \c mitsuba::Struct and \c mitsuba::StructConverter classes have
 * been removed; use \c struct_jit::Struct, \c struct_jit::Converter and
 * \c struct_jit::make_converter() instead (conventionally via the \c sj alias).
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
 * \brief Compile-time mapping from a C++ scalar type to the corresponding
 * \c struct_jit::Type (e.g. \c struct_type_v<float> == \c sj::Type::Float32).
 */
template <typename T>
constexpr sj::Type struct_type_v = sj::type_v<drjit::scalar_t<T>>;

NAMESPACE_END(mitsuba)
