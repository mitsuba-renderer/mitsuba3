#include <mitsuba/core/stream.h>
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
MI_VARIANT class PyMesh : public Mesh<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Mesh)
    PyMesh(const Properties &props) : Mesh(props) { }
    PyMesh(const std::string &name, uint32_t vertex_count, uint32_t face_count,
           const Properties &props = Properties(),
           bool has_vertex_normals = false, bool has_vertex_texcoords = false)
        : Mesh(name, vertex_count, face_count, props, has_vertex_normals,
               has_vertex_texcoords) {}
    std::string to_string() const override {
        PYBIND11_OVERRIDE(std::string, Mesh, to_string);
    }
};

template <typename Ptr, typename Cls> void bind_shape_generic(Cls &cls) {
    MI_PY_IMPORT_TYPES()

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
       .def("has_attribute",
            [](Ptr shape, const std::string &name, const Mask &active) {
                return shape->has_attribute(name, active);
            },
            "name"_a, "active"_a = true, D(Shape, has_attribute))
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
            [](Ptr shape, const Ray3f &ray, uint32_t prim_index, const Mask &active) {
                return shape->ray_intersect_preliminary(ray, prim_index, active);
            },
            "ray"_a, "prim_index"_a = 0, "active"_a = true, D(Shape, ray_intersect_preliminary))
       .def("ray_intersect",
            [](Ptr shape, const Ray3f &ray, uint32_t flags, const Mask &active) {
                return shape->ray_intersect(ray, flags, active);
            },
            "ray"_a, "ray_flags"_a = +RayFlags::All, "active"_a = true,
            D(Shape, ray_intersect))
       .def("ray_test",
            [](Ptr shape, const Ray3f &ray, const Mask &active) {
                return shape->ray_test(ray, 0, active);
            },
            "ray"_a, "active"_a = true, D(Shape, ray_test))
       .def("sample_position",
            [](Ptr shape, Float time, const Point2f &sample, Mask active) {
                return shape->sample_position(time, sample, active);
            },
            "time"_a, "sample"_a, "active"_a = true, D(Shape, sample_position))
       .def("pdf_position",
            [](Ptr shape, const PositionSample3f &ps, Mask active) {
                return shape->pdf_position(ps, active);
            },
            "ps"_a, "active"_a = true, D(Shape, pdf_position))
       .def("sample_direction",
            [](Ptr shape, const Interaction3f &it, const Point2f &sample,
               Mask active) {
                return shape->sample_direction(it, sample, active);
            },
            "it"_a, "sample"_a, "active"_a = true, D(Shape, sample_direction))
       .def("pdf_direction",
            [](Ptr shape, const Interaction3f &it, const DirectionSample3f &ds,
               Mask active) {
                return shape->pdf_direction(it, ds, active);
            },
            "it"_a, "ps"_a, "active"_a = true, D(Shape, pdf_direction))
       .def("eval_parameterization",
            [](Ptr shape, const Point2f &uv, uint32_t ray_flags, Mask active) {
                return shape->eval_parameterization(uv, ray_flags, active);
            },
            "uv"_a, "ray_flags"_a = +RayFlags::All, "active"_a = true,
            D(Shape, eval_parameterization))
       .def("surface_area",
            [](Ptr shape) {
                return shape->surface_area();
            },
            D(Shape, surface_area));

    if constexpr (dr::is_array_v<Ptr>)
        bind_drjit_ptr_array(cls);
}

MI_PY_EXPORT(Shape) {
    MI_PY_IMPORT_TYPES(Shape, Mesh)

    auto shape = MI_PY_CLASS(Shape, Object)
        .def("bbox", py::overload_cast<>(
            &Shape::bbox, py::const_), D(Shape, bbox))
        .def("bbox", py::overload_cast<ScalarUInt32>(
            &Shape::bbox, py::const_), D(Shape, bbox, 2), "index"_a)
        .def("bbox", py::overload_cast<ScalarUInt32, const ScalarBoundingBox3f &>(
            &Shape::bbox, py::const_), D(Shape, bbox, 3), "index"_a, "clip"_a)
        .def_method(Shape, id)
        .def_method(Shape, is_mesh)
        .def_method(Shape, parameters_grad_enabled)
        .def_method(Shape, primitive_count)
        .def_method(Shape, effective_primitive_count);

    bind_shape_generic<Shape *>(shape);

    if constexpr (dr::is_array_v<ShapePtr>) {
        py::object dr       = py::module_::import("drjit"),
                   dr_array = dr.attr("ArrayBase");

        py::class_<ShapePtr> cls(m, "ShapePtr", dr_array);
        bind_shape_generic<ShapePtr>(cls);
    }

    using PyMesh = PyMesh<Float, Spectrum>;
    using ScalarSize = typename Mesh::ScalarSize;
    MI_PY_TRAMPOLINE_CLASS(PyMesh, Mesh, Shape)
        .def(py::init<const Properties&>(), "props"_a)
        .def(py::init<const std::string &, ScalarSize, ScalarSize,
                      const Properties &, bool, bool>(),
             "name"_a, "vertex_count"_a, "face_count"_a,
             py::arg_v("props", Properties(), "Properties()"),
             "has_vertex_normals"_a = false, "has_vertex_texcoords"_a = false,
             D(Mesh, Mesh))
        .def_method(Mesh, initialize)
        .def_method(Mesh, vertex_count)
        .def_method(Mesh, face_count)
        .def_method(Mesh, has_vertex_normals)
        .def_method(Mesh, has_vertex_texcoords)
        .def("write_ply",
             py::overload_cast<const std::string &>(&Mesh::write_ply, py::const_),
             "filename"_a, D(Mesh, write_ply))
        .def("write_ply",
             py::overload_cast<Stream *>(&Mesh::write_ply, py::const_),
             "stream"_a, D(Mesh, write_ply, 2))
        .def("add_attribute", &Mesh::add_attribute, "name"_a, "size"_a, "buffer"_a,
             D(Mesh, add_attribute), py::return_value_policy::reference_internal)
        .def("vertex_position", [](const Mesh &m, UInt32 index, Mask active) {
                return m.vertex_position(index, active);
             }, D(Mesh, vertex_position), "index"_a, "active"_a = true)
        .def("vertex_normal", [](const Mesh &m, UInt32 index, Mask active) {
                return m.vertex_normal(index, active);
             }, D(Mesh, vertex_normal), "index"_a, "active"_a = true)
        .def("vertex_texcoord", [](const Mesh &m, UInt32 index, Mask active) {
                return m.vertex_texcoord(index, active);
             }, D(Mesh, vertex_texcoord), "index"_a, "active"_a = true)
        .def("face_indices", [](const Mesh &m, UInt32 index, Mask active) {
                return m.face_indices(index, active);
             }, D(Mesh, face_indices), "index"_a, "active"_a = true)
        .def("ray_intersect_triangle", &Mesh::ray_intersect_triangle,
             "index"_a, "ray"_a, "active"_a = true,
             D(Mesh, ray_intersect_triangle));

    MI_PY_REGISTER_OBJECT("register_mesh", Mesh)
}
