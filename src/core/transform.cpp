#include <mitsuba/core/transform.h>
#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

// WARNING: the AnimatedTransform class is outdated and dysfunctional with the
// latest version of Mitsuba 3. Please update this code before using it!
#if 0

AnimatedTransform::~AnimatedTransform() { }

void AnimatedTransform::append(const Keyframe &keyframe) {
    if (!m_keyframes.empty() && keyframe.time <= m_keyframes.back().time)
        Throw("AnimatedTransform::append(): time values must be "
              "strictly monotonically increasing!");

    if (m_keyframes.empty())
        m_transform = Transform4f(dr::transform_compose<Matrix4f>(
            keyframe.scale, keyframe.quat, keyframe.trans));

    m_keyframes.push_back(keyframe);
}

void AnimatedTransform::append(Float time, const Transform4f &trafo) {
    if (!m_keyframes.empty() && time <= m_keyframes.back().time)
        Throw("AnimatedTransform::append(): time values must be "
              "strictly monotonically increasing!");

    /* Perform a polar decomposition into a 3x3 scale/shear matrix,
       a rotation quaternion, and a translation vector. These will
       all be interpolated independently. */
    auto [M, Q, T] = dr::transform_decompose(trafo.matrix);

    if (m_keyframes.empty())
        m_transform = trafo;

    m_keyframes.push_back(Keyframe { time, M, Q, T });
}

bool AnimatedTransform::has_scale() const {
    if (m_keyframes.empty())
        return false;

    Matrix3f delta = dr::zeros<Matrix3f>();
    for (auto const &k: m_keyframes)
        delta += dr::abs(k.scale - dr::identity<Matrix3f>());
    return dr::sum_nested(delta) / m_keyframes.size() > 1e-3f;
}

typename AnimatedTransform::BoundingBox3f AnimatedTransform::translation_bounds() const {
    if (m_keyframes.empty()) {
        auto p = m_transform * Point3f(0.f);
        return BoundingBox3f(p, p);
    }
    Throw("AnimatedTransform::translation_bounds() not implemented for"
          " non-constant animation.");
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

MI_IMPLEMENT_CLASS(AnimatedTransform, Object)
#endif

NAMESPACE_END(mitsuba)
