#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/mueller.h>

NAMESPACE_BEGIN(mitsuba)

Endpoint::Endpoint(const Properties &props) {
    m_world_transform =
        props.animated_transform("to_world", Transform4f()).get();
    m_monochrome = props.bool_("monochrome");
}

Endpoint::~Endpoint() { }

ref<Shape> Endpoint::create_shape(const Scene *) {
    return nullptr;
}

void Endpoint::set_shape(Shape * shape) {
    m_shape = shape;
}

void Endpoint::set_medium(Medium *medium) {
    m_medium = medium;
}

std::pair<Ray3f, Spectrumf>
Endpoint::sample_ray(Float /* time */, Float /* sample1 */,
                     const Point2f &/* sample2 */,
                     const Point2f &/* sample3 */) const {
    NotImplementedError("sample_ray");
}

std::pair<Ray3fP, SpectrumfP>
Endpoint::sample_ray(FloatP /* time */, FloatP /* sample1 */,
                     const Point2fP &/* sample2 */,
                     const Point2fP &/* sample3 */,
                     MaskP /* active */) const {
    NotImplementedError("sample_ray_p");
}

#if defined(MTS_ENABLE_AUTODIFF)
std::pair<Ray3fD, SpectrumfD>
Endpoint::sample_ray(FloatD /* time */, FloatD /* sample1 */,
                     const Point2fD &/* sample2 */,
                     const Point2fD &/* sample3 */,
                     MaskD /* active */) const {
    NotImplementedError("sample_ray_d");
}
#endif

std::pair<Ray3f, MuellerMatrixSf>
Endpoint::sample_ray_pol(Float time, Float sample1,
                         const Point2f &sample2,
                         const Point2f &sample3) const {
    auto [ray, spec] = sample_ray(time, sample1, sample2, sample3);
    return std::make_pair(ray, mueller::depolarizer(spec));
}

std::pair<Ray3fP, MuellerMatrixSfP>
Endpoint::sample_ray_pol(FloatP time, FloatP sample1,
                         const Point2fP &sample2,
                         const Point2fP &sample3,
                         MaskP active) const {
    auto [ray, spec] = sample_ray(time, sample1, sample2, sample3, active);
    return std::make_pair(ray, mueller::depolarizer(spec));
}

#if defined(MTS_ENABLE_AUTODIFF)
std::pair<Ray3fD, MuellerMatrixSfD>
Endpoint::sample_ray_pol(FloatD time, FloatD sample1,
                         const Point2fD &sample2,
                         const Point2fD &sample3,
                         MaskD active) const {
    auto [ray, spec] = sample_ray(time, sample1, sample2, sample3, active);
    return std::make_pair(ray, mueller::depolarizer(spec));
}
#endif

std::pair<DirectionSample3f, Spectrumf>
Endpoint::sample_direction(const Interaction3f &/* it */,
                           const Point2f &/* sample */) const {
    NotImplementedError("sample_direction");
}

std::pair<DirectionSample3fP, SpectrumfP>
Endpoint::sample_direction(const Interaction3fP &/* it */,
                           const Point2fP &/* sample */,
                           MaskP /* active */) const {
    NotImplementedError("sample_direction_p");
}

#if defined(MTS_ENABLE_AUTODIFF)
std::pair<DirectionSample3fD, SpectrumfD>
Endpoint::sample_direction(const Interaction3fD &/* it */,
                           const Point2fD &/* sample */,
                           MaskD /* active */) const {
    NotImplementedError("sample_direction_d");
}
#endif

std::pair<DirectionSample3f, MuellerMatrixSf>
Endpoint::sample_direction_pol(const Interaction3f &it,
                               const Point2f &sample) const {
    auto [s, f] = sample_direction(it, sample);
    return std::make_pair(s, mueller::depolarizer(f));
}

std::pair<DirectionSample3fP, MuellerMatrixSfP>
Endpoint::sample_direction_pol(const Interaction3fP &it,
                               const Point2fP &sample,
                               MaskP active) const {
    auto [s, f] = sample_direction(it, sample, active);
    return std::make_pair(s, mueller::depolarizer(f));
}

#if defined(MTS_ENABLE_AUTODIFF)
std::pair<DirectionSample3fD, MuellerMatrixSfD>
Endpoint::sample_direction_pol(const Interaction3fD &it,
                               const Point2fD &sample,
                               MaskD active) const {
    auto [s, f] = sample_direction(it, sample, active);
    return std::make_pair(s, mueller::depolarizer(f));
}
#endif

Float Endpoint::pdf_direction(const Interaction3f &/* it */,
                           const DirectionSample3f &/* ds */) const {
    NotImplementedError("pdf_direction");
}

FloatP Endpoint::pdf_direction(const Interaction3fP &/* it */,
                            const DirectionSample3fP &/* ds */,
                            MaskP /*active */) const {
    NotImplementedError("pdf_direction_p");
}

#if defined(MTS_ENABLE_AUTODIFF)
FloatD Endpoint::pdf_direction(const Interaction3fD &/* it */,
                            const DirectionSample3fD &/* ds */,
                            MaskD /* active */) const {
    NotImplementedError("pdf_direction_d");
}
#endif

Spectrumf Endpoint::eval(const SurfaceInteraction3f &/* si */) const {
    NotImplementedError("eval");
}

SpectrumfP Endpoint::eval(const SurfaceInteraction3fP &/* si */,
                          MaskP /* active */) const {
    NotImplementedError("eval_p");
}

#if defined(MTS_ENABLE_AUTODIFF)
SpectrumfD Endpoint::eval(const SurfaceInteraction3fD &/* si */,
                          MaskD /* active */) const {
    NotImplementedError("eval_d");
}
#endif

MuellerMatrixSf Endpoint::eval_pol(const SurfaceInteraction3f &si) const {
    return mueller::depolarizer(eval(si));
}

MuellerMatrixSfP Endpoint::eval_pol(const SurfaceInteraction3fP &si,
                                    MaskP active) const {
    return mueller::depolarizer(eval(si, active));
}

#if defined(MTS_ENABLE_AUTODIFF)
MuellerMatrixSfD Endpoint::eval_pol(const SurfaceInteraction3fD &si,
                                    MaskD active) const {
    return mueller::depolarizer(eval(si, active));
}
#endif

MTS_IMPLEMENT_CLASS(Endpoint, Object)

NAMESPACE_END(mitsuba)
