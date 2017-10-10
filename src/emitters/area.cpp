#include <mitsuba/render/emitter.h>

#include <mitsuba/core/frame.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

/*!\plugin{area}{Area light}
 * \icon{emitter_area}
 * \order{2}
 * \parameters{
 *     \parameter{radiance}{\Spectrumf}{
 *         Specifies the emitted radiance in units of
 *         power per unit area per unit steradian.
 *     }
 *     \parameter{samplinf_weight}{\Float}{
 *         Specifies the relative amount of samples
 *         allocated to this emitter. \default{1}
 *     }
 * }
 *
 * This plugin implements an area light, i.e. a light source that emits
 * diffuse illumination from the exterior of an arbitrary shape.
 * Since the emission profile of an area light is completely diffuse, it
 * has the same apparent brightness regardless of the observer's viewing
 * direction. Furthermore, since it occupies a nonzero amount of space, an
 * area light generally causes scene objects to cast soft shadows.
 *
 * When modeling scenes involving area lights, it is preferable
 * to use spheres as the emitter shapes, since they provide a
 * particularly good direct illumination sampling strategy (see
 * the \pluginref{sphere} plugin for an example).
 *
 * To create an area light source, simply instantiate the desired
 * emitter shape and specify an \code{area} instance as its child:
 *
 * \vspace{4mm}
 * \begin{xml}
 * <!-- Create a spherical light source at the origin -->
 * <shape type="sphere">
 *     <emitter type="area">
 *         <spectrum name="radiance" value="1"/>
 *     </emitter>
 * </shape>
 * \end{xml}
 */
class AreaLight : public Emitter {
public:
    AreaLight(const Properties &props) : Emitter(props) {
        m_type |= EOnSurface;

        if (props.has_property("to_world")) {
            Log(EError, "Found a 'to_world' transformation -- this is not"
                " allowed -- the area light inherits this transformation from"
                " its parent shape.");
        }

        m_radiance = props.spectrumf("radiance", Spectrumf::D65());
        // Don't know the power yet
        m_power = Spectrumf(0.0f);
    }

    // AreaLight(Stream *stream, InstanceManager *manager)
    //  : Emitter(stream, manager) {
    //     m_intensity = Spectrumf(stream);
    // }

    // void serialize(Stream *stream, InstanceManager *manager) const override {
    //     Emitter::serialize(stream, manager);
    //     m_intensity.serialize(stream);
    // }

    Spectrumf eval(const SurfaceInteraction3f &its,
                   const Vector3f &d) const override {
        if (dot(its.sh_frame.n, d) <= 0)
            return Spectrumf(0.0f);
        else
            return m_radiance;
    }
    SpectrumfP eval(const SurfaceInteraction3fP &its,
                    const Vector3fP &d) const override {
        SpectrumfP res(m_radiance);
        res[dot(its.sh_frame.n, d) <= 0.0f] = 0.0f;
        return res;
    }

    Spectrumf sample_position(
        PositionSample3f &p_rec, const Point2f &sample,
        const Point2f * /*extra*/) const override {
        m_shape->sample_position(p_rec, sample);
        return m_power;
    }
    SpectrumfP sample_position(
        PositionSample3fP &p_rec, const Point2fP &sample,
        const Point2fP * /*extra*/) const override {
        m_shape->sample_position(p_rec, sample);
        return SpectrumfP(m_power);
    }

    Spectrumf eval_position(
            const PositionSample3f &/*p_rec*/) const override {
        return m_radiance * math::Pi;
    }
    SpectrumfP eval_position(
            const PositionSample3fP &/*p_rec*/) const override {
        return SpectrumfP(m_radiance * math::Pi);
    }

    Float pdf_position(const PositionSample3f &p_rec) const override {
        return m_shape->pdf_position(p_rec);
    }
    FloatP pdf_position(const PositionSample3fP &p_rec) const override {
        return m_shape->pdf_position(p_rec);
    }

    template <typename DirectionSample, typename PositionSample,
              typename Point2 = typename DirectionSample::Point2>
    auto sample_direction_impl(DirectionSample &d_rec, PositionSample &p_rec,
                               const Point2 &sample) const {
        using Vector3 = vector3_t<Point2>;
        using Frame = Frame<Vector3>;
        using Spectrum = Spectrum<value_t<Point2>>;

        Vector3 local = warp::square_to_cosine_hemisphere(sample);
        d_rec.d = Frame(p_rec.n).to_world(local);
        d_rec.pdf = warp::square_to_cosine_hemisphere_pdf(local);
        d_rec.measure = ESolidAngle;
        return Spectrum(1.0f);
    }
    Spectrumf sample_direction(
            DirectionSample3f &d_rec, PositionSample3f &p_rec,
            const Point2f &sample, const Point2f * /*extra*/) const override {
        return sample_direction_impl(d_rec, p_rec, sample);
    }
    SpectrumfP sample_direction(
            DirectionSample3fP &d_rec, PositionSample3fP &p_rec,
            const Point2fP &sample, const Point2fP * /*extra*/) const override {
        return sample_direction_impl(d_rec, p_rec, sample);
    }

    template <typename DirectionSample, typename PositionSample>
    auto pdf_direction_impl(DirectionSample &d_rec,
                            const PositionSample &p_rec) const {
        auto dp = dot(d_rec.d, p_rec.n);
        masked(dp, (neq(d_rec.measure, ESolidAngle)) | (dp < 0)) = 0.0f;
        return math::InvPi * dp;
    }
    Float pdf_direction(const DirectionSample3f &d_rec,
                        const PositionSample3f &p_rec) const override {
        return pdf_direction_impl(d_rec, p_rec);
    }
    FloatP pdf_direction(const DirectionSample3fP &d_rec,
            const PositionSample3fP &p_rec) const override {
        return pdf_direction_impl(d_rec, p_rec);
    }


    Spectrumf eval_direction(const DirectionSample3f &d_rec,
                             const PositionSample3f &p_rec) const override {
        return Spectrumf(pdf_direction_impl(d_rec, p_rec));
    }
    SpectrumfP eval_direction(const DirectionSample3fP &d_rec,
                              const PositionSample3fP &p_rec) const override {
        return SpectrumfP(pdf_direction_impl(d_rec, p_rec));
    }

    template <typename Point2, typename Value = value_t<Point2>>
    auto sample_ray_impl(const Point2 &position_sample,
                         const Point2 &direction_sample,
                         Value time_sample) const {
        using Point3 = Point<Value, 3>;
        using Frame = Frame<vector3_t<Point3>>;
        using PositionSample = PositionSample<Point3>;
        using Ray3 = Ray<Point3>;
        using Spectrum = Spectrum<Value>;

        PositionSample p_rec(time_sample);
        m_shape->sample_position(p_rec, position_sample);
        auto local = warp::square_to_cosine_hemisphere(direction_sample);
        Ray3 ray;
        ray.time = time_sample;
        ray.o = p_rec.p;
        ray.d = Frame(p_rec.n).to_world(local);
        return std::make_pair(ray, Spectrum(m_power));
    }
    std::pair<Ray3f, Spectrumf> sample_ray(
            const Point2f &position_sample, const Point2f &direction_sample,
            Float time_sample) const override {
        return sample_ray_impl(position_sample, direction_sample, time_sample);
    }
    std::pair<Ray3fP, SpectrumfP> sample_ray(
            const Point2fP &position_sample, const Point2fP &direction_sample,
            FloatP time_sample) const override {
        return sample_ray_impl(position_sample, direction_sample, time_sample);
    }

    template <typename DirectSample,
              typename Point2 = typename DirectSample::Point2,
              typename Value = value_t<Point2>,
              typename Spectrum = Spectrum<Value>>
    Spectrum sample_direct_impl(DirectSample &d_rec,
                                const Point2 &sample) const {
        m_shape->sample_direct(d_rec, sample);
        // Check that the emitter and reference position are oriented correctly
        // with respect to each other. Note that the >= 0 check
        // for `ref_n` is intentional -- those sampling requests that specify
        // a reference point within a medium or on a transmissive surface
        // will set `d_rec.ref_n = 0`, hence they should always be accepted.
        Spectrum res(0.0f);
        auto mask = (dot(d_rec.d, d_rec.ref_n) >= 0)
                  & (dot(d_rec.d, d_rec.n) < 0)
                  & (d_rec.pdf != 0);
        masked(res, mask) = Spectrum(m_radiance) / d_rec.pdf;
        masked(d_rec.pdf, ~mask) = 0.0f;
        return res;
    }
    Spectrumf sample_direct(
        DirectSample3f &d_rec, const Point2f &sample) const override {
        return sample_direct_impl(d_rec, sample);
    }
    SpectrumfP sample_direct(
        DirectSample3fP &d_rec, const Point2fP &sample) const override {
        return sample_direct_impl(d_rec, sample);
    }

    Float pdf_direct(const DirectSample3f &d_rec) const override {
        // Check that the emitter and receiver are oriented correctly with
        // respect to each other.
        if (dot(d_rec.d, d_rec.ref_n) >= 0 && dot(d_rec.d, d_rec.n) < 0)
            return m_shape->pdf_direct(d_rec);
        else
            return 0.0f;
    }
    FloatP pdf_direct(const DirectSample3fP &d_rec) const override {
        FloatP res(0.0f);
        res[(dot(d_rec.d, d_rec.ref_n) >= 0)
            & (dot(d_rec.d, d_rec.n) < 0)] = m_shape->pdf_direct(d_rec);
        return res;
    }

    BoundingBox3f bbox() const override {
        return m_shape->bbox();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "AreaLight[" << std::endl
            << "  radiance = " << m_radiance << "," << std::endl
            << "  surface_area = ";
        if (m_shape) oss << m_shape->surface_area();
        else         oss << "<no shape attached!>";
        oss << "," << std::endl;
        if (m_medium) oss << string::indent(m_medium->to_string());
        else         oss << "<no medium attached!>";
        oss << "," << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    Spectrumf m_radiance;
    Spectrumf m_power;
};


MTS_IMPLEMENT_CLASS(AreaLight, Emitter)
MTS_EXPORT_PLUGIN(AreaLight, "Point emitter");
NAMESPACE_END(mitsuba)
