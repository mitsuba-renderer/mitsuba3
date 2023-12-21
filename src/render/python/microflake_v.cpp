#include <mitsuba/render/microflake.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(MicroflakeDistribution) {
    MI_PY_IMPORT_TYPES()

    m.def("sggx_sample",
          py::overload_cast<const Frame3f&,
                            const Point2f&,
                            const dr::Array<Float, 6>&>(sggx_sample<Float>),
          "sh_frame"_a, "sample"_a, "s"_a, D(sggx_sample));

    m.def("sggx_sample",
          py::overload_cast<const Vector3f&,
                            const Point2f&,
                            const dr::Array<Float, 6>&>(sggx_sample<Float>),
          "sh_frame"_a, "sample"_a, "s"_a, D(sggx_sample));

    m.def("sggx_pdf", &sggx_pdf<Float>,
          "wm"_a, "s"_a, D(sggx_pdf));

    m.def("sggx_projected_area", &sggx_projected_area<Float>,
          "wi"_a, "s"_a, D(sggx_projected_area));
}
