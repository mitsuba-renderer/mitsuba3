#include <mitsuba/render/scene.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

Scene::Scene(const Properties &props) {
    m_kdtree = new ShapeKDTree(props);

    for (auto &kv : props.objects()) {
        Shape *shape = dynamic_cast<Shape *>(kv.second.get());

        if (shape) {
            m_kdtree->addShape(shape);
        } else {
            Throw("Tried to add an unsupported object of type %s", kv.second);
        }
    }

    m_kdtree->build();
}

Scene::~Scene() { }

std::string Scene::toString() const {
    return tfm::format("Scene[\n"
        "  kdtree = %s\n"
        "]",
        string::indent(m_kdtree->toString())
    );
}


MTS_IMPLEMENT_CLASS(Scene, Object)
NAMESPACE_END(mitsuba)
