#include <mitsuba/core/stream.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Shape) {
    MTS_PY_CLASS(Shape, Object)
        .def("bbox", py::overload_cast<>(&Shape::bbox, py::const_), D(Shape, bbox))
        .def("bbox", py::overload_cast<typename Shape::Index>(&Shape::bbox, py::const_), D(Shape, bbox, 2))
        .def("bbox", py::overload_cast<typename Shape::Index, const BoundingBox3f &>(&Shape::bbox, py::const_), D(Shape, bbox, 2))
        .mdef(Shape, surface_area)
        // TODO: rest of the bindings
        .mdef(Shape, is_emitter)
        .def("emitter", py::overload_cast<>(&Shape::emitter))
        .def("emitter", py::overload_cast<>(&Shape::emitter, py::const_))
        .mdef(Shape, is_sensor)
        .def("sensor", py::overload_cast<>(&Shape::sensor))
        .def("sensor", py::overload_cast<>(&Shape::sensor, py::const_))
        .mdef(Shape, primitive_count)
        .mdef(Shape, effective_primitive_count)
        ;

    MTS_PY_CLASS(Mesh, Shape)
        .def(py::init<const std::string &, Struct *, Mesh::Size, Struct *,
                      Mesh::Size>(), D(Mesh, Mesh))
        .mdef(Mesh, vertex_struct)
        .mdef(Mesh, face_struct)
        .mdef(Mesh, write)
        .mdef(Mesh, has_vertex_normals)
        .mdef(Mesh, recompute_bbox)
        .mdef(Mesh, recompute_vertex_normals)
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
