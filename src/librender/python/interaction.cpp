#include <mitsuba/python/python.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>

MTS_PY_EXPORT_STRUCT(Interaction) {
    MTS_IMPORT_TYPES()
    MTS_PY_CHECK_ALIAS(Interaction3f, m) {
        auto inter = py::class_<Interaction3f>(m, "Interaction3f", D(Interaction3f))
            // Members
            .def_field(Interaction3f, t)
            .def_field(Interaction3f, time)
            .def_field(Interaction3f, wavelengths)
            .def_field(Interaction3f, p)
            // Methods
            .def(py::init<>(), D(Interaction3f, Interaction3f))
            .def_method(Interaction3f, spawn_ray)
            .def_method(Interaction3f, spawn_ray_to)
            .def_method(Interaction3f, is_valid)
            .def_repr(Interaction3f);
        bind_slicing_operators<Interaction3f, Interaction<ScalarFloat, scalar_spectrum_t<Spectrum>>>(inter);
    }
}

MTS_PY_EXPORT_STRUCT(SurfaceInteraction) {
    MTS_IMPORT_TYPES()
    MTS_PY_CHECK_ALIAS(SurfaceInteraction3f, m) {
        auto inter = py::class_<SurfaceInteraction3f, Interaction3f>(m, "SurfaceInteraction3f", D(SurfaceInteraction3f))
            // Members
            .def_field(SurfaceInteraction3f, shape)
            .def_field(SurfaceInteraction3f, uv)
            .def_field(SurfaceInteraction3f, n)
            .def_field(SurfaceInteraction3f, sh_frame)
            .def_field(SurfaceInteraction3f, dp_du)
            .def_field(SurfaceInteraction3f, dp_dv)
            .def_field(SurfaceInteraction3f, duv_dx)
            .def_field(SurfaceInteraction3f, duv_dy)
            .def_field(SurfaceInteraction3f, wi)
            .def_field(SurfaceInteraction3f, prim_index)
            .def_field(SurfaceInteraction3f, instance)
            // // Methods
            .def(py::init<>(), D(SurfaceInteraction3f, SurfaceInteraction3f))
            .def(py::init<const PositionSample3f &, const Wavelength &>(),
                "ps"_a, "wavelengths"_a, D(SurfaceInteraction3f, SurfaceInteraction3f))
            .def_method(SurfaceInteraction3f, to_world)
            .def_method(SurfaceInteraction3f, to_local)
            .def_method(SurfaceInteraction3f, to_world_mueller,
                        "M_local"_a, "wi_local"_a, "wo_local"_a)
            .def_method(SurfaceInteraction3f, to_local_mueller,
                        "M_world"_a, "wi_world"_a, "wo_world"_a)
            .def_method(SurfaceInteraction3f, emitter, "scene"_a, "active"_a = true)
            .def_method(SurfaceInteraction3f, is_sensor)
            .def_method(SurfaceInteraction3f, is_medium_transition)
            .def("target_medium",
                py::overload_cast<const Vector3f &>(&SurfaceInteraction3f::target_medium, py::const_),
                "d"_a, D(SurfaceInteraction3f, target_medium))
            .def("target_medium",
                py::overload_cast<const Float &>(&SurfaceInteraction3f::target_medium, py::const_),
                "cos_theta"_a, D(SurfaceInteraction3f, target_medium, 2))
            .def("bsdf", py::overload_cast<const RayDifferential3f &>(&SurfaceInteraction3f::bsdf),
                "ray"_a, D(SurfaceInteraction3f, bsdf))
            .def("bsdf", py::overload_cast<>(&SurfaceInteraction3f::bsdf, py::const_),
                D(SurfaceInteraction3f, bsdf, 2))
            // .def_method(SurfaceInteraction3f, normal_derivative)
            .def_method(SurfaceInteraction3f, compute_partials)
            .def_method(SurfaceInteraction3f, has_uv_partials)
            .def_repr(SurfaceInteraction3f);

        // Manually bind the slicing operators to handle ShapePtr properly
        if constexpr (is_dynamic_v<Float> && !is_cuda_array_v<Float>) {
            inter.def(py::init([](size_t n) -> SurfaceInteraction3f {
                return zero<SurfaceInteraction3f>(n);
            }))
            .def("__getitem__", [](SurfaceInteraction3f &si, size_t i) {
                if (i >= slices(si))
                    throw py::index_error();

                // ScalarSurfaceInteraction3f res;
                SurfaceInteraction3f res = zero<SurfaceInteraction3f>(1);
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
            .def("__setitem__", [](SurfaceInteraction3f &r, size_t i,
                                   const SurfaceInteraction3f &r2) {
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
            .def("__len__", [](const SurfaceInteraction3f &r) {
                return slices(r);
            });
        }
    }
}
