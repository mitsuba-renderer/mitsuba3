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
#include <nanobind/trampoline.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/optional.h>
#include <drjit/python.h>
#include <list>

MI_PY_EXPORT(SilhouetteSample) {
    MI_PY_IMPORT_TYPES()

    m.def("has_flag", [](uint32_t flags, DiscontinuityFlags f) {return has_flag(flags, f);});
    m.def("has_flag", [](UInt32   flags, DiscontinuityFlags f) {return has_flag(flags, f);});

    auto ss = nb::class_<SilhouetteSample3f, PositionSample3f>(m, "SilhouetteSample3f", D(SilhouetteSample))
        .def(nb::init<>(), "Construct an uninitialized silhouette sample")
        .def(nb::init<const SilhouetteSample3f &>(), "Copy constructor", "other"_a)
        // Members
        .def_rw("discontinuity_type", &SilhouetteSample3f::discontinuity_type, D(SilhouetteSample, discontinuity_type))
        .def_rw("d",                  &SilhouetteSample3f::d,                  D(SilhouetteSample, d))
        .def_rw("silhouette_d",       &SilhouetteSample3f::silhouette_d,       D(SilhouetteSample, silhouette_d))
        .def_rw("prim_index",         &SilhouetteSample3f::prim_index,         D(SilhouetteSample, prim_index))
        .def_rw("scene_index",        &SilhouetteSample3f::scene_index,        D(SilhouetteSample, scene_index))
        .def_rw("flags",              &SilhouetteSample3f::flags,              D(SilhouetteSample, flags))
        .def_rw("projection_index",   &SilhouetteSample3f::projection_index,   D(SilhouetteSample, projection_index))
        .def_rw("shape",              &SilhouetteSample3f::shape,              D(SilhouetteSample, shape))
        .def_rw("foreshortening",     &SilhouetteSample3f::foreshortening,     D(SilhouetteSample, foreshortening))
        .def_rw("offset",             &SilhouetteSample3f::offset,             D(SilhouetteSample, offset))
        // Methods
        .def("is_valid",  &SilhouetteSample3f::is_valid,  D(SilhouetteSample, is_valid))
        .def("spawn_ray",
             [](const SilhouetteSample3f& ss, const std::optional<Wavelength> wavelengths_) {
                Wavelength wavelengths = wavelengths_.has_value() ?
                                         wavelengths_.value() :
                                         dr::zeros<Wavelength>();
                return ss.spawn_ray(wavelengths);
             },
             "wavelengths"_a = nb::none(), D(SilhouetteSample, spawn_ray))
        .def_repr(SilhouetteSample3f);

    MI_PY_DRJIT_STRUCT(ss, SilhouetteSample3f, p, discontinuity_type, n, uv,
                       time, pdf, delta, d, silhouette_d, prim_index,
                       scene_index, flags, projection_index, shape,
                       foreshortening, offset)
}

using Caster = nb::object(*)(mitsuba::Object *);
extern Caster cast_object;

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyMesh : public Mesh<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Mesh)
    NB_TRAMPOLINE(Mesh, 1);

    PyMesh(const Properties &props) : Mesh(props) { }
    PyMesh(const std::string &name, uint32_t vertex_count, uint32_t face_count,
           const Properties &props = Properties(),
           bool has_vertex_normals = false, bool has_vertex_texcoords = false)
        : Mesh(name, vertex_count, face_count, props, has_vertex_normals,
               has_vertex_texcoords) {}
    std::string to_string() const override {
        NB_OVERRIDE(to_string);
    }

    DR_TRAMPOLINE_TRAVERSE_CB(Mesh)
};

template <typename Ptr, typename Cls> void bind_shape_generic(Cls &cls) {
    MI_PY_IMPORT_TYPES()

    using RetMedium = std::conditional_t<drjit::is_array_v<Ptr>, MediumPtr, drjit::scalar_t<MediumPtr>>;
    using RetBSDF = std::conditional_t<drjit::is_array_v<Ptr>, BSDFPtr, drjit::scalar_t<BSDFPtr>>;
    using RetSensor = std::conditional_t<drjit::is_array_v<Ptr>, SensorPtr, drjit::scalar_t<SensorPtr>>;
    using RetEmitter = std::conditional_t<drjit::is_array_v<Ptr>, EmitterPtr, drjit::scalar_t<EmitterPtr>>;

    cls.def("is_emitter", [](Ptr shape) { return shape->is_emitter(); },
            D(Shape, is_emitter))
       .def("is_sensor", [](Ptr shape) { return shape->is_sensor(); },
            D(Shape, is_sensor))
       .def("is_mesh", [](Ptr shape) { return shape->is_mesh(); },
            D(Shape, is_mesh))
       .def("is_ellipsoids", [](Ptr shape) { return shape->is_ellipsoids(); },
            D(Shape, is_ellipsoids))
       .def("is_medium_transition",
            [](Ptr shape) { return shape->is_medium_transition(); },
            D(Shape, is_medium_transition))
       .def("shape_type",
            [](Ptr shape) { return shape->shape_type(); },
            D(Shape, shape_type))
       .def("interior_medium",
            [](Ptr shape) -> RetMedium { return shape->interior_medium(); },
            D(Shape, interior_medium))
       .def("exterior_medium",
            [](Ptr shape) -> RetMedium { return shape->exterior_medium(); },
            D(Shape, exterior_medium))
       .def("bsdf", [](Ptr shape) -> RetBSDF { return shape->bsdf(); },
            D(Shape, bsdf))
       .def("sensor", [](Ptr shape) -> RetSensor { return shape->sensor(); },
            D(Shape, sensor))
       .def("emitter", [](Ptr shape) -> RetEmitter { return shape->emitter(); },
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
       .def("eval_attribute_x",
            [](Ptr shape, const std::string &name,
               const SurfaceInteraction3f &si, const Mask &active) {
                return shape->eval_attribute_x(name, si, active);
            },
            "name"_a, "si"_a, "active"_a = true, D(Shape, eval_attribute_x))
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
            D(Shape, surface_area))
       .def("has_flipped_normals",
            [](Ptr shape) {
                return shape->has_flipped_normals();
            },
            D(Shape, has_flipped_normals));
}

template <typename Ptr, typename Cls> void bind_mesh_generic(Cls &cls) {
    MI_PY_IMPORT_TYPES()
    cls
       .def("vertex_count", [](const Ptr ptr) {
            return ptr->vertex_count();
        }, D(Mesh, vertex_count))
       .def("face_count", [](const Ptr ptr) {
            return ptr->face_count();
        }, D(Mesh, face_count))
       .def("has_vertex_normals", [](const Ptr ptr) {
            return ptr->has_vertex_normals();
        }, D(Mesh, has_vertex_normals))
       .def("has_vertex_texcoords", [](const Ptr ptr) {
            return ptr->has_vertex_texcoords();
        }, D(Mesh, has_vertex_texcoords))
       .def("has_mesh_attributes", [](const Ptr ptr) {
            return ptr->has_mesh_attributes();
        }, D(Mesh, has_mesh_attributes))
       .def("has_face_normals", [](const Ptr ptr) {
            return ptr->has_face_normals();
        }, D(Mesh, has_face_normals))
       .def("face_indices", [](const Ptr m, UInt32 index, Mask active) {
                return m->face_indices(index, active);
            }, D(Mesh, face_indices), "index"_a, "active"_a = true)
       .def("edge_indices", [](const Ptr m, UInt32 tri_index, UInt32 edge_index, Mask active) {
                return m->edge_indices(tri_index, edge_index, active);
            }, D(Mesh, edge_indices), "tri_index"_a, "edge_index"_a, "active"_a = true)
       .def("vertex_position", [](const Ptr m, UInt32 index, Mask active) {
                return m->vertex_position(index, active);
            }, D(Mesh, vertex_position), "index"_a, "active"_a = true)
       .def("vertex_normal", [](const Ptr m, UInt32 index, Mask active) {
                return m->vertex_normal(index, active);
            }, D(Mesh, vertex_normal), "index"_a, "active"_a = true)
       .def("vertex_texcoord", [](const Ptr m, UInt32 index, Mask active) {
                return m->vertex_texcoord(index, active);
            }, D(Mesh, vertex_texcoord), "index"_a, "active"_a = true)
       .def("face_normal", [](const Ptr m, UInt32 index, Mask active) {
                return m->face_normal(index, active);
            }, D(Mesh, face_normal), "index"_a, "active"_a = true)
       .def("opposite_dedge", [](const Ptr m, UInt32 index, Mask active) {
                return m->opposite_dedge(index, active);
            }, D(Mesh, opposite_dedge), "index"_a, "active"_a = true)

       .def("ray_intersect_triangle", [](const Ptr ptr, const UInt32 &index,
                                         const Ray3f &ray, Mask active) {
                return ptr->ray_intersect_triangle(index, ray, active);
            },
            "index"_a, "ray"_a, "active"_a = true,
            D(Mesh, ray_intersect_triangle));

    if constexpr (dr::is_array_v<Ptr> && dr::is_jit_v<Ptr>) {
        // Custom constructor to automatically zero-out non-Mesh pointer entries.
        // using ShapePtr = dr::replace_scalar_t<Ptr, Shape *>;
        cls.def("__init__", [](Ptr *dst) {
               // Zero-sized pointer array.
               new (dst) Ptr();
           })
           .def("__init__", [](Ptr *dst, const ShapePtr &ptr) {
                ShapePtr filtered = dr::select(ptr->is_mesh(), ptr, dr::zeros<ShapePtr>());
                Ptr mesh = dr::reinterpret_array<Ptr>(filtered);
                // Placement new.
                new (dst) Ptr(mesh);
            })
            .def("__init__", [](Ptr *dst, const Shape *ptr) {
                new (dst) Ptr(ptr->is_mesh() ? ptr : nullptr);
            })
            .def("__init__", [](Ptr *dst, const Mesh *ptr) {
                new (dst) Ptr(ptr);
            })
            .def("has_flipped_normals", [](Ptr shape) {
                return shape->has_flipped_normals();
            }, D(Shape, has_flipped_normals));
    }
}

MI_PY_EXPORT(Shape) {
    MI_PY_IMPORT_TYPES(Shape, Mesh)

    auto shape = MI_PY_CLASS(Shape, Object)
        .def("bbox", nb::overload_cast<>(
            &Shape::bbox, nb::const_), D(Shape, bbox))
        .def("bbox", nb::overload_cast<ScalarUInt32>(
            &Shape::bbox, nb::const_), D(Shape, bbox, 2), "index"_a)
        .def("bbox", nb::overload_cast<ScalarUInt32, const ScalarBoundingBox3f &>(
            &Shape::bbox, nb::const_), D(Shape, bbox, 3), "index"_a, "clip"_a)
        .def_method(Shape, add_texture_attribute, "name"_a, "texture"_a)
        .def("texture_attribute", nb::overload_cast<std::string_view>(
            &Shape::texture_attribute), D(Shape, texture_attribute), "name"_a)
        .def_method(Shape, remove_attribute, "name"_a)
        .def_method(Shape, is_mesh)
        .def_method(Shape, parameters_grad_enabled)
        .def_method(Shape, set_bsdf, "bsdf"_a)
        .def_method(Shape, primitive_count)
        .def_method(Shape, effective_primitive_count)
        .def_method(Shape, precompute_silhouette, "viewpoint"_a);

    drjit::bind_traverse(shape);

    bind_shape_generic<Shape *>(shape);

    if constexpr (dr::is_array_v<ShapePtr>) {
        dr::ArrayBinding b;
        auto shape_ptr = dr::bind_array_t<ShapePtr>(b, m, "ShapePtr");
        bind_shape_generic<ShapePtr>(shape_ptr);
    }

    using PyMesh = PyMesh<Float, Spectrum>;
    using ScalarSize = typename Mesh::ScalarSize;
    auto mesh_cls = MI_PY_TRAMPOLINE_CLASS(PyMesh, Mesh, Shape)
        .def(nb::init<const Properties&>(), "props"_a)
        .def(nb::init<const std::string &, ScalarSize, ScalarSize,
                      const Properties &, bool, bool>(),
             "name"_a, "vertex_count"_a, "face_count"_a,
             "props"_a = Properties(),
             "has_vertex_normals"_a = false, "has_vertex_texcoords"_a = false,
             D(Mesh, Mesh))
        .def_method(Mesh, initialize)
        .def("write_ply",
             nb::overload_cast<const std::string &>(&Mesh::write_ply, nb::const_),
             "filename"_a, D(Mesh, write_ply))
        .def("write_ply",
             nb::overload_cast<Stream *>(&Mesh::write_ply, nb::const_),
             "stream"_a, D(Mesh, write_ply, 2))
        .def("merge", &Mesh::merge, "other"_a,
             D(Mesh, merge))

        .def("vertex_positions_buffer", nb::overload_cast<>(&Mesh::vertex_positions_buffer))
        .def("vertex_normals_buffer", nb::overload_cast<>(&Mesh::vertex_normals_buffer))
        .def("vertex_texcoords_buffer", nb::overload_cast<>(&Mesh::vertex_texcoords_buffer))
        .def("faces_buffer", nb::overload_cast<>(&Mesh::faces_buffer))

        .def("attribute_buffer", &Mesh::attribute_buffer, "name"_a,
             D(Mesh, attribute_buffer))
        .def("add_attribute", &Mesh::add_attribute, "name"_a, "size"_a, "buffer"_a,
             D(Mesh, add_attribute))
        .def("remove_attribute", &Mesh::remove_attribute, "name"_a,
             D(Mesh, remove_attribute))

        .def("recompute_vertex_normals", &Mesh::recompute_vertex_normals)
        .def("recompute_bbox", &Mesh::recompute_bbox)
        .def("build_directed_edges", &Mesh::build_directed_edges);

    {
        using CornerAttribute = typename Mesh::CornerAttribute;
        using InputFloat = typename Mesh::InputFloat;
        using FloatNdArray =
            nb::ndarray<const InputFloat, nb::c_contig, nb::device::cpu>;
        using UInt32NdArray =
            nb::ndarray<const uint32_t, nb::c_contig, nb::device::cpu>;

        mesh_cls.def_static(
            "from_corners",
            [](const std::string &name, FloatNdArray positions,
               UInt32NdArray corner_vertex, nb::object normals,
               nb::object texcoords, nb::dict attributes,
               const Properties &props) -> nb::object {
                if (positions.ndim() != 2 || positions.shape(1) != 3)
                    Throw("Mesh.from_corners(): 'positions' must be a float32 "
                          "array of shape (vertex_count, 3)!");
                if (corner_vertex.ndim() != 1)
                    Throw("Mesh.from_corners(): 'corner_vertex' must be a "
                          "uint32 array of shape (corner_count,)!");

                size_t V = positions.shape(0),
                       C = corner_vertex.shape(0);

                auto cast_float = [](nb::handle h, const std::string &desc)
                    -> FloatNdArray {
                    FloatNdArray a;
                    if (!nb::try_cast(h, a))
                        Throw("Mesh.from_corners(): %s must be a "
                              "C-contiguous float32 array!", desc);
                    return a;
                };
                auto cast_uint32 = [](nb::handle h, const std::string &desc)
                    -> UInt32NdArray {
                    UInt32NdArray a;
                    if (!nb::try_cast(h, a))
                        Throw("Mesh.from_corners(): %s must be a "
                              "C-contiguous uint32 array!", desc);
                    return a;
                };

                // Keep the (potentially converted) arrays alive
                std::vector<FloatNdArray> farrs;
                std::vector<UInt32NdArray> iarrs;
                std::list<std::string> names;
                std::vector<CornerAttribute> attrs;

                /* Register one attribute given either as a per-corner value
                   array (C x dim) or as a (pool, indices) tuple. 'fixed_dim'
                   constrains the dimension (0: arbitrary). */
                auto add_attr = [&](std::string aname, nb::handle spec,
                                    size_t fixed_dim) {
                    names.push_back(std::move(aname));
                    const std::string &n = names.back();

                    CornerAttribute attr;
                    attr.name = n;

                    auto check_dim = [&](size_t dim) {
                        if (dim == 0 || (fixed_dim && dim != fixed_dim))
                            Throw("Mesh.from_corners(): attribute \"%s\": "
                                  "expected entries of dimension %zu, got "
                                  "%zu!", n, fixed_dim, dim);
                        attr.dim = dim;
                    };

                    if (nb::isinstance<nb::tuple>(spec)) {
                        nb::tuple t = nb::borrow<nb::tuple>(spec);
                        if (t.size() != 2)
                            Throw("Mesh.from_corners(): attribute \"%s\": "
                                  "expected a (pool, indices) tuple!", n);

                        FloatNdArray pool =
                            cast_float(t[0], "attribute \"" + n + "\" pool");
                        UInt32NdArray indices = cast_uint32(
                            t[1], "attribute \"" + n + "\" indices");

                        if (pool.ndim() != 1 && pool.ndim() != 2)
                            Throw("Mesh.from_corners(): attribute \"%s\": "
                                  "pool must have shape (N,) or (N, dim)!", n);
                        if (indices.ndim() != 1 || indices.shape(0) != C)
                            Throw("Mesh.from_corners(): attribute \"%s\": "
                                  "indices must have shape (corner_count,)!",
                                  n);
                        check_dim(pool.ndim() == 2 ? pool.shape(1) : 1);

                        size_t pool_count = pool.shape(0);
                        const uint32_t *idx = indices.data();
                        for (size_t i = 0; i < C; ++i) {
                            if (idx[i] >= pool_count &&
                                idx[i] != (uint32_t) -1)
                                Throw("Mesh.from_corners(): attribute "
                                      "\"%s\": index %u of corner %zu is out "
                                      "of bounds (pool size: %zu)!",
                                      n, idx[i], i, pool_count);
                        }

                        attr.pool = pool.data();
                        attr.indices = idx;
                        farrs.push_back(std::move(pool));
                        iarrs.push_back(std::move(indices));
                    } else {
                        FloatNdArray values =
                            cast_float(spec, "attribute \"" + n + "\"");
                        if ((values.ndim() != 1 && values.ndim() != 2) ||
                            values.shape(0) != C)
                            Throw("Mesh.from_corners(): attribute \"%s\": "
                                  "expected per-corner values of shape "
                                  "(corner_count,) or (corner_count, dim)!",
                                  n);
                        check_dim(values.ndim() == 2 ? values.shape(1) : 1);

                        attr.values = values.data();
                        farrs.push_back(std::move(values));
                    }

                    attrs.push_back(attr);
                };

                if (!normals.is_none())
                    add_attr("vertex_normals", normals, 3);
                if (!texcoords.is_none())
                    add_attr("vertex_texcoords", texcoords, 2);
                for (auto [k, v] : attributes)
                    add_attr(nb::cast<std::string>(k), v, 0);

                ref<Mesh> result;
                {
                    nb::gil_scoped_release release;
                    result = Mesh::from_corners(
                        name, V, C, positions.data(), corner_vertex.data(),
                        attrs.data(), attrs.size(), props);
                }
                return cast_object(result.get());
            },
            "name"_a, "positions"_a, "corner_vertex"_a,
            "normals"_a = nb::none(), "texcoords"_a = nb::none(),
            "attributes"_a = nb::dict(), "props"_a = Properties(),
            "Construct a mesh from per-corner (face-varying) data.\n"
            "\n"
            "Builds a single-indexed mesh from positions stored per vertex\n"
            "and attributes stored per face corner: vertices are split where\n"
            "corner attributes differ and welded where they agree. Corner\n"
            "``3*i+k`` is the k-th corner of triangle ``i``.\n"
            "\n"
            "Each attribute (``normals``, ``texcoords``, and the entries of\n"
            "``attributes``) accepts two forms:\n"
            "\n"
            "1. a float32 array of shape ``(corner_count, dim)`` with one\n"
            "   value per corner. Welding compares values bitwise: ``-0.0``\n"
            "   differs from ``0.0``, and NaN values never weld.\n"
            "2. a ``(pool, indices)`` tuple, where ``pool`` is a float32\n"
            "   array of shape ``(N, dim)`` and ``indices`` is a uint32\n"
            "   array of shape ``(corner_count,)``. Welding compares the\n"
            "   indices; the value ``0xffffffff`` marks a missing entry and\n"
            "   produces zeros in the output.\n"
            "\n"
            "Extra attribute names must start with ``vertex_``. When no\n"
            "normals are given (and the ``face_normals`` property is not\n"
            "set), smooth vertex normals are computed.");
    }

    bind_mesh_generic<Mesh *>(mesh_cls);

    if constexpr (dr::is_array_v<MeshPtr>) {
        dr::ArrayBinding b;
        auto mesh_ptr = dr::bind_array_t<MeshPtr>(b, m, "MeshPtr");
        bind_mesh_generic<MeshPtr>(mesh_ptr);
    }

    drjit::bind_traverse(mesh_cls);
}
