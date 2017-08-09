#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/render/fwd.h>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Scene : public Object {
public:
    Scene(const Properties &props);

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
