#include <mitsuba/render/scene.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

Scene::Scene(const Properties &props) {
    m_kdtree = new ShapeKDTree(props);

    for (auto &kv : props.objects()) {
        Shape *shape = dynamic_cast<Shape *>(kv.second.get());
        Sensor *sensor = dynamic_cast<Sensor *>(kv.second.get());

        if (shape) {
            m_kdtree->add_shape(shape);
        } else if (sensor) {
            m_sensors.push_back(sensor);
        } else {
            Throw("Tried to add an unsupported object of type %s", kv.second);
        }
    }

    m_kdtree->build();
}

Scene::~Scene() { }

template MTS_EXPORT_CORE std::pair<bool, Float> Scene::ray_intersect_dummy<true>(const Ray3f, Float, Float) const;
template MTS_EXPORT_CORE std::pair<bool, Float> Scene::ray_intersect_dummy<false>(const Ray3f, Float, Float) const;
template MTS_EXPORT_CORE std::pair<mask_t<FloatP>, FloatP> Scene::ray_intersect_dummy<true>(const Ray3fP, FloatP, FloatP) const;
template MTS_EXPORT_CORE std::pair<mask_t<FloatP>, FloatP> Scene::ray_intersect_dummy<false>(const Ray3fP, FloatP, FloatP) const;

template MTS_EXPORT_CORE std::pair<bool, Float> Scene::ray_intersect_pbrt<true>(const Ray3f, Float, Float) const;
template MTS_EXPORT_CORE std::pair<bool, Float> Scene::ray_intersect_pbrt<false>(const Ray3f, Float, Float) const;
template MTS_EXPORT_CORE std::pair<mask_t<FloatP>, FloatP> Scene::ray_intersect_pbrt<true>(const Ray3fP, FloatP, FloatP) const;
template MTS_EXPORT_CORE std::pair<mask_t<FloatP>, FloatP> Scene::ray_intersect_pbrt<false>(const Ray3fP, FloatP, FloatP) const;

template MTS_EXPORT_CORE std::pair<bool, Float> Scene::ray_intersect_havran<true>(const Ray3f, Float, Float) const;
template MTS_EXPORT_CORE std::pair<bool, Float> Scene::ray_intersect_havran<false>(const Ray3f, Float, Float) const;

std::string Scene::to_string() const {
    return tfm::format("Scene[\n"
        "  kdtree = %s\n"
        "]",
        string::indent(m_kdtree->to_string())
    );
}


MTS_IMPLEMENT_CLASS(Scene, Object)
NAMESPACE_END(mitsuba)
