#include <mitsuba/core/math.h>
#include <enoki/morton.h>
#include <stdexcept>
#include <cassert>

#ifdef _MSC_VER
#  pragma warning(disable: 4305 4838 4244) // 8: conversion from 'double' to 'const float' requires a narrowing conversion, etc.
#endif
