#include <mitsuba/core/struct.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/python/python.h>
#include <pybind11/numpy.h>

MTS_PY_EXPORT(Shape) {
    MTS_PY_IMPORT_TYPES(Shape, Mesh)

    MTS_PY_CLASS(Shape, Object)
        .def("sample_position", vectorize(&Shape::sample_position),
            "time"_a, "sample"_a, "active"_a = true, D(Shape, sample_position))
        .def("pdf_position", vectorize(&Shape::pdf_position),
            "ps"_a, "active"_a = true, D(Shape, pdf_position))
        .def("sample_direction", vectorize(&Shape::sample_direction),
            "it"_a, "sample"_a, "active"_a = true, D(Shape, sample_direction))
        .def("pdf_direction", vectorize(&Shape::pdf_direction),
            "it"_a, "ps"_a, "active"_a = true, D(Shape, pdf_direction))
        .def("normal_derivative", vectorize(&Shape::normal_derivative),
            "si"_a, "shading_frame"_a = true, "active"_a = true,
            D(Shape, normal_derivative))
        .def("ray_intersect",
            vectorize(
                py::overload_cast<const Ray3f &, Mask>(&Shape::ray_intersect, py::const_)),
            "ray"_a, "active"_a = true, D(Shape, ray_intersect))
        .def("ray_test", vectorize(&Shape::ray_test), "ray"_a, "active"_a = true)
        .def("fill_surface_interaction", &Shape::fill_surface_interaction,
                "ray"_a, "cache"_a, "si"_a, "active"_a = true) // TODO vectorize this
        .def("bbox", py::overload_cast<>(
            &Shape::bbox, py::const_), D(Shape, bbox))
        .def("bbox", py::overload_cast<ScalarUInt32>(
            &Shape::bbox, py::const_), D(Shape, bbox, 2), "index"_a)
        .def("bbox", py::overload_cast<ScalarUInt32, const ScalarBoundingBox3f &>(
            &Shape::bbox, py::const_), D(Shape, bbox, 3), "index"_a, "clip"_a)
        .def_method(Shape, surface_area)
        .def_method(Shape, id)
        .def_method(Shape, is_mesh)
        .def_method(Shape, bsdf)
        .def_method(Shape, is_medium_transition)
        .def_method(Shape, interior_medium)
        .def_method(Shape, exterior_medium)
        .def_method(Shape, is_emitter)
        .def_method(Shape, is_sensor)
        .def("emitter", vectorize(py::overload_cast<Mask>(&Shape::emitter, py::const_)),
                "active"_a = true)
        .def("sensor", py::overload_cast<>(&Shape::sensor, py::const_))
        .def_method(Shape, primitive_count)
        .def_method(Shape, effective_primitive_count);

    using ScalarSize = typename Mesh::ScalarSize;
    MTS_PY_CLASS(Mesh, Shape)
        .def(py::init<const std::string &, Struct *, ScalarSize, Struct *, ScalarSize>(),
            D(Mesh, Mesh))
        .def(py::init<const std::string &, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, short, const ScalarMatrix4f &>(), "Constructor to call from Blender")
        .def_method(Mesh, vertex_struct)
        .def_method(Mesh, face_struct)
        .def_method(Mesh, vertex_count)
        .def_method(Mesh, face_count)
        .def_method(Mesh, has_vertex_normals)
        .def_method(Mesh, has_vertex_texcoords)
        .def_method(Mesh, has_vertex_colors)
        .def_method(Mesh, recompute_vertex_normals)
        .def_method(Mesh, recompute_bbox)
        .def("write_ply", &Mesh::write_ply, "stream"_a, "Export mesh as a binary PLY file")
        .def("vertices", [](py::object &o) {
            Mesh &m = py::cast<Mesh&>(o);
            py::dtype dtype = o.attr("vertex_struct")().attr("dtype")();
            return py::array(dtype, m.vertex_count(), m.vertices(), o);
        }, D(Mesh, vertices))
        .def("faces", [](py::object &o) {
            Mesh &m = py::cast<Mesh&>(o);
            py::dtype dtype = o.attr("face_struct")().attr("dtype")();
            return py::array(dtype, m.face_count(), m.faces(), o);
        }, D(Mesh, faces))
        .def("ray_intersect_triangle", vectorize(&Mesh::ray_intersect_triangle),
            "index"_a, "ray"_a, "active"_a = true, D(Mesh, ray_intersect_triangle));
}
