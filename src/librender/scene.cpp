#include <mitsuba/render/scene.h>
#include <mitsuba/core/properties.h>
#include <iostream>

NAMESPACE_BEGIN(mitsuba)

Scene::Scene(const Properties &props) {
    for (auto &kv : props.objects()) {
        std::cout << "Got " << kv.first << std::endl;
    }
}

Scene::~Scene() { }

MTS_IMPLEMENT_CLASS(Scene, Object)
NAMESPACE_END(mitsuba)
