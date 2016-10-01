#include <mitsuba/render/scene.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/core/properties.h>
#include "python.h"

MTS_PY_EXPORT(ShapeKDTree) {
    MTS_PY_CLASS(ShapeKDTree, Object)
        .def(py::init<const Properties &>(), DM(ShapeKDTree, ShapeKDTree))
        .mdef(ShapeKDTree, addShape)
        .mdef(ShapeKDTree, primitiveCount)
        .mdef(ShapeKDTree, shapeCount)
        .def("shape", (Shape *(ShapeKDTree::*)(size_t)) &ShapeKDTree::shape, DM(ShapeKDTree, shape))
        .def("__getitem__", [](ShapeKDTree &s, size_t i) -> py::object {
            if (i >= s.primitiveCount())
                throw py::index_error();
            Shape *shape = s.shape(i);
            if (shape->class_()->derivesFrom(MTS_CLASS(Mesh)))
                return py::cast(static_cast<Mesh *>(s.shape(i)));
            else
                return py::cast(s.shape(i));
        })
        .def("__len__", &ShapeKDTree::primitiveCount)
        .mdef(ShapeKDTree, build);
}

MTS_PY_EXPORT(Scene) {
    MTS_PY_CLASS(Scene, Object)
        .def("kdtree", (ShapeKDTree *(Scene::*)()) &Scene::kdtree, DM(Scene, kdtree));
}
