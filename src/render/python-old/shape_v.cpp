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

MI_PY_EXPORT(SilhouetteSample) {
    MI_PY_IMPORT_TYPES()

    m.def("has_flag", [](uint32_t flags, DiscontinuityFlags f) {return has_flag(flags, f);});
    m.def("has_flag", [](UInt32   flags, DiscontinuityFlags f) {return has_flag(flags, f);});

    auto ss = py::class_<SilhouetteSample3f, PositionSample3f>(m, "SilhouetteSample3f", D(SilhouetteSample))
        .def(py::init<>(), "Construct an uninitialized silhouette sample")
        .def(py::init<const SilhouetteSample3f &>(), "Copy constructor", "other"_a)
        // Members
        .def_readwrite("discontinuity_type", &SilhouetteSample3f::discontinuity_type, D(SilhouetteSample, discontinuity_type))
        .def_readwrite("d",                  &SilhouetteSample3f::d,                  D(SilhouetteSample, d))
        .def_readwrite("silhouette_d",       &SilhouetteSample3f::silhouette_d,       D(SilhouetteSample, silhouette_d))
        .def_readwrite("prim_index",         &SilhouetteSample3f::prim_index,         D(SilhouetteSample, prim_index))
        .def_readwrite("scene_index",        &SilhouetteSample3f::scene_index,        D(SilhouetteSample, scene_index))
        .def_readwrite("flags",              &SilhouetteSample3f::flags,              D(SilhouetteSample, flags))
        .def_readwrite("projection_index",   &SilhouetteSample3f::projection_index,   D(SilhouetteSample, projection_index))
        .def_readwrite("shape",              &SilhouetteSample3f::shape,              D(SilhouetteSample, shape))
        .def_readwrite("foreshortening",     &SilhouetteSample3f::foreshortening,     D(SilhouetteSample, foreshortening))
        .def_readwrite("offset",             &SilhouetteSample3f::offset,             D(SilhouetteSample, offset))
        // Methods
        .def("is_valid",  &SilhouetteSample3f::is_valid,  D(SilhouetteSample, is_valid))
        .def("spawn_ray", &SilhouetteSample3f::spawn_ray, D(SilhouetteSample, spawn_ray))
        .def_repr(SilhouetteSample3f);

    MI_PY_DRJIT_STRUCT(ss, SilhouetteSample3f, p, discontinuity_type, n, uv,
                       time, pdf, delta, d, silhouette_d, prim_index,
                       scene_index, flags, projection_index, shape,
                       foreshortening, offset)
}

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
       .def("shape_type",
            [](Ptr shape) { return shape->shape_type(); },
            D(Shape, shape_type))
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
       .def("silhouette_discontinuity_types",
            [](Ptr shape) {
                return shape->silhouette_discontinuity_types();
            },
            D(Shape, silhouette_discontinuity_types))
       .def("silhouette_sampling_weight",
            [](Ptr shape) {
                return shape->silhouette_sampling_weight();
            },
            D(Shape, silhouette_sampling_weight))
       .def("sample_silhouette",
            [](Ptr shape, const Point3f &sample, uint32_t flags, Mask active) {
                return shape->sample_silhouette(sample, flags, active);
            },
            "sample"_a, "flags"_a, "active"_a = true,
            D(Shape, sample_silhouette))
       .def("invert_silhouette_sample",
            [](Ptr shape, const SilhouetteSample3f &ss, Mask active) {
                return shape->invert_silhouette_sample(ss, active);
            },
            "ss"_a, "active"_a = true,
            D(Shape, invert_silhouette_sample))
       .def("differential_motion",
            [](Ptr shape, const SurfaceInteraction3f &si, Mask active) {
                return shape->differential_motion(si, active);
            },
            "si"_a, "active"_a = true,
            D(Shape, differential_motion))
       .def("primitive_silhouette_projection",
            [](Ptr shape, const Point3f &viewpoint,
               const SurfaceInteraction3f &si, uint32_t flags, Float sample,
               Mask active) {
                return shape->primitive_silhouette_projection(viewpoint, si, flags, sample, active);
            },
            "viewpoint"_a, "si"_a, "flags"_a, "sample"_a, "active"_a = true,
            D(Shape, primitive_silhouette_projection))
       .def("sample_precomputed_silhouette",
            [](Ptr shape, const Point3f &viewpoint, Shape::Index sample1,
               Float sample2, Mask active) {
                return shape->sample_precomputed_silhouette(viewpoint, sample1, sample2, active);
            },
            "viewpoint"_a, "sample1"_a, "sample2"_a, "active"_a = true,
            D(Shape, sample_precomputed_silhouette))
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
        .def_method(Shape, effective_primitive_count)
        .def_method(Shape, precompute_silhouette, "viewpoint"_a);

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
