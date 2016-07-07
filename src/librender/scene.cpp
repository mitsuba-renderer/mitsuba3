#include <mitsuba/render/scene.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

Scene::Scene(const Properties &props) {
    m_kdtree = new ShapeKDTree();
    for (auto &kv : props.objects()) {
        Shape *shape = dynamic_cast<Shape *>(kv.second.get());

        if (shape) {
            m_kdtree->addShape(shape);
        } else {
            Throw("Tried to add an unsupported object of type %s", kv.second);
        }
    }
}

Scene::~Scene() { }

MTS_IMPLEMENT_CLASS(Scene, Object)
NAMESPACE_END(mitsuba)
