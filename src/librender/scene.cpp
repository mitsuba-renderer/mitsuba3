#include <mitsuba/render/scene.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

Scene::Scene(const Properties &props) {
}

Scene::~Scene() { }

MTS_IMPLEMENT_CLASS(Scene, Object)
NAMESPACE_END(mitsuba)
