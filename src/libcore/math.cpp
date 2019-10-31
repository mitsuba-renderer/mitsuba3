#include <mitsuba/core/math.h>
#include <enoki/morton.h>
#include <stdexcept>
#include <cassert>

#ifdef _MSC_VER
#  pragma warning(disable: 4305 4838 4244) // 8: conversion from 'double' to 'const float' requires a narrowing conversion, etc.
#endif

#if 0
NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(math)

template MTS_EXPORT_CORE Float  legendre_p(int, Float);
template MTS_EXPORT_CORE FloatP legendre_p(int, FloatP);
template MTS_EXPORT_CORE Float  legendre_p(int, int, Float);
template MTS_EXPORT_CORE FloatP legendre_p(int, int, FloatP);
template MTS_EXPORT_CORE std::pair<Float, Float>   legendre_pd(int, Float);
template MTS_EXPORT_CORE std::pair<FloatP, FloatP> legendre_pd(int, FloatP);
template MTS_EXPORT_CORE std::pair<Float, Float>   legendre_pd_diff(int, Float);
template MTS_EXPORT_CORE std::pair<FloatP, FloatP> legendre_pd_diff(int, FloatP);

template MTS_EXPORT_CORE std::tuple<mask_t<Float>,  Float,  Float>  solve_quadratic(const Float &,  const Float &,  const Float &);
template MTS_EXPORT_CORE std::tuple<mask_t<FloatP>, FloatP, FloatP> solve_quadratic(const FloatP &, const FloatP &, const FloatP &);

NAMESPACE_END(math)
NAMESPACE_END(mitsuba)

NAMESPACE_BEGIN(enoki)

using mitsuba::UInt32P;
using mitsuba::UInt64P;

template MTS_EXPORT_CORE uint32_t morton_encode(Array<uint32_t, 2>);
template MTS_EXPORT_CORE UInt32P  morton_encode(Array<UInt32P, 2>);
template MTS_EXPORT_CORE uint32_t morton_encode(Array<uint32_t, 3>);
template MTS_EXPORT_CORE UInt32P  morton_encode(Array<UInt32P, 3>);

template MTS_EXPORT_CORE uint64_t morton_encode(Array<uint64_t, 2>);
template MTS_EXPORT_CORE UInt64P  morton_encode(Array<UInt64P, 2>);
template MTS_EXPORT_CORE uint64_t morton_encode(Array<uint64_t, 3>);
template MTS_EXPORT_CORE UInt64P  morton_encode(Array<UInt64P, 3>);

template MTS_EXPORT_CORE Array<uint32_t, 2> morton_decode(uint32_t);
template MTS_EXPORT_CORE Array<UInt32P,  2> morton_decode(UInt32P);
template MTS_EXPORT_CORE Array<uint32_t, 3> morton_decode(uint32_t);
template MTS_EXPORT_CORE Array<UInt32P,  3> morton_decode(UInt32P);

template MTS_EXPORT_CORE Array<uint64_t, 2> morton_decode(uint64_t);
template MTS_EXPORT_CORE Array<UInt64P,  2> morton_decode(UInt64P);
template MTS_EXPORT_CORE Array<uint64_t, 3> morton_decode(uint64_t);
template MTS_EXPORT_CORE Array<UInt64P,  3> morton_decode(UInt64P);

NAMESPACE_END(enoki)

#endif