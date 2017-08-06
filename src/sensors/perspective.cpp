#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

class PerspectiveCamera : public Sensor {
public:
    PerspectiveCamera(const Properties &props) : Sensor() {
    }

    //std::string to_string() const override {
        //return tfm::format("PerspectiveCamera[radius=%f]", m_radius);
    //}

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS(PerspectiveCamera, Sensor);
MTS_EXPORT_PLUGIN(PerspectiveCamera, "Perspective Camera");
NAMESPACE_END(mitsuba)
