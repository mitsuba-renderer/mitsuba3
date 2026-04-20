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

constexpr uint32_t stride = 12;

template <typename Transform, typename Scalar = typename Transform::Float>
Transform compose(const Vector<Scalar, 3> &s, const dr::Quaternion<Scalar> &q,
                  const Vector<Scalar, 3> &t) {
    using Matrix3f     = dr::Matrix<Scalar, 3>;
    using Matrix4f     = dr::Matrix<Scalar, 4>;
    using Quaternion4f = dr::Quaternion<Scalar>;
    Matrix3f s_mat     = dr::diag(s);
    return Transform(
        dr::transform_compose<Matrix4f>(s_mat, q, t),
        dr::transpose(dr::transform_compose_inverse<Matrix4f>(s_mat, q, t)));
}

MI_VARIANT
AnimatedTransform<Float, Spectrum>::AnimatedTransform(
    const ScalarAffineTransform4f &trafo) {
    m_transform = AffineTransform4f(trafo);
    add_keyframe(ScalarFloat(0), trafo);
    initialize();
}

MI_VARIANT
AnimatedTransform<Float, Spectrum>::AnimatedTransform(
    const std::vector<std::pair<ScalarFloat, ScalarAffineTransform4f>>
        &keyframes) {
    if (keyframes.size() == 1) {
        m_transform = AffineTransform4f(keyframes.begin()->second);
    }
    for (const auto &[time, trafo] : keyframes) {
        add_keyframe(time, trafo);
    }
    initialize();
}

MI_VARIANT typename AnimatedTransform<Float, Spectrum>::AffineTransform4f
AnimatedTransform<Float, Spectrum>::eval(Float time) const {
    size_t n_keyframes = dr::width(m_data) / stride;
    if (n_keyframes == 0)
        Throw("Animated transform requires at least one keyframe, found 0.");

    if (n_keyframes == 1) {
        return m_transform.value();
    }

    UInt32 v_idx0, v_idx1;
    if (n_keyframes == 2) {
        v_idx0 = 0;
        v_idx1 = 3;
    } else {
        // General case with binary search
        auto pred = [&](UInt32 idx) {
            return dr::gather<Float>(m_data, idx * stride) <= time;
        };
        UInt32 index =
            math::find_interval<UInt32>((uint32_t) n_keyframes, pred);
        v_idx0 = index * 3;
        v_idx1 = (index + 1) * 3;
    }

    Vector4f chunk0_0 = dr::gather<Vector4f>(m_data, v_idx0 + 0);
    Vector4f chunk0_1 = dr::gather<Vector4f>(m_data, v_idx0 + 1);
    Vector4f chunk0_2 = dr::gather<Vector4f>(m_data, v_idx0 + 2);
    Vector4f chunk1_0 = dr::gather<Vector4f>(m_data, v_idx1 + 0);
    Vector4f chunk1_1 = dr::gather<Vector4f>(m_data, v_idx1 + 1);
    Vector4f chunk1_2 = dr::gather<Vector4f>(m_data, v_idx1 + 2);

    Float t0 = chunk0_0.x();
    Vector3f s0(chunk0_0.y(), chunk0_0.z(), chunk0_0.w());
    Quaternion4f q0 = chunk0_1;
    Vector3f tr0(chunk0_2.x(), chunk0_2.y(), chunk0_2.z());
    Float t1 = chunk1_0.x();
    Vector3f s1(chunk1_0.y(), chunk1_0.z(), chunk1_0.w());
    Quaternion4f q1 = chunk1_1;
    Vector3f tr1(chunk1_2.x(), chunk1_2.y(), chunk1_2.z());

    Float t = dr::clip((time - t0) / (t1 - t0), 0.f, 1.f);
    return compose<AffineTransform4f>(dr::lerp(s0, s1, t), dr::slerp(q0, q1, t),
                                      dr::lerp(tr0, tr1, t));
}

MI_VARIANT typename AnimatedTransform<Float, Spectrum>::ScalarAffineTransform4f
AnimatedTransform<Float, Spectrum>::eval_scalar(ScalarFloat time) const {
    if (m_keyframes.empty())
        Throw("Animated transform requires at least one keyframe, found 0.");

    if (m_keyframes.size() == 1) {
        return m_transform.scalar();
    }

    auto it1 = std::lower_bound(m_keyframes.begin(), m_keyframes.end(), time,
                                [](const std::pair<ScalarFloat, Keyframe> &a,
                                   ScalarFloat b) { return a.first < b; });
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
        const Keyframe &kf = it0->second;
        return compose<ScalarAffineTransform4f>(kf.S, kf.Q, kf.T);
    }
    ScalarFloat t = std::clamp((time - it0->first) / (it1->first - it0->first), 0.f, 1.f);
    const Keyframe &kf0 = it0->second;
    const Keyframe &kf1 = it1->second;
    return compose<ScalarAffineTransform4f>(dr::lerp(kf0.S, kf1.S, t),
                                            dr::slerp(kf0.Q, kf1.Q, t),
                                            dr::lerp(kf0.T, kf1.T, t));
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::parameters_changed(
    const std::vector<std::string> &keys) {

    size_t n = dr::width(m_data) / stride;
    if (dr::width(m_data) % stride != 0) {
        Throw("Size of m_data is not a multiple of stride (= 12).");
    }
    dr::eval(m_data);
    auto &&packed_data = dr::migrate(m_data, AllocType::Host);
    if constexpr (dr::is_jit_v<Float>)
        dr::sync_thread();
    const ScalarFloat *data_ptr = packed_data.data();
    bool was_animated           = m_keyframes.size() > 1;
    std::vector<std::pair<ScalarFloat, Keyframe>> new_keyframes;
    new_keyframes.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        Keyframe kf{ .S = { data_ptr[i * stride + 1], data_ptr[i * stride + 2],
                            data_ptr[i * stride + 3] },
                     .Q = { data_ptr[i * stride + 4], data_ptr[i * stride + 5],
                            data_ptr[i * stride + 6],
                            data_ptr[i * stride + 7] },
                     .T = { data_ptr[i * stride + 8], data_ptr[i * stride + 9],
                            data_ptr[i * stride + 10] } };
        new_keyframes.push_back({ data_ptr[i * stride + 0], kf });
    }

    m_keyframes = std::move(new_keyframes);

    // Handles the non-animated case of n == 1 transforms being used.
    if (n == 1) {
        const auto &kf = m_keyframes[0].second;
        if (was_animated) {
            // Transition from N > 1 to N == 1: rebuild single transform
            m_transform = compose<ScalarAffineTransform4f>(kf.S, kf.Q, kf.T);
            dr::make_opaque(m_transform);
        } else {
            // Was already size 1, check if "transform" changed
            if (keys.empty() || string::contains(keys, "transform")) {
                m_transform = m_transform.value().update();
                dr::make_opaque(m_transform);
                // Update keyframe from m_transform.
                ScalarAffineTransform4f trafo = m_transform.scalar();
                auto [S, Q, T]        = dr::transform_decompose(trafo.matrix);
                m_keyframes[0].second = { dr::diag(S), Q, T };
            }
        }
    }
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::add_keyframe(
    ScalarFloat time, const ScalarAffineTransform4f &trafo) {
    auto [S, Q, T] = dr::transform_decompose(trafo.matrix);

    if (dr::abs(S[0][1]) > 1e-6f || dr::abs(S[0][2]) > 1e-6f ||
        dr::abs(S[1][0]) > 1e-6f || dr::abs(S[1][2]) > 1e-6f ||
        dr::abs(S[2][0]) > 1e-6f || dr::abs(S[2][1]) > 1e-6f)
        Throw("AnimatedTransform: Transformation contains shear, which is not "
              "supported!");

    m_keyframes.push_back({ time, { dr::diag(S), Q, T } });
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::initialize() {
    if (m_keyframes.empty()) {
        Throw("Animated transform requires at least one keyframe, found 0.");
    }

    // Ensure keyframes are sorted by time
    std::sort(m_keyframes.begin(), m_keyframes.end(),
              [](const std::pair<ScalarFloat, Keyframe> &a,
                 const std::pair<ScalarFloat, Keyframe> &b) {
                  return a.first < b.first;
              });

    size_t n = m_keyframes.size();
    m_data   = dr::zeros<FloatStorage>(stride * n);

    size_t i = 0;
    for (auto const &[time, kf] : m_keyframes) {
        if constexpr (dr::is_jit_v<Float>) {
            dr::scatter(m_data, time, i * stride + 0);
            dr::scatter(m_data, kf.S.x(), i * stride + 1);
            dr::scatter(m_data, kf.S.y(), i * stride + 2);
            dr::scatter(m_data, kf.S.z(), i * stride + 3);
            for (size_t j = 0; j < 4; ++j) {
                dr::scatter(m_data, kf.Q[j], i * stride + 4 + j);
            }
            dr::scatter(m_data, kf.T.x(), i * stride + 8);
            dr::scatter(m_data, kf.T.y(), i * stride + 9);
            dr::scatter(m_data, kf.T.z(), i * stride + 10);
        } else {
            dr::store(m_data.data() + i * stride + 0, time);
            dr::store(m_data.data() + i * stride + 1, kf.S);
            dr::store(m_data.data() + i * stride + 4, kf.Q);
            dr::store(m_data.data() + i * stride + 8, kf.T);
        }
        i++;
    }
    dr::eval(m_data);
}

MI_VARIANT typename AnimatedTransform<Float, Spectrum>::ScalarBoundingBox1f
AnimatedTransform<Float, Spectrum>::get_time_bounds() const {
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

MI_VARIANT typename AnimatedTransform<Float, Spectrum>::ScalarBoundingBox3f
AnimatedTransform<Float, Spectrum>::get_translation_bounds() const {
    ScalarBoundingBox3f bbox;
    for (auto const &[time, kf] : m_keyframes) {
        bbox.expand(ScalarPoint3f(kf.T));
    }
    return bbox;
}

MI_VARIANT typename AnimatedTransform<Float, Spectrum>::ScalarBoundingBox3f
AnimatedTransform<Float, Spectrum>::get_spatial_bounds(
    const ScalarBoundingBox3f &bbox) const {
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
    cb->put("data", m_data, ParamFlags::Differentiable);
}

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif

MI_INSTANTIATE_STRUCT(AnimatedTransform)
NAMESPACE_END(mitsuba)
