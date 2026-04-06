#include <mitsuba/core/animated_transform.h>
#include <mitsuba/core/config.h>
#include <sstream>

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdouble-promotion"
#endif

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT
AnimatedTransform<Float, Spectrum>::AnimatedTransform(
    const ScalarAffineTransform4f &trafo) {
    m_transform = AffineTransform4f(trafo);
    add_keyframe(ScalarFloat(0), trafo);
    initialize();
}

MI_VARIANT
AnimatedTransform<Float, Spectrum>::AnimatedTransform(
    const std::map<ScalarFloat, ScalarAffineTransform4f> &keyframes) {
    if (keyframes.size() == 1) {
        m_transform = AffineTransform4f(keyframes.begin()->second);
    }
    for (const auto &[time, trafo] : keyframes) {
        add_keyframe(time, trafo);
    }
    initialize();
}

MI_VARIANT auto AnimatedTransform<Float, Spectrum>::eval(Float time) const
    -> AffineTransform4f {
    size_t n_keyframes = m_times.size();
    if (n_keyframes == 0)
        Throw("Animated transform requires at least one keyframe, found 0.");

    if (n_keyframes == 1) {
        return m_transform.value();
    } else if (n_keyframes == 2) {
        // Fast path for 2 keyframes
        UInt32 i0 = 0, i1 = 1;
        Float t0        = dr::gather<Float>(m_times, i0);
        Float t1        = dr::gather<Float>(m_times, i1);
        Float t         = dr::clip((time - t0) / (t1 - t0), 0.f, 1.f);
        Vector3f s0     = dr::gather<Vector3f>(m_scales, i0);
        Vector3f s1     = dr::gather<Vector3f>(m_scales, i1);
        Quaternion4f q0 = dr::gather<Quaternion4f>(m_rotations, i0);
        Quaternion4f q1 = dr::gather<Quaternion4f>(m_rotations, i1);
        Vector3f tr0    = dr::gather<Vector3f>(m_translations, i0);
        Vector3f tr1    = dr::gather<Vector3f>(m_translations, i1);

        Matrix3f s     = dr::diag(dr::lerp(s0, s1, t));
        Quaternion4f q = dr::slerp(q0, q1, t);
        Vector3f tr    = dr::lerp(tr0, tr1, t);
        return AffineTransform4f(
            dr::transform_compose<Matrix4f>(s, q, tr),
            dr::transpose(dr::transform_compose_inverse<Matrix4f>(s, q, tr)));
    } else {
        // General case with binary search
        auto pred = [&](UInt32 index) {
            return dr::gather<Float>(m_times, index) <= time;
        };

        UInt32 index =
            math::find_interval<UInt32>((uint32_t) n_keyframes, pred);

        Float t0        = dr::gather<Float>(m_times, index);
        Float t1        = dr::gather<Float>(m_times, index + 1);
        Float t         = dr::clip((time - t0) / (t1 - t0), 0.f, 1.f);
        Vector3f s0     = dr::gather<Vector3f>(m_scales, index);
        Vector3f s1     = dr::gather<Vector3f>(m_scales, index + 1);
        Quaternion4f q0 = dr::gather<Quaternion4f>(m_rotations, index);
        Quaternion4f q1 = dr::gather<Quaternion4f>(m_rotations, index + 1);
        Vector3f tr0    = dr::gather<Vector3f>(m_translations, index);
        Vector3f tr1    = dr::gather<Vector3f>(m_translations, index + 1);

        Matrix3f s     = dr::diag(dr::lerp(s0, s1, t));
        Quaternion4f q = dr::slerp(q0, q1, t);
        Vector3f tr    = dr::lerp(tr0, tr1, t);
        return AffineTransform4f(
            dr::transform_compose<Matrix4f>(s, q, tr),
            dr::transpose(dr::transform_compose_inverse<Matrix4f>(s, q, tr)));
    }
}

MI_VARIANT auto
AnimatedTransform<Float, Spectrum>::eval_scalar(ScalarFloat time) const
    -> ScalarAffineTransform4f {
    if (m_keyframes.empty())
        Throw("Animated transform requires at least one keyframe, found 0.");

    if (m_keyframes.size() == 1) {
        return m_transform.scalar();
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
        const auto &kf   = it0->second;
        ScalarMatrix3f s = dr::diag(kf.S);
        return ScalarAffineTransform4f(
            dr::transform_compose<ScalarMatrix4f>(s, kf.Q, kf.T),
            dr::transpose(
                dr::transform_compose_inverse<ScalarMatrix4f>(s, kf.Q, kf.T)));
    }
    ScalarFloat t = std::clamp((time - it0->first) / (it1->first - it0->first),
                               ScalarFloat(0), ScalarFloat(1));
    const auto &kf0      = it0->second;
    const auto &kf1      = it1->second;
    ScalarMatrix3f s     = dr::diag(dr::lerp(kf0.S, kf1.S, t));
    ScalarQuaternion4f q = dr::slerp(kf0.Q, kf1.Q, t);
    ScalarVector3f tr    = dr::lerp(kf0.T, kf1.T, t);
    return ScalarAffineTransform4f(
        dr::transform_compose<ScalarMatrix4f>(s, q, tr),
        dr::transpose(dr::transform_compose_inverse<ScalarMatrix4f>(s, q, tr)));
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::parameters_changed(
    const std::vector<std::string> &keys) {
    size_t n = dr::width(m_times);
    if (dr::width(m_scales) != 3 * n || dr::width(m_translations) != 3 * n ||
        dr::width(m_rotations) != 4 * n) {
        Throw("Size of scales, translations, or rotations does not match the "
              "number of keyframes.");
    }

    if (n == 1) {
        if (m_keyframes.size() > 1) {
            // Transition from N > 1 to N == 1: update from arrays
            dr::eval(m_times, m_scales, m_translations, m_rotations);
            Keyframe kf;
            kf.S.x() = dr::slice(m_scales, 0);
            kf.S.y() = dr::slice(m_scales, 1);
            kf.S.z() = dr::slice(m_scales, 2);
            kf.Q.w() = dr::slice(m_rotations, 0);
            kf.Q.x() = dr::slice(m_rotations, 1);
            kf.Q.y() = dr::slice(m_rotations, 2);
            kf.Q.z() = dr::slice(m_rotations, 3);
            kf.T.x() = dr::slice(m_translations, 0);
            kf.T.y() = dr::slice(m_translations, 1);
            kf.T.z() = dr::slice(m_translations, 2);

            m_keyframes.clear();
            m_keyframes[dr::slice(m_times, 0)] = kf;

            // Rebuild the single transform
            ScalarMatrix3f s = dr::diag(kf.S);
            ScalarMatrix4f mat =
                dr::transform_compose<ScalarMatrix4f>(s, kf.Q, kf.T);
            ScalarMatrix4f inv = dr::transpose(
                dr::transform_compose_inverse<ScalarMatrix4f>(s, kf.Q, kf.T));
            m_transform = ScalarAffineTransform4f(mat, inv);
            dr::make_opaque(m_transform);
        } else {
            // Was already size 1, check if "transform" changed
            if (keys.empty() || string::contains(keys, "transform")) {
                m_transform = m_transform.value().update();
                dr::make_opaque(m_transform);

                // Also update the single keyframe in the map
                auto it                       = m_keyframes.begin();
                ScalarAffineTransform4f trafo = m_transform.scalar();
                auto [S, Q, T] = dr::transform_decompose(trafo.matrix);
                it->second     = { dr::diag(S), Q, T };
            }
        }
    } else if (n > 1) {
        // Read back all data from device to keep host map in sync
        dr::eval(m_times, m_scales, m_translations, m_rotations);
        std::map<ScalarFloat, Keyframe> new_keyframes;
        for (size_t i = 0; i < n; ++i) {
            Keyframe kf;
            kf.S.x() = dr::slice(m_scales, 3 * i + 0);
            kf.S.y() = dr::slice(m_scales, 3 * i + 1);
            kf.S.z() = dr::slice(m_scales, 3 * i + 2);
            kf.Q.w() = dr::slice(m_rotations, 4 * i + 0);
            kf.Q.x() = dr::slice(m_rotations, 4 * i + 1);
            kf.Q.y() = dr::slice(m_rotations, 4 * i + 2);
            kf.Q.z() = dr::slice(m_rotations, 4 * i + 3);
            kf.T.x() = dr::slice(m_translations, 3 * i + 0);
            kf.T.y() = dr::slice(m_translations, 3 * i + 1);
            kf.T.z() = dr::slice(m_translations, 3 * i + 2);
            new_keyframes[dr::slice(m_times, i)] = kf;
        }
        m_keyframes = std::move(new_keyframes);
    }
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::add_keyframe(
    ScalarFloat time, const ScalarAffineTransform4f &trafo) {
    auto [S, Q, T] = dr::transform_decompose(trafo.matrix);

    if (dr::abs(S[0][1]) > 1e-6 || dr::abs(S[0][2]) > 1e-6 ||
        dr::abs(S[1][0]) > 1e-6 || dr::abs(S[1][2]) > 1e-6 ||
        dr::abs(S[2][0]) > 1e-6 || dr::abs(S[2][1]) > 1e-6)
        Throw("AnimatedTransform: Transformation contains shear, which is not "
              "supported!");

    m_keyframes[time] = { dr::diag(S), Q, T };
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::initialize() {
    if (m_keyframes.empty()) {
        Throw("Animated transform requires at least one keyframe, found 0.");
    }

    size_t n       = m_keyframes.size();
    m_times        = dr::zeros<FloatStorage>(n);
    m_scales       = dr::zeros<FloatStorage>(3 * n);
    m_translations = dr::zeros<FloatStorage>(3 * n);
    m_rotations    = dr::zeros<FloatStorage>(4 * n);

    size_t i = 0;
    for (auto const &[time, kf] : m_keyframes) {
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
    dr::eval(m_times, m_scales, m_translations, m_rotations);
}

MI_VARIANT auto AnimatedTransform<Float, Spectrum>::get_time_bounds() const
    -> ScalarBoundingBox1f {
    if (m_keyframes.empty())
        return { 0.f, 0.f };
    return { m_keyframes.begin()->first, m_keyframes.rbegin()->first };
}

MI_VARIANT bool AnimatedTransform<Float, Spectrum>::operator==(
    const AnimatedTransform &other) const {
    if (m_keyframes.size() != other.m_keyframes.size())
        return false;

    auto it1 = m_keyframes.begin();
    auto it2 = other.m_keyframes.begin();
    for (; it1 != m_keyframes.end(); ++it1, ++it2) {
        if (it1->first != it2->first)
            return false;
        const auto &kf1 = it1->second;
        const auto &kf2 = it2->second;
        if (dr::any_nested(kf1.S != kf2.S) || dr::any_nested(kf1.Q != kf2.Q) ||
            dr::any_nested(kf1.T != kf2.T))
            return false;
    }
    return true;
}

MI_VARIANT auto
AnimatedTransform<Float, Spectrum>::get_translation_bounds() const
    -> ScalarBoundingBox3f {
    ScalarBoundingBox3f bbox;
    for (auto const &[time, kf] : m_keyframes) {
        bbox.expand(ScalarPoint3f(kf.T));
    }
    return bbox;
}

MI_VARIANT auto AnimatedTransform<Float, Spectrum>::get_spatial_bounds(
    const ScalarBoundingBox3f &bbox) const -> ScalarBoundingBox3f {
    if (m_keyframes.empty()) {
        return bbox;
    }
    ScalarBoundingBox3f res;
    if (m_keyframes.size() <= 1) {
        auto trafo = eval_scalar(m_keyframes.begin()->first);
        for (int j = 0; j < 8; ++j)
            res.expand(trafo * bbox.corner(j));
    } else {
        int n_steps                     = 100;
        ScalarBoundingBox1f time_bounds = get_time_bounds();
        ScalarFloat step = time_bounds.extents()[0] / (n_steps - 1);
        for (int i = 0; i < n_steps; ++i) {
            ScalarFloat t                 = time_bounds.min[0] + step * i;
            ScalarAffineTransform4f trafo = eval_scalar(t);
            for (int j = 0; j < 8; ++j)
                res.expand(trafo * bbox.corner(j));
        }
    }
    return res;
}

MI_VARIANT bool AnimatedTransform<Float, Spectrum>::has_scale() const {
    for (auto const &[time, kf] : m_keyframes) {
        if (dr::any_nested(dr::abs(kf.S - ScalarVector3f(1.f)) > 1e-3f))
            return true;
    }
    return false;
}

MI_VARIANT void
AnimatedTransform<Float, Spectrum>::ensure_uniform_keyframes() const {
    if (m_keyframes.size() <= 2)
        return;

    ScalarFloat start = m_keyframes.begin()->first;
    ScalarFloat end   = m_keyframes.rbegin()->first;
    ScalarFloat step  = (end - start) / (m_keyframes.size() - 1);
    size_t i          = 0;
    for (auto const &[time, kf] : m_keyframes) {
        ScalarFloat expected_time = start + i * step;
        if (std::abs(time - expected_time) > 1e-5f) {
            Throw("Expected a uniform range of keyframes, but keyframe %d was "
                  "at time %f, expected %f",
                  i, time, expected_time);
        }
        ++i;
    }
}

MI_VARIANT std::string AnimatedTransform<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
    oss << class_name() << "[";
    if (!m_keyframes.empty()) {
        oss << std::endl;
    }
    for (auto const &[time, kf] : m_keyframes) {
        oss << "  " << time << ": " << kf.to_string() << "," << std::endl;
    }
    oss << "]";
    return oss.str();
}

MI_VARIANT void
AnimatedTransform<Float, Spectrum>::traverse(TraversalCallback *cb) {
    if (m_keyframes.size() == 1) {
        cb->put("transform", m_transform, ParamFlags::Differentiable);
    }
    cb->put("times", m_times, ParamFlags::Differentiable);
    cb->put("scales", m_scales, ParamFlags::Differentiable);
    cb->put("translations", m_translations, ParamFlags::Differentiable);
    cb->put("rotations", m_rotations, ParamFlags::Differentiable);
}

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif

MI_INSTANTIATE_STRUCT(AnimatedTransform)
NAMESPACE_END(mitsuba)
