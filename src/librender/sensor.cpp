#include <mitsuba/render/sensor.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/logger.h>

NAMESPACE_BEGIN(mitsuba)

Sensor::Sensor(const Properties &props) {
    m_shutter_open      = props.float_("shutter_open", 0.f);
    m_shutter_open_time = props.float_("shutter_close", 0.f) - m_shutter_open;

    if (m_shutter_open_time < 0)
        Throw("Shutter opening time must be less than or equal to the shutter "
              "closing time!");
}

Sensor::~Sensor() { }

MTS_IMPLEMENT_CLASS(Sensor, Object)
NAMESPACE_END(mitsuba)
