#include <mitsuba/core/struct.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/python/python.h>
#include <pybind11/numpy.h>


/// Trampoline for derived types implemented in Python
MTS_VARIANT class PyMesh : public Mesh<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Mesh)
    PyMesh(const Properties &props) : Mesh(props) { }
    PyMesh(const std::string &name, uint32_t vertex_count, uint32_t face_count,
           const Properties &props = Properties(),
           bool has_vertex_normals = false, bool has_vertex_texcoords = false)
        : Mesh(name, vertex_count, face_count, props, has_vertex_normals,
               has_vertex_texcoords) {}
    std::string to_string() const override {
        PYBIND11_OVERRIDE(std::string, Mesh, to_string,);
    }
};

template <typename Ptr, typename Cls> void bind_shape_generic(Cls &cls) {
    MTS_PY_IMPORT_TYPES()

    cls.def("is_emitter", [](Ptr shape) { return shape->is_emitter(); },
            D(Shape, is_emitter))
       .def("is_sensor", [](Ptr shape) { return shape->is_sensor(); },
            D(Shape, is_sensor))
       .def("is_medium_transition",
            [](Ptr shape) { return shape->is_medium_transition(); },
            D(Shape, is_medium_transition))
       .def("interior_medium",
            [](Ptr shape) { return shape->interior_medium(); },
            D(Shape, interior_medium))
       .def("exterior_medium",
            [](Ptr shape) { return shape->exterior_medium(); },
            D(Shape, exterior_medium))
       .def("bsdf", [](Ptr shape) { return shape->bsdf(); },
            D(Shape, bsdf))
       .def("sensor", [](Ptr shape) { return shape->sensor(); },
            D(Shape, sensor))
       .def("emitter", [](Ptr shape) { return shape->emitter(); },
            D(Shape, emitter))
       .def("compute_surface_interaction",
            [](Ptr shape, const Ray3f &ray,
               const PreliminaryIntersection3f &pi, uint32_t ray_flags,
               Mask active) {
                return shape->compute_surface_interaction(ray, pi, ray_flags, 0, active);
            },
            "ray"_a, "pi"_a, "ray_flags"_a = +RayFlags::All,
            "active"_a = true, D(Shape, compute_surface_interaction))
       .def("eval_attribute",
            [](Ptr shape, const std::string &name,
               const SurfaceInteraction3f &si, const Mask &active) {
                return shape->eval_attribute(name, si, active);
            },
            "name"_a, "si"_a, "active"_a = true, D(Shape, eval_attribute))
       .def("eval_attribute_1",
            [](Ptr shape, const std::string &name,
               const SurfaceInteraction3f &si, const Mask &active) {
                return shape->eval_attribute_1(name, si, active);
            },
            "name"_a, "si"_a, "active"_a = true, D(Shape, eval_attribute_1))
       .def("eval_attribute_3",
            [](Ptr shape, const std::string &name,
               const SurfaceInteraction3f &si, const Mask &active) {
                return shape->eval_attribute_3(name, si, active);
            },
            "name"_a, "si"_a, "active"_a = true, D(Shape, eval_attribute_3))
       .def("ray_intersect_preliminary",
            [](Ptr shape, const Ray3f &ray, const Mask &active) {
                return shape->ray_intersect_preliminary(ray, active);
            },
            "ray"_a, "active"_a = true, D(Shape, ray_intersect_preliminary))
       .def("ray_intersect_preliminary",
            [](Ptr shape, const Ray3f &ray, const Mask &active) {
                return shape->ray_intersect_preliminary(ray, active);
            },
            "ray"_a, "active"_a = true, D(Shape, ray_intersect_preliminary))
       .def("ray_intersect",
            [](Ptr shape, const Ray3f &ray, uint32_t flags, const Mask &active) {
                return shape->ray_intersect(ray, flags, active);
            },
            "ray"_a, "ray_flags"_a = +RayFlags::All, "active"_a = true,
            D(Shape, ray_intersect))
       .def("ray_test",
            [](Ptr shape, const Ray3f &ray, const Mask &active) {
                return shape->ray_test(ray, active);
            },
            "ray"_a, "active"_a = true, D(Shape, ray_test));

    if constexpr (ek::is_array_v<Ptr>)
        bind_enoki_ptr_array(cls);
}

MTS_PY_EXPORT(Shape) {
    MTS_PY_IMPORT_TYPES(Shape, Mesh)

    auto shape = MTS_PY_CLASS(Shape, Object)
        .def("sample_position", &Shape::sample_position,
            "time"_a, "sample"_a, "active"_a = true, D(Shape, sample_position))
        .def("pdf_position", &Shape::pdf_position,
            "ps"_a, "active"_a = true, D(Shape, pdf_position))
        .def("sample_direction", &Shape::sample_direction,
            "it"_a, "sample"_a, "active"_a = true, D(Shape, sample_direction))
        .def("pdf_direction", &Shape::pdf_direction,
            "it"_a, "ps"_a, "active"_a = true, D(Shape, pdf_direction))
        .def("bbox", py::overload_cast<>(
            &Shape::bbox, py::const_), D(Shape, bbox))
        .def("bbox", py::overload_cast<ScalarUInt32>(
            &Shape::bbox, py::const_), D(Shape, bbox, 2), "index"_a)
        .def("bbox", py::overload_cast<ScalarUInt32, const ScalarBoundingBox3f &>(
            &Shape::bbox, py::const_), D(Shape, bbox, 3), "index"_a, "clip"_a)
        .def_method(Shape, surface_area)
        .def_method(Shape, id)
        .def_method(Shape, is_mesh)
        .def_method(Shape, parameters_grad_enabled)
        .def_method(Shape, primitive_count)
        .def_method(Shape, effective_primitive_count);

    bind_shape_generic<Shape *>(shape);

    if constexpr (ek::is_array_v<ShapePtr>) {
        py::object ek       = py::module_::import("enoki"),
                   ek_array = ek.attr("ArrayBase");

        py::class_<ShapePtr> cls(m, "ShapePtr", ek_array);
        bind_shape_generic<ShapePtr>(cls);
    }

    using PyMesh = PyMesh<Float, Spectrum>;
    using ScalarSize = typename Mesh::ScalarSize;
    auto mesh = py::class_<Mesh, PyMesh, Shape, ref<Mesh>>(m, "Mesh", D(Mesh))
        .def(py::init<const Properties&>(), "props"_a)
        .def(py::init<const std::string &, ScalarSize, ScalarSize,
                      const Properties &, bool, bool>(),
             "name"_a, "vertex_count"_a, "face_count"_a,
             "props"_a = Properties(), "has_vertex_normals"_a = false,
             "has_vertex_texcoords"_a = false, D(Mesh, Mesh))
        .def_method(Mesh, initialize)
        .def_method(Mesh, vertex_count)
        .def_method(Mesh, face_count)
        .def_method(Mesh, has_vertex_normals)
        .def_method(Mesh, has_vertex_texcoords)
        .def_method(Mesh, recompute_vertex_normals)
        .def_method(Mesh, recompute_bbox)
        .def("write_ply", &Mesh::write_ply, "filename"_a,
             "Export mesh as a binary PLY file")
        .def("vertex_positions_buffer",
             py::overload_cast<>(&Mesh::vertex_positions_buffer),
             D(Mesh, vertex_positions_buffer),
             py::return_value_policy::reference_internal)
        .def("vertex_normals_buffer",
             py::overload_cast<>(&Mesh::vertex_normals_buffer),
             D(Mesh, vertex_normals_buffer),
             py::return_value_policy::reference_internal)
        .def("vertex_texcoords_buffer",
             py::overload_cast<>(&Mesh::vertex_texcoords_buffer),
             D(Mesh, vertex_texcoords_buffer),
             py::return_value_policy::reference_internal)
        .def("faces_buffer", py::overload_cast<>(&Mesh::faces_buffer),
             D(Mesh, faces_buffer), py::return_value_policy::reference_internal)
        .def("attribute_buffer", &Mesh::attribute_buffer, "name"_a,
             D(Mesh, attribute_buffer), py::return_value_policy::reference_internal)
        .def("add_attribute", &Mesh::add_attribute, "name"_a, "size"_a, "buffer"_a,
             D(Mesh, add_attribute), py::return_value_policy::reference_internal)
        .def("ray_intersect_triangle", &Mesh::ray_intersect_triangle,
             "index"_a, "ray"_a, "active"_a = true,
             D(Mesh, ray_intersect_triangle))
        .def("eval_parameterization", &Mesh::eval_parameterization,
             "uv"_a, "ray_flags"_a = +RayFlags::All, "active"_a = true,
             D(Mesh, eval_parameterization));

    MTS_PY_REGISTER_OBJECT("register_mesh", Mesh)
}
