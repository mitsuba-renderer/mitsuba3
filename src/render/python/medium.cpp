#include <mitsuba/render/medium.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(MediumExtras) {
    auto e = py::enum_<MediumEventSamplingMode>(m, "MediumEventSamplingMode", D(MediumEventSamplingMode))
        .def_value(MediumEventSamplingMode, Analogue)
        .def_value(MediumEventSamplingMode, Maximum)
        .def_value(MediumEventSamplingMode, Mean);

        MI_PY_DECLARE_ENUM_OPERATORS(MediumEventSamplingMode, e)
}
