#include <mitsuba/core/properties.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

class PerspectiveCamera : public Sensor {
public:
    PerspectiveCamera(const Properties &props) : Sensor(props) {
        m_to_world = props.transform("to_world", Transform4f());
        std::cout << m_to_world << std::endl;
    }

    template <typename Point2, typename Float,
              typename Point3 = mitsuba::Point<Float, 3>,
              typename Spectrum = mitsuba::Spectrum<Float>,
              typename Ray = mitsuba::Ray<Point3>>
    auto sample_ray_impl(const Point2 &,//position_sample,
                         const Point2 &,//aperture_sample,
                         Float /*time_sample*/) const {
        Spectrum spec;
        Ray ray;
        return std::make_pair(ray, spec);
    }

    std::pair<Ray3f, Spectrumf>
    sample_ray(const Point2f &position_sample,
               const Point2f &aperture_sample,
               Float time_sample) const override {
        return sample_ray_impl(position_sample, aperture_sample, time_sample);
    }

    std::pair<Ray3fP, SpectrumfP>
    sample_ray(const Point2fP &position_sample,
               const Point2fP &aperture_sample,
               FloatP time_sample) const override {
        return sample_ray_impl(position_sample, aperture_sample, time_sample);
    }

    //std::string to_string() const override {
        //return tfm::format("PerspectiveCamera[radius=%f]", m_radius);
    //}

    MTS_DECLARE_CLASS()

private:
    Transform4f m_to_world;
};

MTS_IMPLEMENT_CLASS(PerspectiveCamera, Sensor);
MTS_EXPORT_PLUGIN(PerspectiveCamera, "Perspective Camera");
NAMESPACE_END(mitsuba)
