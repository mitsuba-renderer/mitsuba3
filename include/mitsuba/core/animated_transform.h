#pragma once

#if defined(__GNUG__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdouble-promotion"
#elif defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdouble-promotion"
#endif

#include <mitsuba/core/object.h>
#include <mitsuba/core/transform.h>
#include <drjit/quaternion.h>
#include <drjit/transform.h>
#include <mitsuba/core/hash.h>
#include <mitsuba/core/math.h>
#include <map>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Animated transformation
 *
 * This struct stores a sequence of transformations and interpolates between them
 * using a combination of linear interpolation (for translation and scaling)
 * and spherical linear interpolation (for rotation).
 */
template <typename Float>
struct AnimatedTransform : public Object {
public:
    MI_IMPORT_CORE_TYPES()

    using Transform = mitsuba::Transform<Point4f, true>;
    using Matrix    = typename Transform::Matrix;

    using ScalarTransform = mitsuba::Transform<Point<ScalarFloat, 4>, true>;
    using ScalarMatrix    = typename ScalarTransform::Matrix;
    using FloatStorage = DynamicBuffer<Float>;

    struct Keyframe {
        ScalarVector3f S;
        ScalarQuaternion4f Q;
        ScalarVector3f T;

        std::string to_string() const {
            std::ostringstream oss;
            oss << "Keyframe[S=" << S << ", Q=" << Q << ", T=" << T << "]";
            return oss.str();
        }
    };

    /// Create an empty animated transformation
    AnimatedTransform() = default;

    /// Initialize from a constant transformation
    template <typename T, std::enable_if_t<std::is_same_v<T, Transform> ||
                                           std::is_same_v<T, ScalarTransform>, int> = 0>
    AnimatedTransform(const T &trafo) {
        if constexpr (std::is_same_v<T, ScalarTransform>) {
            add_keyframe(0.f, trafo);
        } else {
            // If it's a JIT transform, we must convert it to scalar first for host-side storage
            if constexpr (dr::is_jit_v<Float>) {
                ScalarTransform scalar_trafo;
                for (size_t i = 0; i < 4; ++i)
                    for (size_t j = 0; j < 4; ++j)
                        scalar_trafo.matrix(i, j) = dr::slice(trafo.matrix(i, j));
                add_keyframe(0.f, scalar_trafo);
            } else {
                add_keyframe(0.f, (ScalarTransform) trafo);
            }
        }
    }

    /// Conversion constructor from another AnimatedTransform variant
    template <typename Float2>
    AnimatedTransform(const AnimatedTransform<Float2> &other) {
        for (auto const& [time, kf] : other.keyframes()) {
            m_keyframes[time] = kf;
        }
        m_need_flatten = true;
    }

    /**
     * \brief Add a keyframe to the animated transformation
     *
     * This method decomposes the transformation and stores it in the keyframe list.
     * Note that this is a host-side operation and should not be used in JIT-compiled kernels.
     */
    void add_keyframe(ScalarFloat time, const ScalarTransform &trafo) {
        auto [S, Q, T] = dr::transform_decompose(trafo.matrix);

        if (dr::abs(S[0][1]) > 1e-6f || dr::abs(S[0][2]) > 1e-6f || dr::abs(S[1][0]) > 1e-6f ||
            dr::abs(S[1][2]) > 1e-6f || dr::abs(S[2][0]) > 1e-6f || dr::abs(S[2][1]) > 1e-6f)
            Throw("AnimatedTransform: Transformation contains shear, which is not supported!");

        m_keyframes[time] = {dr::diag(S), Q, T};
        m_need_flatten = true;
    }

    /**
     * \brief Evaluate the transformation at a specific time
     *
     * This method performs a vectorized interpolation between keyframes.
     */
    Transform eval(Float time) const {
        if (m_need_flatten)
            const_cast<AnimatedTransform*>(this)->flatten();

        size_t n_keyframes = m_times.size();
        size_t width = dr::width(time);

        if (n_keyframes == 1) {
            // Recompose from single keyframe
            UInt32 index = dr::zeros<UInt32>(width);
            return Transform(dr::transform_compose<Matrix>(
                dr::diag(dr::gather<Vector3f>(m_scales, index)),
                dr::gather<Quaternion4f>(m_rotations, index),
                dr::gather<Vector3f>(m_translations, index)
            ));
        } else if (n_keyframes == 2) {
            // Fast path for 2 keyframes
            UInt32 i0 = dr::zeros<UInt32>(width);
            UInt32 i1 = dr::full<UInt32>(1, width);

            Float t0 = dr::gather<Float>(m_times, i0);
            Float t1 = dr::gather<Float>(m_times, i1);
            Float t = dr::clip((time - t0) / (t1 - t0), 0.f, 1.f);

            Vector3f s0 = dr::gather<Vector3f>(m_scales, i0);
            Vector3f s1 = dr::gather<Vector3f>(m_scales, i1);
            Quaternion4f q0 = dr::gather<Quaternion4f>(m_rotations, i0);
            Quaternion4f q1 = dr::gather<Quaternion4f>(m_rotations, i1);
            Vector3f tr0 = dr::gather<Vector3f>(m_translations, i0);
            Vector3f tr1 = dr::gather<Vector3f>(m_translations, i1);

            return Transform(dr::transform_compose<Matrix>(
                dr::diag(dr::lerp(s0, s1, t)),
                dr::slerp(q0, q1, t),
                dr::lerp(tr0, tr1, t)
            ));
        } else {
            // General case with binary search
            auto pred = [&](UInt32 index) {
                return dr::gather<Float>(m_times, index) <= time;
            };

            UInt32 index = math::find_interval<UInt32>((uint32_t) n_keyframes, pred);

            Float t0 = dr::gather<Float>(m_times, index);
            Float t1 = dr::gather<Float>(m_times, index + 1);
            Float t = dr::clip((time - t0) / (t1 - t0), 0.f, 1.f);

            Vector3f s0 = dr::gather<Vector3f>(m_scales, index);
            Vector3f s1 = dr::gather<Vector3f>(m_scales, index + 1);
            Quaternion4f q0 = dr::gather<Quaternion4f>(m_rotations, index);
            Quaternion4f q1 = dr::gather<Quaternion4f>(m_rotations, index + 1);
            Vector3f tr0 = dr::gather<Vector3f>(m_translations, index);
            Vector3f tr1 = dr::gather<Vector3f>(m_translations, index + 1);

            return Transform(dr::transform_compose<Matrix>(
                dr::diag(dr::lerp(s0, s1, t)),
                dr::slerp(q0, q1, t),
                dr::lerp(tr0, tr1, t)
            ));
        }
    }

    /**
     * \brief Scalar evaluation of the transformation
     *
     * This version is for use on the host (e.g., during AABB construction).
     */
    Transform eval_scalar(ScalarFloat time) const {
        if (m_keyframes.empty())
            Throw("Animated transform requires at least one keyframe, found 0.");

        if (m_keyframes.size() == 1) {
            const auto &kf = m_keyframes.begin()->second;
            return Transform(dr::transform_compose<Matrix>(
                dr::diag(kf.S), kf.Q, kf.T
            ));
        }

        auto it1 = m_keyframes.lower_bound(time);
        auto it0 = it1;

        if (it1 == m_keyframes.begin()) {
            it0 = it1;
        } else if (it1 == m_keyframes.end()) {
            it1 = std::prev(it1);
            it0 = it1;
        } else {
            it0 = std::prev(it1);
        }

        if (it0 == it1) {
            const auto &kf = it0->second;
            return Transform(dr::transform_compose<Matrix>(
                dr::diag(kf.S), kf.Q, kf.T
            ));
        }

        ScalarFloat t = std::clamp((time - it0->first) / (it1->first - it0->first),
                                   ScalarFloat(0), ScalarFloat(1));
        const auto &kf0 = it0->second;
        const auto &kf1 = it1->second;

        return Transform(dr::transform_compose<Matrix>(
            dr::diag(dr::lerp(kf0.S, kf1.S, t)),
            dr::slerp(kf0.Q, kf1.Q, t),
            dr::lerp(kf0.T, kf1.T, t)
        ));
    }

    /// Check if the transformation is animated
    bool is_animated() const {
        return m_keyframes.size() > 1;
    }

    /// Equality comparison operator
    bool operator==(const AnimatedTransform &other) const {
        if (m_keyframes.size() != other.m_keyframes.size())
            return false;

        auto it1 = m_keyframes.begin();
        auto it2 = other.m_keyframes.begin();
        for (; it1 != m_keyframes.end(); ++it1, ++it2) {
            if (it1->first != it2->first)
                return false;
            const auto &kf1 = it1->second;
            const auto &kf2 = it2->second;
            if (dr::any_nested(kf1.S != kf2.S) ||
                dr::any_nested(kf1.Q != kf2.Q) ||
                dr::any_nested(kf1.T != kf2.T))
                return false;
        }
        return true;
    }

    /// Inequality comparison operator
    bool operator!=(const AnimatedTransform &other) const {
        return !operator==(other);
    }

    // Internal methods needed for hashing and bindings
    const std::map<ScalarFloat, Keyframe>& keyframes() const { return m_keyframes; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << class_name() << "[";
        if (!m_keyframes.empty()) {
            oss << std::endl;
        }
        for (auto const& [time, kf] : m_keyframes) {
            oss << "  " << time << ": " << kf.to_string() << "," << std::endl;
        }
        oss << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(AnimatedTransform)

private:

    void flatten() {
        if (m_keyframes.empty()) {
            Throw("Animated transform requires at least one keyframe, found 0.");
        }

        size_t n = m_keyframes.size();
        m_times = dr::zeros<FloatStorage>(n);
        m_scales = dr::zeros<FloatStorage>(3 * n);
        m_translations = dr::zeros<FloatStorage>(3 * n);
        m_rotations = dr::zeros<FloatStorage>(4 * n);

        size_t i = 0;
        for (auto const& [time, kf] : m_keyframes) {
            if constexpr (dr::is_jit_v<Float>) {
                dr::scatter(m_times, time, i);
                for (size_t j = 0; j < 3; ++j) {
                    dr::scatter(m_scales, kf.S[j], i * 3 + j);
                    dr::scatter(m_translations, kf.T[j], i * 3 + j);
                }
                for (size_t j = 0; j < 4; ++j) {
                    dr::scatter(m_rotations, kf.Q[j], i * 4 + j);
                }
            } else {
                dr::store(m_times.data() + i, time);
                dr::store(m_scales.data() + 3 * i, kf.S);
                dr::store(m_translations.data() + 3 * i, kf.T);
                dr::store(m_rotations.data() + 4 * i, kf.Q);
            }
            i++;
        }
        m_need_flatten = false;
    }

    std::map<ScalarFloat, Keyframe> m_keyframes;
    DynamicBuffer<Float> m_times;
    DynamicBuffer<Vector3f> m_scales;
    DynamicBuffer<Quaternion4f> m_rotations;
    DynamicBuffer<Vector3f> m_translations;
    bool m_need_flatten = true;
};


#if defined(__GNUG__)
#  pragma GCC diagnostic pop
#elif defined(__clang__)
#  pragma clang diagnostic pop
#endif

NAMESPACE_END(mitsuba)
