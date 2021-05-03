#include <mitsuba/render/denoiser.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(denoiser) {
    MI_PY_IMPORT_TYPES()
    m.def("denoise",
        py::overload_cast<const Bitmap &>(&denoise<Float>),
        "noisy"_a, D(denoise))
    .def("denoise",
        py::overload_cast<const Bitmap &, Bitmap *, Bitmap *>(&denoise<Float>),
        "noisy"_a, "albedo"_a, "normals"_a, D(denoise, 2));
}
