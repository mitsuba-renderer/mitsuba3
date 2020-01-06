#include <mitsuba/python/python.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>

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
        .def("spawn_ray",    &Interaction3f::spawn_ray,    D(Interaction, spawn_ray))
        .def("spawn_ray_to", &Interaction3f::spawn_ray_to, D(Interaction, spawn_ray_to))
        .def("is_valid",     &Interaction3f::is_valid,     D(Interaction, is_valid))
        .def_repr(Interaction3f);
    bind_slicing_operators<Interaction3f, Interaction<ScalarFloat, scalar_spectrum_t<Spectrum>>>(inter);
}


template<typename Class, typename PyClass>
void bind_slicing_operator_surfaceinteraction(PyClass &cl) {
    using Float = typename Class::Float;
    if constexpr (is_dynamic_v<Float>) {
        cl.def("__getitem__", [](Class &si, size_t i) {
            if (i >= slices(si))
                throw py::index_error();

            Class res = zero<Class>(1);
            res.t           = enoki::slice(si.t, i);
            res.time        = enoki::slice(si.time, i);
            res.wavelengths = enoki::slice(si.wavelengths, i);
            res.p           = enoki::slice(si.p, i);
            res.shape       = si.shape[i];
            res.uv          = enoki::slice(si.uv, i);
            res.n           = enoki::slice(si.n, i);
            res.sh_frame    = enoki::slice(si.sh_frame, i);
            res.dp_du       = enoki::slice(si.dp_du, i);
            res.dp_dv       = enoki::slice(si.dp_dv, i);
            res.duv_dx      = enoki::slice(si.duv_dx, i);
            res.duv_dy      = enoki::slice(si.duv_dy, i);
            res.wi          = enoki::slice(si.wi, i);
            res.prim_index  = enoki::slice(si.prim_index, i);
            res.instance    = si.instance[i];
            return res;
        })
        .def("__setitem__", [](Class &r, size_t i,
                                const Class &r2) {
            if (i >= slices(r))
                throw py::index_error();

            if (slices(r2) != 1)
                throw py::index_error();

            enoki::slice(r.t, i)           = enoki::slice(r2.t, 0);
            enoki::slice(r.time, i)        = enoki::slice(r2.time, 0);
            enoki::slice(r.wavelengths, i) = enoki::slice(r2.wavelengths, 0);
            enoki::slice(r.p, i)           = enoki::slice(r2.p, 0);
            r.shape[i]                     = enoki::slice(r2.shape, 0);
            enoki::slice(r.uv, i)          = enoki::slice(r2.uv, 0);
            enoki::slice(r.n, i)           = enoki::slice(r2.n, 0);
            enoki::slice(r.sh_frame, i)    = enoki::slice(r2.sh_frame, 0);
            enoki::slice(r.dp_du, i)       = enoki::slice(r2.dp_du, 0);
            enoki::slice(r.dp_dv, i)       = enoki::slice(r2.dp_dv, 0);
            enoki::slice(r.duv_dx, i)      = enoki::slice(r2.duv_dx, 0);
            enoki::slice(r.duv_dy, i)      = enoki::slice(r2.duv_dy, 0);
            enoki::slice(r.wi, i)          = enoki::slice(r2.wi, 0);
            enoki::slice(r.prim_index, i)  = enoki::slice(r2.prim_index, 0);
            r.instance[i]                  = enoki::slice(r2.instance, 0);
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
        .def_field(SurfaceInteraction3f, duv_dx,     D(SurfaceInteraction, duv_dx))
        .def_field(SurfaceInteraction3f, duv_dy,     D(SurfaceInteraction, duv_dy))
        .def_field(SurfaceInteraction3f, wi,         D(SurfaceInteraction, wi))
        .def_field(SurfaceInteraction3f, prim_index, D(SurfaceInteraction, prim_index))
        .def_field(SurfaceInteraction3f, instance,   D(SurfaceInteraction, instance))
        // // Methods
        .def(py::init<>(), D(SurfaceInteraction, SurfaceInteraction))
        .def(py::init<const PositionSample3f &, const Wavelength &>(), "ps"_a,
            "wavelengths"_a, D(SurfaceInteraction, SurfaceInteraction))
        .def("to_world", &SurfaceInteraction3f::to_world, D(SurfaceInteraction, to_world))
        .def("to_local", &SurfaceInteraction3f::to_local, D(SurfaceInteraction, to_local))
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
        .def("compute_partials", &SurfaceInteraction3f::compute_partials,
            D(SurfaceInteraction, compute_partials))
        .def("has_uv_partials", &SurfaceInteraction3f::has_uv_partials,
            D(SurfaceInteraction, has_uv_partials))
        // .def("normal_derivative", &SurfaceInteraction3f::normal_derivative, D(SurfaceInteraction, normal_derivative)) // TODO
        .def_repr(SurfaceInteraction3f);

    // Manually bind the slicing operators to handle ShapePtr properly
    bind_slicing_operator_surfaceinteraction<SurfaceInteraction3f>(inter);
}
