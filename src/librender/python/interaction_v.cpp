#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Interaction) {
    MTS_PY_IMPORT_TYPES_DYNAMIC()
    auto inter = py::class_<Interaction3f>(m, "Interaction3f", D(Interaction))
        // Members
        .def_field(Interaction3f, t,           D(Interaction, t))
        .def_field(Interaction3f, time,        D(Interaction, time))
        .def_field(Interaction3f, wavelengths, D(Interaction, wavelengths))
        .def_field(Interaction3f, p,           D(Interaction, p))
        // Methods
        .def(py::init<>(), D(Interaction, Interaction))
        .def("spawn_ray",    &Interaction3f::spawn_ray, "d"_a,    D(Interaction, spawn_ray))
        .def("spawn_ray_to", &Interaction3f::spawn_ray_to, "t"_a, D(Interaction, spawn_ray_to))
        .def("is_valid",     &Interaction3f::is_valid,     D(Interaction, is_valid))
        .def_repr(Interaction3f);
    bind_slicing_operators<Interaction3f, Interaction<ScalarFloat, scalar_spectrum_t<Spectrum>>>(inter);
}


template<typename Class, typename PyClass>
void bind_slicing_operator_surfaceinteraction(PyClass &cl) {
    using Float = typename Class::Float;
    if constexpr (is_dynamic_v<Float> && !is_cuda_array_v<Float>) { // TODO could we enable this for CUDA?
        cl.def("__getitem__", [](Class &si, size_t i) {
            if (i >= slices(si))
                throw py::index_error();

            Class res = zero<Class>(1);
            res.t           = slice(si.t, i);
            res.time        = slice(si.time, i);
            res.wavelengths = slice(si.wavelengths, i);
            res.p           = slice(si.p, i);
            res.shape       = si.shape[i];
            res.uv          = slice(si.uv, i);
            res.n           = slice(si.n, i);
            res.sh_frame    = slice(si.sh_frame, i);
            res.dp_du       = slice(si.dp_du, i);
            res.dp_dv       = slice(si.dp_dv, i);
            res.dn_du       = slice(si.dn_du, i);
            res.dn_dv       = slice(si.dn_dv, i);
            res.duv_dx      = slice(si.duv_dx, i);
            res.duv_dy      = slice(si.duv_dy, i);
            res.wi          = slice(si.wi, i);
            res.prim_index  = slice(si.prim_index, i);
            res.instance    = si.instance[i];
            return res;
        })
        .def("__setitem__", [](Class &r, size_t i,
                                const Class &r2) {
            if (i >= slices(r))
                throw py::index_error();

            if (slices(r2) != 1)
                throw py::index_error();

            slice(r.t, i)           = slice(r2.t, 0);
            slice(r.time, i)        = slice(r2.time, 0);
            slice(r.wavelengths, i) = slice(r2.wavelengths, 0);
            slice(r.p, i)           = slice(r2.p, 0);
            r.shape[i]                     = slice(r2.shape, 0);
            slice(r.uv, i)          = slice(r2.uv, 0);
            slice(r.n, i)           = slice(r2.n, 0);
            slice(r.sh_frame, i)    = slice(r2.sh_frame, 0);
            slice(r.dp_du, i)       = slice(r2.dp_du, 0);
            slice(r.dp_dv, i)       = slice(r2.dp_dv, 0);
            slice(r.dn_du, i)       = slice(r2.dn_du, 0);
            slice(r.dn_dv, i)       = slice(r2.dn_dv, 0);
            slice(r.duv_dx, i)      = slice(r2.duv_dx, 0);
            slice(r.duv_dy, i)      = slice(r2.duv_dy, 0);
            slice(r.wi, i)          = slice(r2.wi, 0);
            slice(r.prim_index, i)  = slice(r2.prim_index, 0);
            r.instance[i]                  = slice(r2.instance, 0);
        })
        .def("__len__", [](const Class &r) {
            return slices(r);
        });
    }
    cl.def_static("zero", [](size_t size) {
            if constexpr (!is_dynamic_v<Float>) {
                if (size != 1)
                    throw std::runtime_error("zero(): Size must equal 1 in scalar mode!");
            }
            return zero<Class>(size);
        }, "size"_a = 1
    );
}

MTS_PY_EXPORT(SurfaceInteraction) {
    MTS_PY_IMPORT_TYPES_DYNAMIC()
    auto inter =
        py::class_<SurfaceInteraction3f, Interaction3f>(m, "SurfaceInteraction3f",
                                                        D(SurfaceInteraction))
        // Members
        .def_field(SurfaceInteraction3f, shape,      D(SurfaceInteraction, shape))
        .def_field(SurfaceInteraction3f, uv,         D(SurfaceInteraction, uv))
        .def_field(SurfaceInteraction3f, n,          D(SurfaceInteraction, n))
        .def_field(SurfaceInteraction3f, sh_frame,   D(SurfaceInteraction, sh_frame))
        .def_field(SurfaceInteraction3f, dp_du,      D(SurfaceInteraction, dp_du))
        .def_field(SurfaceInteraction3f, dp_dv,      D(SurfaceInteraction, dp_dv))
        .def_field(SurfaceInteraction3f, dn_du,      D(SurfaceInteraction, dn_du))
        .def_field(SurfaceInteraction3f, dn_dv,      D(SurfaceInteraction, dn_dv))
        .def_field(SurfaceInteraction3f, duv_dx,     D(SurfaceInteraction, duv_dx))
        .def_field(SurfaceInteraction3f, duv_dy,     D(SurfaceInteraction, duv_dy))
        .def_field(SurfaceInteraction3f, wi,         D(SurfaceInteraction, wi))
        .def_field(SurfaceInteraction3f, prim_index, D(SurfaceInteraction, prim_index))
        .def_field(SurfaceInteraction3f, instance,   D(SurfaceInteraction, instance))

        // Methods
        .def(py::init<>(), D(SurfaceInteraction, SurfaceInteraction))
        .def(py::init<const PositionSample3f &, const Wavelength &>(), "ps"_a,
            "wavelengths"_a, D(SurfaceInteraction, SurfaceInteraction))
        .def("to_world", &SurfaceInteraction3f::to_world, "v"_a, D(SurfaceInteraction, to_world))
        .def("to_local", &SurfaceInteraction3f::to_local, "v"_a, D(SurfaceInteraction, to_local))
        .def("to_world_mueller", &SurfaceInteraction3f::to_world_mueller, "M_local"_a,
            "wi_local"_a, "wo_local"_a, D(SurfaceInteraction, to_world_mueller))
        .def("to_local_mueller", &SurfaceInteraction3f::to_local_mueller, "M_world"_a,
            "wi_world"_a, "wo_world"_a, D(SurfaceInteraction, to_local_mueller))
        .def("emitter", &SurfaceInteraction3f::emitter, D(SurfaceInteraction, emitter),
            "scene"_a, "active"_a = true)
        .def("is_sensor", &SurfaceInteraction3f::is_sensor, D(SurfaceInteraction, is_sensor))
        .def("is_medium_transition", &SurfaceInteraction3f::is_medium_transition,
            D(SurfaceInteraction, is_medium_transition))
        .def("target_medium",
            py::overload_cast<const Vector3f &>(&SurfaceInteraction3f::target_medium,
                                                py::const_),
            "d"_a, D(SurfaceInteraction, target_medium))
        .def("target_medium",
            py::overload_cast<const Float &>(&SurfaceInteraction3f::target_medium,
                                            py::const_),
            "cos_theta"_a, D(SurfaceInteraction, target_medium, 2))
        .def("bsdf",
            py::overload_cast<const RayDifferential3f &>(&SurfaceInteraction3f::bsdf),
            "ray"_a, D(SurfaceInteraction, bsdf))
        .def("bsdf", py::overload_cast<>(&SurfaceInteraction3f::bsdf, py::const_),
            D(SurfaceInteraction, bsdf, 2))
        .def("compute_uv_partials", &SurfaceInteraction3f::compute_uv_partials, "ray"_a,
            D(SurfaceInteraction, compute_uv_partials))
        .def("has_uv_partials", &SurfaceInteraction3f::has_uv_partials,
            D(SurfaceInteraction, has_uv_partials))
        .def("has_n_partials", &SurfaceInteraction3f::has_n_partials,
            D(SurfaceInteraction, has_n_partials))
        .def_repr(SurfaceInteraction3f);

    // Manually bind the slicing operators to handle ShapePtr properly
    bind_slicing_operator_surfaceinteraction<SurfaceInteraction3f>(inter);
}

template<typename Class, typename PyClass>
void bind_slicing_operator_mediuminteraction(PyClass &cl) {
    using Float = typename Class::Float;
    if constexpr (is_dynamic_v<Float> && !is_cuda_array_v<Float>) { // TODO could we enable this for CUDA?
        cl.def("__getitem__", [](Class &mi, size_t i) {
            if (i >= slices(mi))
                throw py::index_error();

            Class res = zero<Class>(1);
            res.t           = slice(mi.t, i);
            res.time        = slice(mi.time, i);
            res.wavelengths = slice(mi.wavelengths, i);
            res.p           = slice(mi.p, i);
            res.medium      = mi.medium[i];
            res.sh_frame    = slice(mi.sh_frame, i);
            res.wi          = slice(mi.wi, i);
            res.mint        = slice(mi.mint, i);
            return res;
        })
        .def("__setitem__", [](Class &r, size_t i,
                                const Class &r2) {
            if (i >= slices(r))
                throw py::index_error();

            if (slices(r2) != 1)
                throw py::index_error();

            slice(r.t, i)           = slice(r2.t, 0);
            slice(r.time, i)        = slice(r2.time, 0);
            slice(r.wavelengths, i) = slice(r2.wavelengths, 0);
            slice(r.p, i)           = slice(r2.p, 0);
            r.medium[i]             = slice(r2.medium, 0);
            slice(r.sh_frame, i)    = slice(r2.sh_frame, 0);
            slice(r.wi, i)          = slice(r2.wi, 0);
            slice(r.mint, i)        = slice(r2.mint, 0);
        })
        .def("__len__", [](const Class &r) {
            return slices(r);
        });
    }
    cl.def_static("zero", [](size_t size) {
            if constexpr (!is_dynamic_v<Float>) {
                if (size != 1)
                    throw std::runtime_error("zero(): Size must equal 1 in scalar mode!");
            }
            return zero<Class>(size);
        }, "size"_a = 1
    );
}

MTS_PY_EXPORT(MediumInteraction) {
    MTS_PY_IMPORT_TYPES_DYNAMIC()
    auto inter =
        py::class_<MediumInteraction3f, Interaction3f>(m, "MediumInteraction3f",
                                                        D(MediumInteraction))
        // Members
        .def_field(MediumInteraction3f, medium,     D(MediumInteraction, medium))
        .def_field(MediumInteraction3f, sh_frame,   D(MediumInteraction, sh_frame))
        .def_field(MediumInteraction3f, wi,         D(MediumInteraction, wi))

        // Methods
        .def(py::init<>(), D(MediumInteraction, MediumInteraction))
        .def("to_world", &MediumInteraction3f::to_world, "v"_a, D(MediumInteraction, to_world))
        .def("to_local", &MediumInteraction3f::to_local, "v"_a, D(MediumInteraction, to_local))
        .def_repr(MediumInteraction3f);

    // Manually bind the slicing operators to handle ShapePtr properly
    bind_slicing_operator_mediuminteraction<MediumInteraction3f>(inter);
}

// MSVC hack to force non compilation for packet variants
template<typename PreliminaryIntersection3f, typename PyClass>
void bind_method_preliminaryintersection(PyClass &pi) {
    // Do not binding this method for packet variants
    if constexpr(is_cuda_array_v<typename PreliminaryIntersection3f::Float> ||
                 is_scalar_v<typename PreliminaryIntersection3f::Float>) {
        pi.def("compute_surface_interaction", &PreliminaryIntersection3f::compute_surface_interaction,
               D(PreliminaryIntersection, compute_surface_interaction),
               "ray"_a, "flags"_a = HitComputeFlags::All, "active"_a = true);
    }
}

MTS_PY_EXPORT(PreliminaryIntersection) {
    MTS_PY_IMPORT_TYPES_DYNAMIC()

    m.def("has_flag", [](HitComputeFlags f0, HitComputeFlags f1) { return has_flag(f0, f1); });

    auto pi =
        py::class_<PreliminaryIntersection3f>(m, "PreliminaryIntersection3f",
                                              D(PreliminaryIntersection))
        // Members
        .def_field(PreliminaryIntersection3f, t,           D(PreliminaryIntersection, t))
        .def_field(PreliminaryIntersection3f, prim_uv,     D(PreliminaryIntersection, prim_uv))
        .def_field(PreliminaryIntersection3f, prim_index,  D(PreliminaryIntersection, prim_index))
        .def_field(PreliminaryIntersection3f, shape_index, D(PreliminaryIntersection, shape_index))
        .def_field(PreliminaryIntersection3f, shape,       D(PreliminaryIntersection, shape))
        .def_field(PreliminaryIntersection3f, instance,    D(PreliminaryIntersection, instance))

        // Methods
        .def(py::init<>(), D(PreliminaryIntersection, PreliminaryIntersection))
        .def("is_valid", &PreliminaryIntersection3f::is_valid, D(PreliminaryIntersection, is_valid))
        .def_repr(PreliminaryIntersection3f);

    bind_method_preliminaryintersection<PreliminaryIntersection3f>(pi);

    // TODO bind slicing operator
}