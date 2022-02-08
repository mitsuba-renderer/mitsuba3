#include <mitsuba/render/sensor.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(Sensor) {
    m.def("parse_fov", &parse_fov, "props"_a, "aspect"_a, D(parse_fov));
}
