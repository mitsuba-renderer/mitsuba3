#include <mitsuba/render/microflake.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(MicroflakeDistribution) {
    MI_PY_IMPORT_TYPES()

    m.def(
        "sggx_sample",
        [](const Frame3f &frame, const Point2f &sample,
           const std::array<Float, 6> &s) {
            dr::Array<Float, 6> s_drjit_array(s[0], s[1], s[2], s[3], s[4], s[5]);
            return sggx_sample(frame, sample, s_drjit_array);
        },
        "sh_frame"_a, "sample"_a, "s"_a, D(sggx_sample));

    m.def(
        "sggx_sample",
        [](const Vector3f &wi, const Point2f &sample,
           const std::array<Float, 6> &s) {
            dr::Array<Float, 6> s_drjit_array(s[0], s[1], s[2], s[3], s[4], s[5]);
            return sggx_sample(wi, sample, s_drjit_array);
        },
        "wi"_a, "sample"_a, "s"_a, D(sggx_sample));

    m.def(
        "sggx_pdf",
        [](const Vector3f &wm, const std::array<Float, 6> &s) {
            dr::Array<Float, 6> s_drjit_array(s[0], s[1], s[2], s[3], s[4], s[5]);
            return sggx_pdf(wm, s_drjit_array);
        },
        "wm"_a, "s"_a, D(sggx_pdf));

    m.def(
        "sggx_projected_area",
        [](const Vector3f &wi, const std::array<Float, 6> &s) {
            dr::Array<Float, 6> s_drjit_array(s[0], s[1], s[2], s[3], s[4], s[5]);
            return sggx_projected_area(wi, s_drjit_array);
        },
        "wi"_a, "s"_a, D(sggx_projected_area));
}
