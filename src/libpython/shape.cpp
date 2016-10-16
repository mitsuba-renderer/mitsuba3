#include <mitsuba/render/mesh.h>
#include <mitsuba/core/stream.h>
#include "python.h"

MTS_PY_EXPORT(Shape) {
    MTS_PY_CLASS(Shape, Object)
        .def("bbox", (BoundingBox3f (Shape::*)() const) &Shape::bbox, DM(Shape, bbox))
        .def("bbox", (BoundingBox3f (Shape::*)(Shape::Index) const) &Shape::bbox, DM(Shape, bbox, 2))
        .mdef(Shape, primitiveCount);

    MTS_PY_CLASS(Mesh, Shape)
        .mdef(Mesh, vertexStruct)
        .mdef(Mesh, faceStruct)
        .mdef(Mesh, write)
        .def("vertices", [](py::object &o) {
            Mesh &m = py::cast<Mesh&>(o);
            py::dtype dtype = o.attr("vertexStruct")().attr("dtype")();
            return py::array(dtype, m.vertexCount(), m.vertices());
        })
        .def("faces", [](py::object &o) {
            Mesh &m = py::cast<Mesh&>(o);
            py::dtype dtype = o.attr("faceStruct")().attr("dtype")();
            return py::array(dtype, m.faceCount(), m.faces());
        });
}
