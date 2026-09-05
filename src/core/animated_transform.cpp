#include <mitsuba/core/animated_transform.h>
#include <mitsuba/core/config.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <drjit/transform.h>
#include <algorithm>
#include <sstream>

#if defined(__GNUG__) // also matches clang, which defines __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

NAMESPACE_BEGIN(mitsuba)

namespace {
/// Check for off-diagonal stretch relative to the largest scale component.
template <typename Matrix3>
bool keyframe_has_shear(const Matrix3 &S) {
    auto scale = dr::diag(S);
    return dr::any_nested(dr::abs(dr::plain_t<Matrix3>(S - dr::diag(scale))) >
                          1e-5f * dr::max(dr::abs(scale)));
}
} // namespace

MI_VARIANT
AnimatedTransform<Float, Spectrum>::AnimatedTransform(
    const std::vector<std::pair<ScalarFloat, ScalarAffineTransform4f>>
        &keyframes) {
    std::vector<std::pair<ScalarFloat, Keyframe>> decomposed;
    decomposed.reserve(keyframes.size());
    for (const auto &[time, trafo] : keyframes) {
        auto [S, Q, T] = dr::transform_decompose(trafo.matrix);
        if (keyframe_has_shear(S))
            Throw("AnimatedTransform: keyframe transformations must not contain "
                  "shear, the interpolated representation cannot express it.");
        decomposed.push_back({ time, { dr::diag(S), Q, T } });
    }

    upload(std::move(decomposed));
}

MI_VARIANT std::pair<typename AnimatedTransform<Float, Spectrum>::ScalarAffineTransform4f,
                     ref<AnimatedTransform<Float, Spectrum>>>
AnimatedTransform<Float, Spectrum>::from_properties(const Properties &props,
                                                    std::string_view name) {
    if (!props.has_property(name) ||
        props.type(name) != Properties::Type::Object)
        return { props.get<ScalarAffineTransform4f>(name, ScalarAffineTransform4f()),
                 nullptr };

    ref<Object> obj = props.get<ref<Object>>(name);
    ref<AnimatedTransform> anim = dynamic_cast<AnimatedTransform *>(obj.get());
    if (!anim)
        Throw("Property \"%s\" must be a transformation or an <animation> "
              "element, but a '%s' was given.", name, obj->class_name());

    return { ScalarAffineTransform4f(), anim };
}

MI_VARIANT typename AnimatedTransform<Float, Spectrum>::AffineTransform4f
AnimatedTransform<Float, Spectrum>::eval(Float time) const {
    auto fetch = [&](const UInt32 &index) {
        return std::make_tuple(
            dr::gather<Vector3f>(m_scale.array(), index),
            Quaternion4f(dr::gather<Vector4f>(m_rotation.array(), index)),
            dr::gather<Vector3f>(m_translation.array(), index));
    };

    uint32_t n = (uint32_t) m_keyframes.size();
    auto time_at = [&](const UInt32 &idx) {
        return dr::gather<Float>(m_times.array(), idx);
    };
    UInt32 index = math::find_interval<UInt32>(
        n, [&](const UInt32 &idx) { return time_at(idx) <= time; });

    Float t0 = time_at(index), t1 = time_at(index + 1);
    auto [s0, q0, tr0] = fetch(index);
    auto [s1, q1, tr1] = fetch(index + 1);

    Float t = dr::clip((time - t0) / (t1 - t0), 0.f, 1.f);
    return AffineTransform4f(dr::lerp(s0, s1, t),
                             dr::slerp(q0, q1, t),
                             dr::lerp(tr0, tr1, t));
}

MI_VARIANT typename AnimatedTransform<Float, Spectrum>::ScalarAffineTransform4f
AnimatedTransform<Float, Spectrum>::eval_scalar(ScalarFloat time) const {
    // Interval [i, i + 1] containing `time`. The clip below clamps times
    // outside of the keyframe range.
    auto it = std::upper_bound(m_keyframes.begin(), m_keyframes.end(), time,
                               [](ScalarFloat a, const std::pair<ScalarFloat, Keyframe> &b) {
                                   return a < b.first;
                               });
    size_t i = (size_t) std::clamp<ptrdiff_t>(it - m_keyframes.begin() - 1, 0,
                                              (ptrdiff_t) m_keyframes.size() - 2);

    const auto &[t0, kf0] = m_keyframes[i];
    const auto &[t1, kf1] = m_keyframes[i + 1];
    ScalarFloat t = dr::clip((time - t0) / (t1 - t0), 0.f, 1.f);
    return ScalarAffineTransform4f(dr::lerp(kf0.S, kf1.S, t),
                                   dr::slerp(kf0.Q, kf1.Q, t),
                                   dr::lerp(kf0.T, kf1.T, t));
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::parameters_changed(
    const std::vector<std::string> & /* keys */) {
    upload(download());
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::upload(
    std::vector<std::pair<ScalarFloat, Keyframe>> keyframes) {
    size_t n = keyframes.size();
    if (n < 2)
        Throw("AnimatedTransform: an animation requires at least two "
              "keyframes, but %zu were given. Static objects must use a plain "
              "transformation.", n);

    std::sort(keyframes.begin(), keyframes.end(),
              [](const std::pair<ScalarFloat, Keyframe> &a,
                 const std::pair<ScalarFloat, Keyframe> &b) {
                  return a.first < b.first;
              });

    for (size_t i = 1; i < n; ++i) {
        // Coincident keyframes would make the interpolation weight in eval()
        // and eval_scalar() divide by zero.
        if (keyframes[i].first == keyframes[i - 1].first)
            Throw("AnimatedTransform: found two keyframes at the same time "
                  "(%f), keyframe times must be distinct.",
                  keyframes[i].first);

        // Keep adjacent quaternions in the same hemisphere for OptiX.
        // Rotations beyond 180 degrees need intermediate keyframes.
        if (dr::dot(keyframes[i - 1].second.Q, keyframes[i].second.Q) < 0.f)
            keyframes[i].second.Q = -keyframes[i].second.Q;
    }

    m_keyframes = std::move(keyframes);

    std::vector<ScalarFloat> times(n), scale(3 * n), rotation(4 * n),
                             translation(3 * n);
    for (size_t i = 0; i < n; ++i) {
        const auto &[time, kf] = m_keyframes[i];
        times[i] = time;
        for (size_t j = 0; j < 3; ++j) {
            scale[3 * i + j]       = kf.S[j];
            translation[3 * i + j] = kf.T[j];
        }
        for (size_t j = 0; j < 4; ++j)
            rotation[4 * i + j] = kf.Q[j];
    }

    m_times       = TensorXf(dr::load<FloatStorage>(times.data(), n), { n });
    m_scale       = TensorXf(dr::load<FloatStorage>(scale.data(), 3 * n), { n, 3 });
    m_rotation    = TensorXf(dr::load<FloatStorage>(rotation.data(), 4 * n), { n, 4 });
    m_translation = TensorXf(dr::load<FloatStorage>(translation.data(), 3 * n), { n, 3 });
}

MI_VARIANT std::vector<std::pair<typename AnimatedTransform<Float, Spectrum>::ScalarFloat,
                                 typename AnimatedTransform<Float, Spectrum>::Keyframe>>
AnimatedTransform<Float, Spectrum>::download() const {
    size_t n = dr::width(m_times.array());

    auto check = [&](const char *name, const TensorXf &t, uint32_t dim) {
        if (dr::width(t.array()) != n * dim)
            Throw("AnimatedTransform: the keyframe tensors disagree on the "
                  "number of keyframes: 'times' has %zu entries, but '%s' has "
                  "%zu instead of the expected %zu. All of 'times', 'scale', "
                  "'rotation' and 'translation' must be written together when "
                  "changing the number of keyframes.",
                  n, name, dr::width(t.array()), n * dim);
    };
    check("scale", m_scale, 3);
    check("rotation", m_rotation, 4);
    check("translation", m_translation, 3);

    if constexpr (dr::is_jit_v<Float>)
        dr::eval(m_times.array(), m_scale.array(), m_rotation.array(),
                 m_translation.array());

    auto to_host = [](const TensorXf &t) {
        if constexpr (dr::is_jit_v<Float>)
            return dr::migrate(dr::detach(t.array()), JitBackend::None);
        else
            return t.array();
    };

    auto times = to_host(m_times), scale = to_host(m_scale),
         rotation = to_host(m_rotation), translation = to_host(m_translation);
    if constexpr (dr::is_jit_v<Float>)
        dr::sync_thread();

    const ScalarFloat *t = times.data(), *s = scale.data(),
                      *r = rotation.data(), *tr = translation.data();

    std::vector<std::pair<ScalarFloat, Keyframe>> keyframes;
    keyframes.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        Keyframe kf{ /*S=*/ { s[3 * i], s[3 * i + 1], s[3 * i + 2] },
                     /*Q=*/ { r[4 * i], r[4 * i + 1], r[4 * i + 2], r[4 * i + 3] },
                     /*T=*/ { tr[3 * i], tr[3 * i + 1], tr[3 * i + 2] } };
        keyframes.push_back({ t[i], kf });
    }
    return keyframes;
}

MI_VARIANT typename AnimatedTransform<Float, Spectrum>::ScalarBoundingBox3f
AnimatedTransform<Float, Spectrum>::get_spatial_bounds(
    const ScalarBoundingBox3f &bbox) const {
    ScalarBoundingBox3f res;

    auto expand_at = [&](ScalarFloat time) {
        ScalarAffineTransform4f trafo = eval_scalar(time);
        for (size_t j = 0; j < 8; ++j)
            res.expand(trafo * bbox.corner(j));
    };

    // 1) Uniformly sample the time range
    size_t n_steps = 100;
    ScalarFloat t_min = m_keyframes.front().first,
                step  = (m_keyframes.back().first - t_min) / (n_steps - 1);
    for (size_t i = 0; i < n_steps; ++i)
        expand_at(t_min + step * i);
    // 2) Hit each key frame exactly, since uniform sampling may miss them
    for (auto const &[time, kf] : m_keyframes)
        expand_at(time);
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
    ScalarFloat relative_tol = 1e-5f * dr::maximum(end - start, dr::maximum(dr::abs(start), dr::abs(end)));
    size_t i = 0;
    for (auto const &[time, kf] : m_keyframes) {
        ScalarFloat expected_time = start + i * step;
        if (dr::abs(time - expected_time) > relative_tol)
            Throw("Expected a uniform range of keyframes, but keyframe %zu was "
                  "at time %f, expected %f",
                  i, time, expected_time);
        ++i;
    }
}

MI_VARIANT std::string AnimatedTransform<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
    oss << class_name() << "[" << std::endl;
    for (auto const &[time, kf] : m_keyframes)
        oss << "  " << time << ": Keyframe[S=" << kf.S << ", Q=" << kf.Q
            << ", T=" << kf.T << "]," << std::endl;
    oss << "]";
    return oss.str();
}

MI_VARIANT std::string AnimatedTransform<Float, Spectrum>::transform_string(
    const ScalarAffineTransform4f &trafo, const AnimatedTransform *anim) {
    if (anim)
        return anim->to_string();
    std::ostringstream oss;
    oss << trafo;
    return oss.str();
}

MI_VARIANT void
AnimatedTransform<Float, Spectrum>::traverse(TraversalCallback *cb) {
    cb->put("times",       m_times,       ParamFlags::NonDifferentiable);
    cb->put("scale",       m_scale,       ParamFlags::NonDifferentiable);
    cb->put("rotation",    m_rotation,    ParamFlags::NonDifferentiable);
    cb->put("translation", m_translation, ParamFlags::NonDifferentiable);
}

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

MI_INSTANTIATE_CLASS(AnimatedTransform)
NAMESPACE_END(mitsuba)
