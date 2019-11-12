#include <mitsuba/python/python.h>
#include <mitsuba/core/struct.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/shape.h>

MTS_PY_EXPORT_VARIANTS(Shape) {
    MTS_IMPORT_TYPES()
    MTS_IMPORT_OBJECT_TYPES()
    using ShapeP = mitsuba::Shape<FloatP, SpectrumP>;
    using Ray3fP = typename ShapeP::Ray3f;
    using MaskP = typename ShapeP::Mask;

    MTS_PY_CHECK_ALIAS(Shape, m) {
        MTS_PY_CLASS(Shape, Object)
            .def("sample_position", vectorize<Float>(&ShapeP::sample_position),
                "time"_a, "sample"_a, "active"_a = true, D(Shape, sample_position))
            .def("pdf_position", vectorize<Float>(&ShapeP::pdf_position),
                "ps"_a, "active"_a = true, D(Shape, pdf_position))
            .def("sample_direction", vectorize<Float>(&ShapeP::sample_direction),
                "it"_a, "sample"_a, "active"_a = true, D(Shape, sample_direction))
            .def("pdf_direction", vectorize<Float>(&ShapeP::pdf_direction),
                "it"_a, "ps"_a, "active"_a = true, D(Shape, pdf_direction))
            .def("normal_derivative", vectorize<Float>(&ShapeP::normal_derivative),
                "si"_a, "shading_frame"_a = true, "active"_a = true,
                D(Shape, normal_derivative))
            .def("ray_intersect",
                vectorize<Float>(
                    py::overload_cast<const Ray3fP &, MaskP>(&ShapeP::ray_intersect, py::const_)),
                "ray"_a, "active"_a = true, D(Shape, ray_intersect))
            .def_method(Shape, ray_test, "ray"_a, "active"_a = true)
            .def_method(Shape, fill_surface_interaction, "ray"_a, "cache"_a, "si"_a, "active"_a = true)
            .def("bbox", py::overload_cast<>(
                &Shape::bbox, py::const_), D(Shape, bbox))
            .def("bbox", py::overload_cast<UInt32>(
                &Shape::bbox, py::const_), D(Shape, bbox, 2), "index"_a)
            .def("bbox", py::overload_cast<UInt32, const BoundingBox3f &>(
                &Shape::bbox, py::const_), D(Shape, bbox, 3), "index"_a, "clip"_a)
            .def_method(Shape, surface_area)
            .def_method(Shape, id)
            .def_method(Shape, is_mesh)
            .def_method(Shape, is_medium_transition)
            .def_method(Shape, interior_medium)
            .def_method(Shape, exterior_medium)
            .def_method(Shape, is_emitter)
            .def_method(Shape, is_sensor)
            .def("emitter", py::overload_cast<bool>(&Shape::emitter, py::const_), "active"_a = true)
            .def("sensor", py::overload_cast<>(&Shape::sensor, py::const_))
            .def_method(Shape, primitive_count)
            .def_method(Shape, effective_primitive_count);
    }

    using Mesh = Mesh<Float, Spectrum>;
    using Size = typename Mesh::Size;

    MTS_PY_CHECK_ALIAS(Mesh, m) {
        MTS_PY_CLASS(Mesh, Shape)
            .def(py::init<const std::string &, Struct *, Size, Struct *, Size>(),
                D(Mesh, Mesh))
            .def_method(Mesh, vertex_struct)
            .def_method(Mesh, face_struct)
            .def_method(Mesh, has_vertex_normals)
            .def_method(Mesh, has_vertex_texcoords)
            .def_method(Mesh, has_vertex_colors)
            .def_method(Mesh, write)
            .def_method(Mesh, recompute_vertex_normals)
            .def_method(Mesh, recompute_bbox)
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
}
