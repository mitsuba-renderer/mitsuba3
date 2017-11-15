#include <mitsuba/core/bbox.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

Shape::Shape(const Properties &props) {
    for (auto &kv : props.objects()) {
        auto emitter = dynamic_cast<Emitter *>(kv.second.get());
        auto bsdf = dynamic_cast<BSDF *>(kv.second.get());

        if (emitter) {
            if (m_emitter)
                Log(EError, "Only a single Emitter child object can be specified"
                            " per shape.");
            m_emitter = emitter;
            emitter->set_shape(this);
        } else if (bsdf) {
            if (m_bsdf)
                Log(EError, "Only a single BSDF child object can be specified"
                            " per shape.");
            set_bsdf(bsdf);
        } else
            Throw("Tried to add an unsupported object of type %s", kv.second);
    }
}

Shape::~Shape() { }

// =============================================================
//! @{ \name Sampling routines
// =============================================================

template <typename DirectSample, typename Point2, typename Mask>
void Shape::sample_direct_impl(DirectSample &d_rec, const Point2 &sample,
                               const Mask &active) const {
    using Value = typename DirectSample::Value;
    // Rely on the concrete shape's implementation of sample_position()
    sample_position(d_rec, sample, active);

    masked(d_rec.d, active)       = d_rec.p - d_rec.ref_p;

    Value dist_squared            = squared_norm(d_rec.d);
    masked(d_rec.dist, active)    = sqrt(dist_squared);
    masked(d_rec.d, active)      /= d_rec.dist;

    Value dp                      = abs_dot(d_rec.d, d_rec.n);
    masked(d_rec.pdf, active)    *= select(neq(dp, 0.0f), dist_squared / dp, 0.0f);
    masked(d_rec.measure, active) = ESolidAngle;
}

void Shape::sample_direct(DirectSample3f &d_rec, const Point2f &sample) const {
    return sample_direct_impl(d_rec, sample, true);
}

void Shape::sample_direct(DirectSample3fP &d_rec, const Point2fP &sample,
                          const mask_t<FloatP> &active) const {
    return sample_direct_impl(d_rec, sample, active);
}

template <typename DirectSample, typename Value, typename Mask>
Value Shape::pdf_direct_impl(const DirectSample &d_rec, const Mask &active) const {
    // Rely on the concrete shape's implementation of pdf_position()
    Value pdf = pdf_position(d_rec, active);

    auto mask = eq(d_rec.measure, ESolidAngle);
    masked(pdf, active & mask) *=
        (d_rec.dist * d_rec.dist) / abs_dot(d_rec.d, d_rec.n);

    mask = (~mask) & neq(d_rec.measure, EArea);
    masked(pdf, active & mask) = 0.0f;
    return pdf;
}

Float Shape::pdf_direct(const DirectSample3f &d_rec) const {
    return pdf_direct_impl(d_rec, true);
}

FloatP Shape::pdf_direct(const DirectSample3fP &d_rec,
                          const mask_t<FloatP> &active) const {
    return pdf_direct_impl(d_rec, active);
}

//! @}
// =============================================================

BoundingBox3f Shape::bbox(Index) const {
    return bbox();
}

BoundingBox3f Shape::bbox(Index index, const BoundingBox3f &clip) const {
    BoundingBox3f result = bbox(index);
    result.clip(clip);
    return result;
}

Shape::Size Shape::primitive_count() const {
    return 1;
}

void Shape::adjust_time(SurfaceInteraction3f &si, const Float &time) const {
    si.time = time;
}

void Shape::adjust_time(SurfaceInteraction3fP &si, const FloatP &time,
                        const mask_t<FloatP> &active) const {
    si.time[active] = time;
}

MTS_IMPLEMENT_CLASS(Shape, Object)
NAMESPACE_END(mitsuba)
