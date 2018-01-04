#pragma once

#include <utility>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(rtbench)

extern MTS_EXPORT_RENDER std::pair<Float, size_t> planar_morton_scalar(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> planar_morton_packet(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> planar_morton_scalar_shadow(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> planar_morton_packet_shadow(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> spherical_morton_scalar(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> spherical_morton_packet(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> spherical_morton_scalar_shadow(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> spherical_morton_packet_shadow(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> planar_independent_scalar(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> planar_independent_packet(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> planar_independent_scalar_shadow(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> planar_independent_packet_shadow(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> spherical_independent_scalar(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> spherical_independent_packet(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> spherical_independent_scalar_shadow(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> spherical_independent_packet_shadow(const Scene*, uint32_t);

extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_planar_morton_scalar(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_planar_morton_packet(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_planar_morton_scalar_shadow(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_planar_morton_packet_shadow(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_spherical_morton_scalar(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_spherical_morton_packet(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_spherical_morton_scalar_shadow(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_spherical_morton_packet_shadow(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_planar_independent_scalar(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_planar_independent_packet(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_planar_independent_scalar_shadow(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_planar_independent_packet_shadow(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_spherical_independent_scalar(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_spherical_independent_packet(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_spherical_independent_scalar_shadow(const Scene*, uint32_t);
extern MTS_EXPORT_RENDER std::pair<Float, size_t> naive_spherical_independent_packet_shadow(const Scene*, uint32_t);

NAMESPACE_END(rtbench)
NAMESPACE_END(mitsuba)
