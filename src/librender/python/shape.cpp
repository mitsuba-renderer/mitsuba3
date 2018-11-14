#include <mitsuba/core/stream.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/kdtree.h>

MTS_PY_EXPORT(Shape) {
    using Index = Shape::Index;
    MTS_PY_CLASS(Shape, DifferentiableObject)
        .def("ray_intersect",
             py::overload_cast<const Ray3f &, bool>(
                 &Shape::ray_intersect, py::const_),
             D(Shape, ray_intersect), "ray"_a, "active"_a = true)
        .def("ray_intersect",
             enoki::vectorize_wrapper(py::overload_cast<const Ray3fP &, MaskP>(
                 &Shape::ray_intersect, py::const_)),
             D(Shape, ray_intersect), "ray"_a, "active"_a = true)
        .def("sample_position",
             py::overload_cast<Float, const Point2f &, bool>(
                 &Shape::sample_position, py::const_),
             D(Shape, sample_position), "time"_a, "sample"_a, "active"_a = true)
        .def("sample_position",
             enoki::vectorize_wrapper(py::overload_cast<FloatP, const Point2fP &, MaskP>(
                 &Shape::sample_position, py::const_)),
             "time"_a, "sample"_a, "active"_a = true)
        .def("pdf_position",
             py::overload_cast<const PositionSample3f &, bool>(
                 &Shape::pdf_position, py::const_),
             D(Shape, pdf_position), "ps"_a, "active"_a = true)
        .def("pdf_position",
             enoki::vectorize_wrapper(py::overload_cast<const PositionSample3fP &, MaskP>(
                 &Shape::pdf_position, py::const_)),
             "ps"_a, "active"_a = true)
        .def("sample_direction",
             py::overload_cast<const Interaction3f &, const Point2f &, bool>(
                 &Shape::sample_direction, py::const_),
             D(Shape, sample_direction), "it"_a, "sample"_a, "active"_a = true)
        .def("sample_direction",
             enoki::vectorize_wrapper(py::overload_cast<const Interaction3fP &, const Point2fP &, MaskP>(
                 &Shape::sample_direction, py::const_)),
             "it"_a, "sample"_a, "active"_a = true)
        .def("pdf_direction",
             py::overload_cast<const Interaction3f &, const DirectionSample3f &, bool>(
                 &Shape::pdf_direction, py::const_),
             D(Shape, pdf_direction), "it"_a, "ps"_a, "active"_a = true)
        .def("pdf_direction",
             enoki::vectorize_wrapper(py::overload_cast<const Interaction3fP &, const DirectionSample3fP &, MaskP>(
                 &Shape::pdf_direction, py::const_)),
             "it"_a, "ps"_a, "active"_a = true)
        .def("bbox", py::overload_cast<>(
            &Shape::bbox, py::const_), D(Shape, bbox))
        .def("bbox", py::overload_cast<Index>(
            &Shape::bbox, py::const_), D(Shape, bbox, 2), "index"_a)
        .def("bbox", py::overload_cast<Index, const BoundingBox3f &>(
            &Shape::bbox, py::const_), D(Shape, bbox, 3), "index"_a, "clip"_a)
        .def("normal_derivative",
             py::overload_cast<const SurfaceInteraction3f &, bool, bool>(
                 &Shape::normal_derivative, py::const_),
             D(Shape, normal_derivative), "si"_a, "shading_frame"_a = true, "active"_a = true)
        .def("normal_derivative",
             enoki::vectorize_wrapper(py::overload_cast<const SurfaceInteraction3fP &, bool, MaskP>(
                 &Shape::normal_derivative, py::const_)),
             "si"_a, "shading_frame"_a = true, "active"_a = true)
        .mdef(Shape, surface_area)
        .mdef(Shape, is_emitter)
        .mdef(Shape, is_sensor)
        .def("emitter", py::overload_cast<bool>(&Shape::emitter, py::const_), "active"_a = true)
        .def("sensor", py::overload_cast<>(&Shape::sensor, py::const_))
        .mdef(Shape, primitive_count)
        .mdef(Shape, effective_primitive_count)
        ;

    MTS_PY_CLASS(Mesh, Shape)
        .def(py::init<const std::string &, Struct *, Mesh::Size, Struct *,
                      Mesh::Size>(), D(Mesh, Mesh))
        .mdef(Mesh, vertex_struct)
        .mdef(Mesh, face_struct)
        .mdef(Mesh, has_vertex_normals)
        .mdef(Mesh, has_vertex_texcoords)
        .mdef(Mesh, has_vertex_colors)
        .mdef(Mesh, write)
        .mdef(Mesh, recompute_vertex_normals)
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


#if defined(MTS_ENABLE_AUTODIFF)
    using ShapeD = enoki::DiffArray<enoki::CUDAArray<const Shape *>>;

    bind_array<ShapeD>(m, "ShapeD")
        .def("bsdf", [](const ShapeD &a) { return a->bsdf(); });
#endif
}
