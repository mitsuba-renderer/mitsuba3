#include <mitsuba/core/animated_transform.h>
#include <mitsuba/core/config.h>
#include <algorithm>
#include <sstream>

#if defined(__GNUG__) // also matches clang, which defines __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdouble-promotion"
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
    size_t n_keyframes = dr::width(m_data) / KeyframeStride;
    if (n_keyframes == 0)
        Throw("Animated transform requires at least one keyframe, found 0.");

    if (n_keyframes == 1) {
        return m_transform.value();
    }

    auto pred = [&](UInt32 idx) {
        return dr::gather<Float>(m_data, idx * KeyframeStride) <= time;
    };
    UInt32 index = math::find_interval<UInt32>((uint32_t) n_keyframes, pred);

    constexpr uint32_t stride = KeyframeStride / 4;
    UInt32 v_idx0 = index * stride,
           v_idx1 = (index + 1) * stride;
    Vector4f time_scale0 = dr::gather<Vector4f>(m_data, v_idx0 + 0);
    Vector4f quat0       = dr::gather<Vector4f>(m_data, v_idx0 + 1);
    Vector4f trans0      = dr::gather<Vector4f>(m_data, v_idx0 + 2);
    Vector4f time_scale1 = dr::gather<Vector4f>(m_data, v_idx1 + 0);
    Vector4f quat1       = dr::gather<Vector4f>(m_data, v_idx1 + 1);
    Vector4f trans1      = dr::gather<Vector4f>(m_data, v_idx1 + 2);

    Float t0 = time_scale0.x();
    Vector3f s0(time_scale0.y(), time_scale0.z(), time_scale0.w());
    Quaternion4f q0 = quat0;
    Vector3f tr0(trans0.x(), trans0.y(), trans0.z());
    Float t1 = time_scale1.x();
    Vector3f s1(time_scale1.y(), time_scale1.z(), time_scale1.w());
    Quaternion4f q1 = quat1;
    Vector3f tr1(trans1.x(), trans1.y(), trans1.z());

    Float t = dr::clip((time - t0) / (t1 - t0), 0.f, 1.f);
    return AffineTransform4f(dr::lerp(s0, s1, t), dr::slerp(q0, q1, t),
                                      dr::lerp(tr0, tr1, t));
}

MI_VARIANT typename AnimatedTransform<Float, Spectrum>::ScalarAffineTransform4f
AnimatedTransform<Float, Spectrum>::eval_scalar(ScalarFloat time) const {
    if (m_keyframes.empty())
        Throw("Animated transform requires at least one keyframe, found 0.");

    if (m_keyframes.size() == 1) {
        return m_transform.scalar();
    }

    // First keyframe at or after `time`.
    auto it1 = std::lower_bound(m_keyframes.begin(), m_keyframes.end(), time,
                                [](const std::pair<ScalarFloat, Keyframe> &a,
                                   ScalarFloat b) { return a.first < b; });
    auto it0 = it1;
    if (it1 == m_keyframes.end()) {
        // Past the last keyframe: clamp to it.
        it1 = std::prev(it1);
        it0 = it1;
    } else if (it1 != m_keyframes.begin()) {
        it0 = std::prev(it1);
    } // else: before the first keyframe, clamp to it (it0 == it1).

    if (it0 == it1) {
        const Keyframe &kf = it0->second;
        return ScalarAffineTransform4f(kf.S, kf.Q, kf.T);
    }
    ScalarFloat t = dr::clip((time - it0->first) / (it1->first - it0->first), 0.f, 1.f);
    const Keyframe &kf0 = it0->second;
    const Keyframe &kf1 = it1->second;
    return ScalarAffineTransform4f(dr::lerp(kf0.S, kf1.S, t),
                                            dr::slerp(kf0.Q, kf1.Q, t),
                                            dr::lerp(kf0.T, kf1.T, t));
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::parameters_changed(
    const std::vector<std::string> &keys) {

    bool views_changed = keys.empty() ||
                         string::contains(keys, "times") ||
                         string::contains(keys, "scale") ||
                         string::contains(keys, "rotation") ||
                         string::contains(keys, "translation"),
         transform_changed = keys.empty() || string::contains(keys, "transform");
    if (!views_changed && !transform_changed)
        return;

    if (views_changed) {
        // Fold the (possibly resized) views back into the packed buffer, then
        // re-derive the host-side keyframes from it
        pack_views();
        unpack_data();
    }

    // With a single keyframe, eval() and eval_scalar() both return m_transform,
    // so it has to agree with m_keyframes[0].
    if (m_keyframes.size() == 1) {
        if (transform_changed) {
            // The user wrote m_transform in place through traversal; refresh its
            // inverse transpose and re-derive the keyframe from it.
            m_transform = m_transform.value().update();
            auto [S, Q, T] = dr::transform_decompose(m_transform.scalar().matrix);
            m_keyframes[0].second = { dr::diag(S), Q, T };
            // 'm_data' and its views are derived from the keyframes, so they
            // have to follow the matrix that was just written
            pack_data();
        } else {
            // The views changed (possibly shrinking the animation down to a
            // single keyframe); m_keyframes[0] is already up to date.
            const auto &kf = m_keyframes[0].second;
            m_transform = ScalarAffineTransform4f(kf.S, kf.Q, kf.T);
        }
        dr::make_opaque(m_transform);
    }

    if (views_changed)
        build_views(); // Re-anchor the views on the new 'm_data'
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::add_keyframe(
    ScalarFloat time, const ScalarAffineTransform4f &trafo) {
    auto [S, Q, T] = dr::transform_decompose(trafo.matrix);
    m_keyframes.push_back({ time, { dr::diag(S), Q, T } });
    // Off-diagonal entries in the scale matrix mean the transformation contains
    // shear, which the decomposition above cannot represent. Only rejected once
    // we know the transform is animated, see initialize().
    m_sheared |= dr::abs(S[0][1]) > 1e-6f || dr::abs(S[0][2]) > 1e-6f ||
                 dr::abs(S[1][0]) > 1e-6f || dr::abs(S[1][2]) > 1e-6f ||
                 dr::abs(S[2][0]) > 1e-6f || dr::abs(S[2][1]) > 1e-6f;
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::initialize() {
    if (m_keyframes.empty()) {
        Throw("Animated transform requires at least one keyframe, found 0.");
    }

    // Sort keyframes by time.
    std::sort(m_keyframes.begin(), m_keyframes.end(),
              [](const std::pair<ScalarFloat, Keyframe> &a,
                 const std::pair<ScalarFloat, Keyframe> &b) {
                  return a.first < b.first;
              });

    // Interpolation runs through the scale/rotation/translation decomposition,
    // which cannot represent shear. A single keyframe is fine: eval() and
    // eval_scalar() return `m_transform`, i.e. the original matrix.
    if (m_sheared && m_keyframes.size() > 1)
        Throw("AnimatedTransform: Transformation contains shear, which is not "
              "supported for animated transformations!");

    for (size_t idx = 1; idx < m_keyframes.size(); ++idx) {
        // Coincident keyframes would make the interpolation weight in eval()
        // and eval_scalar() divide by zero.
        if (m_keyframes[idx].first == m_keyframes[idx - 1].first)
            Throw("AnimatedTransform: found two keyframes at the same time "
                  "(%f), keyframe times must be distinct.",
                  m_keyframes[idx].first);

        // OptiX requires subsequent quaternions to be on the same hemisphere.
        // For larger rotations, additional keys need to be specified.
        if (dr::dot(m_keyframes[idx - 1].second.Q, m_keyframes[idx].second.Q) < 0.f)
            m_keyframes[idx].second.Q = -m_keyframes[idx].second.Q;
    }

    pack_data();
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::pack_data() {
    // Pack all keyframes on the host, then upload them in one go.
    std::vector<ScalarFloat> packed(KeyframeStride * m_keyframes.size());
    size_t i = 0;
    for (auto const &[time, kf] : m_keyframes) {
        pack_keyframe(time, kf, packed.data() + i * KeyframeStride);
        i++;
    }
    m_data = dr::load<FloatStorage>(packed.data(), packed.size());

    dr::eval(m_data);
    build_views();
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::unpack_data() {
    size_t n = dr::width(m_data) / KeyframeStride;

    dr::eval(m_data);
    auto &&packed_data = dr::migrate(m_data, JitBackend::None);
    if constexpr (dr::is_jit_v<Float>)
        dr::sync_thread();
    const ScalarFloat *data_ptr = packed_data.data();

    // Rebuild the host-side keyframes verbatim, in buffer order. The caller
    // owns the invariants (see the header), so this deliberately does not sort
    // or repair anything.
    std::vector<std::pair<ScalarFloat, Keyframe>> new_keyframes;
    new_keyframes.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        const ScalarFloat *kf_ptr = data_ptr + i * KeyframeStride;
        Keyframe kf{ /*S=*/ { kf_ptr[1], kf_ptr[2], kf_ptr[3] },
                     /*Q=*/ { kf_ptr[4], kf_ptr[5], kf_ptr[6], kf_ptr[7] },
                     /*T=*/ { kf_ptr[8], kf_ptr[9], kf_ptr[10] } };
        new_keyframes.push_back({ kf_ptr[0], kf });
    }

    m_keyframes = std::move(new_keyframes);
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::build_views() {
    // A view is a read-back expression, not a differentiation entry point:
    // gradients enter through a *write*, which pack_views() scatters into
    // 'm_data'.
    dr::suspend_grad<Float> guard;

    size_t n = dr::width(m_data) / KeyframeStride;

    // The 'dim' lanes starting at 'offset' of every keyframe, as an (n, dim)
    // row-major buffer
    auto view = [&](uint32_t offset, uint32_t dim) -> FloatStorage {
        if (n == 0)
            return FloatStorage();
        if constexpr (dr::is_jit_v<Float>) {
            UInt32 j    = dr::arange<UInt32>(n * dim),
                   row  = j / dim,
                   lane = j - row * dim;
            return dr::gather<Float>(m_data, row * KeyframeStride + offset + lane);
        } else {
            FloatStorage result = dr::empty<FloatStorage>(n * dim);
            ScalarFloat *dst = result.data();
            const ScalarFloat *src = m_data.data();
            for (size_t row = 0; row < n; ++row)
                for (uint32_t lane = 0; lane < dim; ++lane)
                    *dst++ = src[row * KeyframeStride + offset + lane];
            return result;
        }
    };

    m_times       = TensorXf(view(0, 1), { n });
    m_scale       = TensorXf(view(1, 3), { n, 3 });
    m_rotation    = TensorXf(view(4, 4), { n, 4 });
    m_translation = TensorXf(view(8, 3), { n, 3 });
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::pack_views() {
    size_t n = dr::width(m_times.array());

    auto check = [&](const char *name, const TensorXf &t, uint32_t dim) {
        if (dr::width(t.array()) != n * dim)
            Throw("AnimatedTransform: the keyframe views disagree on the "
                  "number of keyframes: 'times' has %zu entries, but '%s' has "
                  "%zu instead of the expected %zu. All of 'times', 'scale', "
                  "'rotation' and 'translation' must be written together when "
                  "changing the number of keyframes.",
                  n, name, dr::width(t.array()), n * dim);
    };
    check("scale", m_scale, 3);
    check("rotation", m_rotation, 4);
    check("translation", m_translation, 3);

    if (n == 0)
        Throw("Animated transform requires at least one keyframe, found 0.");

    m_data = dr::zeros<FloatStorage>(KeyframeStride * n);

    // Scatter the 'dim' lanes of a view back into every keyframe's chunk
    auto scatter_view = [&](uint32_t offset, uint32_t dim,
                            const TensorXf &t) {
        if constexpr (dr::is_jit_v<Float>) {
            UInt32 j    = dr::arange<UInt32>(n * dim),
                   row  = j / dim,
                   lane = j - row * dim;
            dr::scatter(m_data, Float(t.array()),
                        row * KeyframeStride + offset + lane);
        } else {
            ScalarFloat *dst = m_data.data();
            const ScalarFloat *src = t.array().data();
            for (size_t row = 0; row < n; ++row)
                for (uint32_t lane = 0; lane < dim; ++lane)
                    dst[row * KeyframeStride + offset + lane] =
                        src[row * dim + lane];
        }
    };

    scatter_view(0, 1, m_times);
    scatter_view(1, 3, m_scale);
    scatter_view(4, 4, m_rotation);
    scatter_view(8, 3, m_translation);

    dr::eval(m_data);
}

MI_VARIANT typename AnimatedTransform<Float, Spectrum>::ScalarBoundingBox1f
AnimatedTransform<Float, Spectrum>::get_time_bounds() const {
    if (m_keyframes.empty())
        return ScalarBoundingBox1f();
    return { m_keyframes.begin()->first, m_keyframes.rbegin()->first };
}

MI_VARIANT bool AnimatedTransform<Float, Spectrum>::operator==(
    const AnimatedTransform &other) const {
    if (m_keyframes.size() != other.m_keyframes.size())
        return false;

    if (m_keyframes.size() == 1 && m_transform.value() != other.m_transform.value())
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

    auto expand_at = [&](ScalarFloat time) {
        ScalarAffineTransform4f trafo = eval_scalar(time);
        for (size_t j = 0; j < 8; ++j)
            res.expand(trafo * bbox.corner(j));
    };

    // With a fixed rotation the transformed corners are linear in time, so the
    // keyframes alone bound the motion. (Also covers a single keyframe, where
    // the loop below does not run.)
    bool uniform_rotation = true;
    const auto &q0 = m_keyframes.front().second.Q;
    for (size_t i = 1; i < m_keyframes.size(); ++i) {
        if (dr::abs(dr::dot(m_keyframes[i].second.Q, q0)) <= 1.f - 1e-6f) {
            uniform_rotation = false;
            break;
        }
    }

    if (uniform_rotation) {
        for (auto const &[time, kf] : m_keyframes)
            expand_at(time);
    } else {
        // Uniformly sample the time range ...
        size_t n_steps = 100;
        ScalarBoundingBox1f time_bounds = get_time_bounds();
        ScalarFloat step = time_bounds.extents()[0] / (n_steps - 1);
        for (size_t i = 0; i < n_steps; ++i)
            expand_at(time_bounds.min[0] + step * i);

        // ... and additionally hit every keyframe exactly, since the uniform
        // grid above generally does not land on them.
        for (auto const &[time, kf] : m_keyframes)
            expand_at(time);
    }
    return res;
}

MI_VARIANT bool AnimatedTransform<Float, Spectrum>::has_scale() const {
    if (m_sheared)
        return true;
    if (m_keyframes.size() == 1)
        return m_transform.scalar().has_scale();
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
        if (dr::abs(time - expected_time) > relative_tol) {
            Throw("Expected a uniform range of keyframes, but keyframe %zu was "
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
    if (m_keyframes.size() == 1)
        cb->put("transform", m_transform, ParamFlags::Differentiable);

    cb->put("times",       m_times,       ParamFlags::NonDifferentiable);
    cb->put("scale",       m_scale,       ParamFlags::Differentiable);
    cb->put("rotation",    m_rotation,    ParamFlags::Differentiable);
    cb->put("translation", m_translation, ParamFlags::Differentiable);
}

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

MI_INSTANTIATE_CLASS(AnimatedTransform)
NAMESPACE_END(mitsuba)
