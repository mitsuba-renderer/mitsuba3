#include <mitsuba/render/mesh.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Shape) {
    MTS_PY_CLASS(Shape, Object)
        .def("bbox", (BoundingBox3f (Shape::*)() const) &Shape::bbox, D(Shape, bbox))
        .def("bbox", (BoundingBox3f (Shape::*)(Shape::Index) const) &Shape::bbox, D(Shape, bbox, 2))
        .mdef(Shape, primitive_count);

    MTS_PY_CLASS(Mesh, Shape)
        .def(py::init<const std::string &, Struct *, Mesh::Size, Struct *,
                      Mesh::Size>(), D(Mesh, Mesh))
        .mdef(Mesh, vertex_struct)
        .mdef(Mesh, face_struct)
        .mdef(Mesh, write)
        .mdef(Mesh, recompute_bbox)
        .def("vertices", [](py::object &o) {
            Mesh &m = py::cast<Mesh&>(o);
            py::dtype dtype = o.attr("vertex_struct")().attr("dtype")();
            return py::array(dtype, m.vertex_count(), m.vertices(), o);
        }, D(Mesh, vertices))
        .def("faces", [](py::object &o) {
            Mesh &m = py::cast<Mesh&>(o);
            py::dtype dtype = o.attr("face_struct")().attr("dtype")();
            return py::array(dtype, m.face_count(), m.faces(), o);
        }, D(Mesh, faces));
}
