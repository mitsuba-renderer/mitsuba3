#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/random.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/core/bitmap.h>
#include <enoki/morton.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(rt)

extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_planar_morton_scalar(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_planar_morton_packet(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_planar_morton_scalar_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_planar_morton_packet_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_spherical_morton_scalar(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_spherical_morton_packet(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_spherical_morton_scalar_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_spherical_morton_packet_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_planar_independent_scalar(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_planar_independent_packet(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_planar_independent_scalar_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_planar_independent_packet_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_spherical_independent_scalar(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_spherical_independent_packet(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_spherical_independent_scalar_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_pbrt_spherical_independent_packet_shadow(const ShapeKDTree*, uint32_t);

extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_havran_planar_morton_scalar(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_havran_planar_morton_scalar_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_havran_spherical_morton_scalar(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_havran_spherical_morton_scalar_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_havran_planar_independent_scalar(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_havran_planar_independent_scalar_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_havran_spherical_independent_scalar(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_havran_spherical_independent_scalar_shadow(const ShapeKDTree*, uint32_t);

extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_planar_morton_scalar(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_planar_morton_packet(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_planar_morton_scalar_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_planar_morton_packet_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_spherical_morton_scalar(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_spherical_morton_packet(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_spherical_morton_scalar_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_spherical_morton_packet_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_planar_independent_scalar(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_planar_independent_packet(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_planar_independent_scalar_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_planar_independent_packet_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_spherical_independent_scalar(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_spherical_independent_packet(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_spherical_independent_scalar_shadow(const ShapeKDTree*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> rt_dummy_spherical_independent_packet_shadow(const ShapeKDTree*, uint32_t);

NAMESPACE_END(rt)
NAMESPACE_END(mitsuba)
