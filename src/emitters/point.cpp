#include <mitsuba/render/emitter.h>

#include <mitsuba/core/plugin.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

/*!\plugin{point}{Point light source}
 * \icon{emitter_point}
 * \order{1}
 * \parameters{
 *     \parameter{to_world}{\Transform\Or\Animation}{
 *        Specifies an optional sensor-to-world transformation.
 *        \default{none (i.e. sensor space $=$ world space)}
 *     }
 *     \parameter{position}{\Point}{
 *        Alternative parameter for specifying the light source
 *        position. Note that only one of the parameters
 *        \code{to_world} and \code{position} can be used at a time.
 *     }
 *     \parameter{intensity}{\Spectrumf}{
 *         Specifies the radiant intensity in units of
 *         power per unit steradian.
 *         \default{1}
 *     }
 * }
 *
 * This emitter plugin implements a simple point light source, which
 * uniformly radiates illumination into all directions.
 */
class PointLight : public Emitter {
public:
    PointLight(const Properties &props) : Emitter(props) {
        m_type |= EDeltaPosition;

        if (props.has_property("position")) {
            if (props.has_property("to_world")) {
                Log(EError, "Only one of the parameters 'position'"
                    " and 'to_world' can be used!'");
            }
            m_world_transform = new AnimatedTransform(
                Transform4f::translate(Vector3f(props.point3f("position")))
            );
        }

        m_intensity = props.spectrumf("intensity", Spectrumf::D65());
    }

    // PointLight(Stream *stream, InstanceManager *manager)
    //  : Emitter(stream, manager) {
    //     m_intensity = Spectrumf(stream);
    // }

    // void serialize(Stream *stream, InstanceManager *manager) const override {
    //     Emitter::serialize(stream, manager);
    //     m_intensity.serialize(stream);
    // }

    template <typename PositionSample,
              typename Point3 = typename PositionSample::Point3,
              typename Spectrum = Spectrum<value_t<Point3>>,
              typename Mask = mask_t<value_t<Point3>>>
    auto sample_position_impl(PositionSample &p_rec,
                              const Mask &active = true) const {
        using Normal3 = normal3_t<Point3>;

        const auto &trafo = m_world_transform->lookup(p_rec.time, active);
        masked(p_rec.p, active) = trafo * Point3(0.0f);
        masked(p_rec.n, active) = Normal3(0.0f);
        masked(p_rec.pdf, active) = 1.0f;
        masked(p_rec.measure, active) = EDiscrete;
        return Spectrum(m_intensity * (4.0f * math::Pi));
    }
    Spectrumf sample_position(
            PositionSample3f &p_rec, const Point2f &/*sample*/,
            const Point2f * /*extra*/) const override {
        return sample_position_impl(p_rec);
    }
    SpectrumfP sample_position(
            PositionSample3fP &p_rec, const Point2fP &/*sample*/,
            const Point2fP * /*extra*/,
            const mask_t<FloatP> &active = true) const override {
        return sample_position_impl(p_rec, active);
    }

    Spectrumf eval_position(const PositionSample3f &p_rec) const override {
        return (p_rec.measure == EDiscrete)
             ? (m_intensity * 4.0f * math::Pi) : Spectrumf(0.0f);
    }
    SpectrumfP eval_position(
            const PositionSample3fP &p_rec,
            const mask_t<FloatP> &/*active*/ = true) const override {
        SpectrumfP res(0.0f);
        res[eq(p_rec.measure, EDiscrete)] = m_intensity * 4.0f * math::Pi;
        return 0;
    }

    Float pdf_position(const PositionSample3f &p_rec) const override {
        return (p_rec.measure == EDiscrete) ? 1.0f : 0.0f;
    }
    FloatP pdf_position(const PositionSample3fP &p_rec,
                        const mask_t<FloatP> &/*active*/ = true) const override {
        return select(eq(p_rec.measure, EDiscrete), FloatP(1.0f), FloatP(0.0f));
    }

    template <typename DirectionSample,
              typename Point2 = typename DirectionSample::Point2,
              typename Spectrum = Spectrum<value_t<Point2>>,
              typename Mask = mask_t<value_t<Point2>>>
    auto sample_direction_impl(DirectionSample &d_rec,
                               const Point2 &sample,
                               const Mask &active = true) const {
        masked(d_rec.d, active) = warp::square_to_uniform_sphere(sample);
        masked(d_rec.pdf, active) = math::InvFourPi;
        masked(d_rec.measure, active) = ESolidAngle;
        return Spectrum(1.0f);
    }
    Spectrumf sample_direction(
            DirectionSample3f &d_rec, PositionSample3f &/*p_rec*/,
            const Point2f &sample, const Point2f * /*extra*/) const override {
        return sample_direction_impl(d_rec, sample);
    }
    SpectrumfP sample_direction(
            DirectionSample3fP &d_rec, PositionSample3fP &/*p_rec*/,
            const Point2fP &sample, const Point2fP * /*extra*/,
            const mask_t<FloatP> &active = true) const override {
        return sample_direction_impl(d_rec, sample, active);
    }

    Float pdf_direction(const DirectionSample3f &d_rec,
                        const PositionSample3f &/*p_rec*/) const override {
        return (d_rec.measure == ESolidAngle) ? math::InvFourPi : 0.0f;
    }
    FloatP pdf_direction(const DirectionSample3fP &d_rec,
                         const PositionSample3fP &/*p_rec*/,
                         const mask_t<FloatP> &/*active*/ = true) const override {
        return select(eq(d_rec.measure, ESolidAngle),
                      FloatP(math::InvFourPi), FloatP(0.0f));
    }

    Spectrumf eval_direction(const DirectionSample3f &d_rec,
                             const PositionSample3f &/*p_rec*/) const override {
        return Spectrumf((d_rec.measure == ESolidAngle) ? math::InvFourPi : 0.0f);
    }
    SpectrumfP eval_direction(
            const DirectionSample3fP &d_rec, const PositionSample3fP &/*p_rec*/,
            const mask_t<FloatP> &/*active*/ = true) const override {
        return SpectrumfP(
            select(eq(d_rec.measure, ESolidAngle),
                   SpectrumfP(math::InvFourPi), SpectrumfP(0.0f))
        );
    }

    template <typename Point2, typename Value = value_t<Point2>,
              typename Mask = mask_t<Value>>
    auto sample_ray_impl(const Point2 &direction_sample,
                         Value time_sample, const Mask &/*active*/ = true) const {
        using Point3 = Point<Value, 3>;
        using Ray3 = Ray<Point3>;
        using Spectrum = Spectrum<Value>;

        const auto &trafo = m_world_transform->lookup(time_sample);
        Ray3 ray;
        ray.time = time_sample;
        ray.o = trafo * Point3(0.0f);
        ray.d = warp::square_to_uniform_sphere(direction_sample);
        return std::make_pair(ray, Spectrum(m_intensity * (4.0f * math::Pi)));
    }
    std::pair<Ray3f, Spectrumf> sample_ray(
            const Point2f &/*position_sample*/, const Point2f &direction_sample,
            Float time_sample) const override {
        return sample_ray_impl(direction_sample, time_sample);
    }
    std::pair<Ray3fP, SpectrumfP> sample_ray(
            const Point2fP &/*position_sample*/,
            const Point2fP &direction_sample,
            FloatP time_sample,
            const mask_t<FloatP> &active = true) const override {
        return sample_ray_impl(direction_sample, time_sample, active);
    }

    template <typename DirectSample,
              typename Value = value_t<typename DirectSample::Point2>,
              typename Spectrum = Spectrum<Value>,
              typename Mask = mask_t<Value>>
    Spectrum sample_direct_impl(DirectSample &d_rec,
                                const Mask &active = true) const {
        using Point2 = Point<Value, 2>;
        using Point3 = Point<Value, 3>;
        using Normal3 = normal3_t<Point3>;

        const auto &trafo = m_world_transform->lookup(d_rec.time, active);
        masked(d_rec.p, active) = trafo.transform_affine(Point3(0.0f));
        masked(d_rec.pdf, active) = 1.0f;
        masked(d_rec.measure, active) = EDiscrete;
        masked(d_rec.uv, active) = Point2(0.5f);
        masked(d_rec.d, active) = d_rec.p - d_rec.ref_p;
        masked(d_rec.dist, active) = norm(d_rec.d);
        Value inv_dist = rcp(d_rec.dist);
        masked(d_rec.d, active) *= inv_dist;
        masked(d_rec.n, active) = Normal3(0.0f);

        return Spectrum(m_intensity) * (inv_dist * inv_dist);
    }
    Spectrumf sample_direct(
            DirectSample3f &d_rec, const Point2f &/*sample*/) const override {
        return sample_direct_impl(d_rec);
    }
    SpectrumfP sample_direct(
        DirectSample3fP &d_rec, const Point2fP &/*sample*/,
        const mask_t<FloatP> &active = true) const override {
        return sample_direct_impl(d_rec, active);
    }

    Float pdf_direct(const DirectSample3f &d_rec) const override {
        return d_rec.measure == EDiscrete ? 1.0f : 0.0f;
    }
    FloatP pdf_direct(const DirectSample3fP &d_rec,
                      const mask_t<FloatP> &/*active*/ = true) const override {
        return select(eq(d_rec.measure, EDiscrete), FloatP(1.0f), FloatP(0.0f));
    }

    BoundingBox3f bbox() const override {
        return m_world_transform->translation_bounds();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "PointLight[" << std::endl
            << "  world_transform = " << string::indent(m_world_transform->to_string()) << "," << std::endl
            << "  intensity = " << m_intensity << "," << std::endl
            << "  medium = " << (m_medium
                               ? string::indent(m_medium->to_string()) : "")
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    Spectrumf m_intensity;
};


MTS_IMPLEMENT_CLASS(PointLight, Emitter)
MTS_EXPORT_PLUGIN(PointLight, "Point emitter");
NAMESPACE_END(mitsuba)
