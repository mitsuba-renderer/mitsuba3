#include <mitsuba/core/transform.h>
#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

AnimatedTransform::~AnimatedTransform() { }

void AnimatedTransform::append(const Keyframe &keyframe) {
    if (!m_keyframes.empty() && keyframe.time <= m_keyframes.back().time)
        Throw("AnimatedTransform::append(): time values must be "
              "strictly monotonically increasing!");

    if (m_keyframes.empty())
        m_transform = Transform4f(
            enoki::transform_compose(keyframe.scale, keyframe.quat, keyframe.trans),
            enoki::transform_compose_inverse(keyframe.scale, keyframe.quat, keyframe.trans));

    m_keyframes.push_back(keyframe);
}

void AnimatedTransform::append(Float time, const Transform4f &trafo) {
    if (!m_keyframes.empty() && time <= m_keyframes.back().time)
        Throw("AnimatedTransform::append(): time values must be "
              "strictly monotonically increasing!");

    /* Perform a polar decomposition into a 3x3 scale/shear matrix,
       a rotation quaternion, and a translation vector. These will
       all be interpolated independently. */
    auto [M, Q, T] = enoki::transform_decompose(trafo.matrix);

    if (m_keyframes.empty())
        m_transform = trafo;

    m_keyframes.push_back(Keyframe { time, M, Q, T });
}

bool AnimatedTransform::has_scale() const {
    if (m_keyframes.empty())
        return false;

    Matrix3f delta = zero<Matrix3f>();
    for (auto const &k: m_keyframes)
        delta += abs(k.scale - enoki::identity<Matrix3f>());
    return hsum_nested(delta) / m_keyframes.size() > 1e-3f;
}

#if 0
typename AnimatedTransform::Transform4f
AnimatedTransform::eval(Float time, Mask active) const {

    /* Compute constants describing the layout of the 'Keyframe' data structure */
    constexpr size_t Stride      = sizeof(Keyframe);
    constexpr size_t ScaleOffset = offsetof(Keyframe, scale) / sizeof(Float);
    constexpr size_t QuatOffset  = offsetof(Keyframe, quat)  / sizeof(Float);
    constexpr size_t TransOffset = offsetof(Keyframe, trans) / sizeof(Float);

    using Index = uint32_array_t<Float>;

    /* Perhaps the transformation isn't animated */
    if (likely(size() <= 1))
        return m_transform;

    /* Look up the interval containing 'time' */
    Index idx0 = math::find_interval(
        (uint32_t) size(),
        [&](Index idx, Mask active) {
            constexpr size_t Stride_ = sizeof(Keyframe); // MSVC: have to redeclare constexpr variable in lambda scope :(
            return gather<Float, Stride_>(m_keyframes.data(), idx, active) <= time;
        },
        active);

    Index idx1 = idx0 + 1;

    /* Compute the relative time value in [0, 1] */
    Float t0 = gather<Float, Stride, false>(m_keyframes.data(), idx0, active),
          t1 = gather<Float, Stride, false>(m_keyframes.data(), idx1, active),
          t  = min(max((time - t0) / (t1 - t0), 0.f), 1.f);

    /* Interpolate the scale matrix */
    Matrix3f scale0 = gather<Matrix3f, Stride, false>((Float *) m_keyframes.data() + ScaleOffset, idx0, active),
             scale1 = gather<Matrix3f, Stride, false>((Float *) m_keyframes.data() + ScaleOffset, idx1, active),
             scale = scale0 * (1 - t) + scale1 * t;

    /* Interpolate the rotation quaternion */
    Quaternion4f quat0 = gather<Quaternion4f, Stride, false>((Float *) m_keyframes.data() + QuatOffset, idx0, active),
                 quat1 = gather<Quaternion4f, Stride, false>((Float *) m_keyframes.data() + QuatOffset, idx1, active),
                 quat = enoki::slerp(quat0, quat1, t);

    /* Interpolate the translation component */
    Vector3f trans0 = gather<Vector3f, Stride, false>((Float *) m_keyframes.data() + TransOffset, idx0, active),
             trans1 = gather<Vector3f, Stride, false>((Float *) m_keyframes.data() + TransOffset, idx1, active),
             trans = trans0 * (1 - t) + trans1 * t;

    return Transform4f(
        enoki::transform_compose(scale, quat, trans),
        enoki::transform_compose_inverse(scale, quat, trans)
    );
}
#endif

typename AnimatedTransform::BoundingBox3f AnimatedTransform::translation_bounds() const {
    if (m_keyframes.empty()) {
        auto p = m_transform * Point3f(0.f);
        return BoundingBox3f(p, p);
    }
    Throw("AnimatedTransform::translation_bounds() not implemented for"
          " non-constant animation.");
    return BoundingBox3f();
}

std::string AnimatedTransform::to_string() const {
    std::ostringstream oss;
    oss << "AnimatedTransform[" << std::endl
        << "  m_transform = " << string::indent(m_transform, 16) << "," << std::endl
        << "  m_keyframes = " << string::indent(m_keyframes, 16) << std::endl
        << "]";

    return oss.str();
}

std::ostream &operator<<(std::ostream &os, const AnimatedTransform::Keyframe &frame) {
    os << "Keyframe[" << std::endl
       << "  time = " << frame.time << "," << std::endl
       << "  scale = " << frame.scale << "," << std::endl
       << "  quat = " << frame.quat << "," << std::endl
       << "  trans = " << frame.trans
       << "]";
    return os;
}

std::ostream &operator<<(std::ostream &os, const AnimatedTransform &t) {
    os << t.to_string();
    return os;
}

MTS_IMPLEMENT_CLASS(AnimatedTransform, Object)

NAMESPACE_END(mitsuba)
