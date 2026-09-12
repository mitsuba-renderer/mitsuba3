#include <mitsuba/core/animated_transform.h>
#include <mitsuba/core/config.h>
#include <algorithm>
#include <sstream>

#if defined(__GNUG__) // also matches clang, which defines __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

NAMESPACE_BEGIN(mitsuba)

namespace {

/**
 * Decomposes an affine 4x4 transformation matrix into scale (S), shear (H),
 * rotation quaternion (Q), and translation (T).
 *
 * For transforms without shear, this uses Dr.Jit's polar decomposition.
 * When shear is present, it uses an upper-triangular QR decomposition
 * M = T * R(Q) * U(S, H) as required by Embree, OptiX, and Metal.
 */
template <typename Matrix4>
auto transform_decompose_srt(const Matrix4 &a) {
    using Value = dr::entry_t<Matrix4>;
    using Vector3 = dr::Array<Value, 3>;

    // Fast path: standard polar decomposition for rigid / scaled transforms
    auto [P, Q, T] = dr::transform_decompose(a);

    // A mirroring transformation has to take the QR path below instead: the
    // polar decomposition folds the reflection into the rotation, which can
    // make it a 180 degree rotation, and 'matrix_to_quat' recovers the axis of
    // those with the wrong sign. QR keeps the reflection in the scale.
    bool mirrored = dr::det(dr::Matrix<Value, 3>(a)) < 0.f;

    // 'P' is negative definite in that case, so the relative tolerance below
    // has to be built from a magnitude
    Value max_s = dr::maximum(dr::abs(P(0, 0)),
                              dr::maximum(dr::abs(P(1, 1)), dr::abs(P(2, 2))));
    bool has_shear = dr::abs(P(0, 1)) > 1e-6f * max_s ||
                     dr::abs(P(0, 2)) > 1e-6f * max_s ||
                     dr::abs(P(1, 2)) > 1e-6f * max_s;
    if (!mirrored && !has_shear)
        return std::make_tuple(dr::diag(P), Vector3(0.f), Q, T);

    // Upper-triangular QR decomposition for sheared transforms
    Vector3 c0(a(0, 0), a(1, 0), a(2, 0)),
            c1(a(0, 1), a(1, 1), a(2, 1)),
            c2(a(0, 2), a(1, 2), a(2, 2));

    Value sx = dr::norm(c0);
    Vector3 r0 = c0 / sx;
    Value h_xy = dr::dot(r0, c1);
    Vector3 v1 = c1 - h_xy * r0;
    Value sy = dr::norm(v1);
    Vector3 r1 = v1 / sy;
    Value h_xz = dr::dot(r0, c2);
    Value h_yz = dr::dot(r1, c2);
    Vector3 r2 = dr::cross(r0, r1);
    Value sz = dr::dot(r2, c2);

    dr::Matrix<Value, 3> R(
        r0.x(), r1.x(), r2.x(),
        r0.y(), r1.y(), r2.y(),
        r0.z(), r1.z(), r2.z()
    );

    return std::make_tuple(Vector3(sx, sy, sz),
                           Vector3(h_xy, h_xz, h_yz),
                           dr::matrix_to_quat(R),
                           T);
}

} // namespace

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
    if (m_n_keyframes == 1) {
        return m_transform.value();
    }

    auto pred = [&](UInt32 idx) {
        return dr::gather<Float>(m_data, idx * KeyframeStride) <= time;
    };
    UInt32 index = math::find_interval<UInt32>((uint32_t) m_n_keyframes, pred);

    constexpr uint32_t stride = KeyframeStride / 4;
    UInt32 v_idx0 = index * stride,
           v_idx1 = (index + 1) * stride;
    Vector4f time_scale0 = dr::gather<Vector4f>(m_data, v_idx0 + 0);
    Vector4f shear0      = dr::gather<Vector4f>(m_data, v_idx0 + 1);
    Vector4f quat0       = dr::gather<Vector4f>(m_data, v_idx0 + 2);
    Vector4f trans0      = dr::gather<Vector4f>(m_data, v_idx0 + 3);
    Vector4f time_scale1 = dr::gather<Vector4f>(m_data, v_idx1 + 0);
    Vector4f shear1      = dr::gather<Vector4f>(m_data, v_idx1 + 1);
    Vector4f quat1       = dr::gather<Vector4f>(m_data, v_idx1 + 2);
    Vector4f trans1      = dr::gather<Vector4f>(m_data, v_idx1 + 3);

    Float t0 = time_scale0.x();
    Vector3f s0(time_scale0.y(), time_scale0.z(), time_scale0.w());
    Vector3f h0(shear0.x(), shear0.y(), shear0.z());
    Quaternion4f q0 = quat0;
    Vector3f tr0(trans0.x(), trans0.y(), trans0.z());
    Float t1 = time_scale1.x();
    Vector3f s1(time_scale1.y(), time_scale1.z(), time_scale1.w());
    Vector3f h1(shear1.x(), shear1.y(), shear1.z());
    Quaternion4f q1 = quat1;
    Vector3f tr1(trans1.x(), trans1.y(), trans1.z());

    Float t = dr::clip((time - t0) / (t1 - t0), 0.f, 1.f);
    return AffineTransform4f(dr::lerp(s0, s1, t),
                             dr::lerp(h0, h1, t),
                             dr::slerp(q0, q1, t),
                             dr::lerp(tr0, tr1, t));
}

MI_VARIANT typename AnimatedTransform<Float, Spectrum>::ScalarAffineTransform4f
AnimatedTransform<Float, Spectrum>::eval_scalar(ScalarFloat time) const {
    if (m_n_keyframes == 1) {
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
        return ScalarAffineTransform4f(kf.S, kf.H, kf.Q, kf.T);
    }
    ScalarFloat t = dr::clip((time - it0->first) / (it1->first - it0->first), 0.f, 1.f);
    const Keyframe &kf0 = it0->second;
    const Keyframe &kf1 = it1->second;
    return ScalarAffineTransform4f(dr::lerp(kf0.S, kf1.S, t),
                                   dr::lerp(kf0.H, kf1.H, t),
                                   dr::slerp(kf0.Q, kf1.Q, t),
                                   dr::lerp(kf0.T, kf1.T, t));
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::parameters_changed(
    const std::vector<std::string> &keys) {

    bool views_changed = keys.empty() ||
                         string::contains(keys, "times") ||
                         string::contains(keys, "scale") ||
                         string::contains(keys, "shear") ||
                         string::contains(keys, "rotation") ||
                         string::contains(keys, "translation"),
         transform_changed = keys.empty() || string::contains(keys, "");
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
    if (m_n_keyframes == 1) {
        if (transform_changed) {
            // The user wrote m_transform in place through traversal; refresh its
            // inverse transpose and re-derive the keyframe from it.
            m_transform = m_transform.value().update();
            auto [S, H, Q, T] = transform_decompose_srt(m_transform.scalar().matrix);
            m_keyframes[0].second = { S, H, Q, T };
            // 'm_data' and its views are derived from the keyframes, so they
            // have to follow the matrix that was just written
            pack_data();
        } else {
            // The views changed (possibly shrinking the animation down to a
            // single keyframe); m_keyframes[0] is already up to date.
            const auto &kf = m_keyframes[0].second;
            m_transform = ScalarAffineTransform4f(kf.S, kf.H, kf.Q, kf.T);
        }
        dr::make_opaque(m_transform);
    }

    if (views_changed)
        build_views(); // Re-anchor the views on the new 'm_data'
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::add_keyframe(
    ScalarFloat time, const ScalarAffineTransform4f &trafo) {
    auto [S, H, Q, T] = transform_decompose_srt(trafo.matrix);
    m_keyframes.push_back({ time, { S, H, Q, T } });
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
    m_n_keyframes = m_keyframes.size();

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
                     /*H=*/ { kf_ptr[4], kf_ptr[5], kf_ptr[6] },
                     /*Q=*/ { kf_ptr[8], kf_ptr[9], kf_ptr[10], kf_ptr[11] },
                     /*T=*/ { kf_ptr[12], kf_ptr[13], kf_ptr[14] } };
        new_keyframes.push_back({ kf_ptr[0], kf });
    }

    m_keyframes = std::move(new_keyframes);
    m_n_keyframes = m_keyframes.size();
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::build_views() {
    dr::suspend_grad<Float> guard;
    size_t n = dr::width(m_data) / KeyframeStride;

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
    m_shear       = TensorXf(view(4, 3), { n, 3 });
    m_rotation    = TensorXf(view(8, 4), { n, 4 });
    m_translation = TensorXf(view(12, 3), { n, 3 });
}

MI_VARIANT void AnimatedTransform<Float, Spectrum>::pack_views() {
    size_t n = dr::width(m_times.array());

    auto check = [&](const char *name, const TensorXf &t, uint32_t dim) {
        if (dr::width(t.array()) != n * dim)
            Throw("AnimatedTransform: the keyframe views disagree on the "
                  "number of keyframes: 'times' has %zu entries, but '%s' has "
                  "%zu instead of the expected %zu. All of 'times', 'scale', "
                  "'shear', 'rotation' and 'translation' must be written together when "
                  "changing the number of keyframes.",
                  n, name, dr::width(t.array()), n * dim);
    };
    check("scale", m_scale, 3);
    check("shear", m_shear, 3);
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
    scatter_view(4, 3, m_shear);
    scatter_view(8, 4, m_rotation);
    scatter_view(12, 3, m_translation);

    dr::eval(m_data);
}

MI_VARIANT typename AnimatedTransform<Float, Spectrum>::ScalarBoundingBox1f
AnimatedTransform<Float, Spectrum>::get_time_bounds() const {
    return { m_keyframes.begin()->first, m_keyframes.rbegin()->first };
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
    ScalarBoundingBox3f res;

    auto expand_at = [&](ScalarFloat time) {
        ScalarAffineTransform4f trafo = eval_scalar(time);
        for (size_t j = 0; j < 8; ++j)
            res.expand(trafo * bbox.corner(j));
    };

    if (!is_animated()) {
        expand_at(0.f);
        return res;
    }

    // 1) Uniformly sample the time range
    size_t n_steps = 100;
    ScalarBoundingBox1f time_bounds = get_time_bounds();
    ScalarFloat step = time_bounds.extents()[0] / (n_steps - 1);
    for (size_t i = 0; i < n_steps; ++i)
        expand_at(time_bounds.min[0] + step * i);
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

MI_VARIANT bool AnimatedTransform<Float, Spectrum>::has_shear() const {
    for (auto const &[time, kf] : m_keyframes) {
        if (dr::any_nested(dr::abs(kf.H) > 1e-5f))
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
    oss << class_name() << "[" << std::endl;
    for (auto const &[time, kf] : m_keyframes) {
        oss << "  " << time << ": " << kf.to_string() << "," << std::endl;
    }
    oss << "]";
    return oss.str();
}

MI_VARIANT void
AnimatedTransform<Float, Spectrum>::traverse(TraversalCallback *cb) {
    if (m_n_keyframes == 1)
        cb->put("", m_transform, ParamFlags::Differentiable);

    cb->put("times",       m_times,       ParamFlags::NonDifferentiable);
    cb->put("scale",       m_scale,       ParamFlags::Differentiable);
    cb->put("shear",       m_shear,       ParamFlags::Differentiable);
    cb->put("rotation",    m_rotation,    ParamFlags::Differentiable);
    cb->put("translation", m_translation, ParamFlags::Differentiable);
}

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

MI_INSTANTIATE_CLASS(AnimatedTransform)
NAMESPACE_END(mitsuba)
