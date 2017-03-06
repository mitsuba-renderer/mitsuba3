#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/fwd.h>
#include <vector>
#include <iostream>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Scene : public Object {
public:
    Scene(const Properties &props);

    // =============================================================
    //! @{ \name Ray tracing
    // =============================================================
    template<bool IsShadowRay = false,
        typename Ray,
        typename Point = typename Ray::Point,
        typename Scalar = value_t<Point>,
        typename Mask = mask_t<Scalar>>
    std::pair<Mask, Scalar> ray_intersect_dummy(const Ray ray, Scalar mint, Scalar maxt) const {
        return m_kdtree->ray_intersect_dummy<IsShadowRay>(ray, mint, maxt);
    }

    template<bool IsShadowRay = false,
        typename Ray,
        typename Point = typename Ray::Point,
        typename Scalar = value_t<Point>,
        typename Mask = mask_t<Scalar>>
    std::pair<Mask, Scalar> ray_intersect_pbrt(const Ray ray, Scalar mint, Scalar maxt) const {
        return m_kdtree->ray_intersect_pbrt<IsShadowRay>(ray, mint, maxt);
    }

    template<bool IsShadowRay = false>
    std::pair<bool, Float> ray_intersect_havran(const Ray3f ray, Float mint, Float maxt) const {
        return m_kdtree->ray_intersect_havran<IsShadowRay>(ray, mint, maxt);
    }
    //! @}
    // =============================================================

    const ShapeKDTree *kdtree() const { return m_kdtree; }
    ShapeKDTree *kdtree() { return m_kdtree; }

    /// Return a human-readable string representation of the scene contents.
    virtual std::string to_string() const override;

    MTS_DECLARE_CLASS()
protected:
    virtual ~Scene();

    ref<ShapeKDTree> m_kdtree;
    std::vector<ref<Sensor>> m_sensors;
};



NAMESPACE_END(mitsuba)
