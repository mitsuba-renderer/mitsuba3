#pragma once

#include <utility>
#include <vector>

#include <drjit/quaternion.h>
#include <drjit/tensor.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Animated transformation
 *
 * This class stores a sequence of keyframes and interpolates between them
 * using linear interpolation for scale and translation and spherical linear
 * interpolation for rotation. Keyframes are stored in decomposed form, which
 * cannot express shear. An animation has at least two keyframes. Objects with
 * a constant transformation store it as a plain matrix instead.
 *
 * `traverse` exposes the keyframes as four tensors.
 *
 * .. list-table::
 *    :header-rows: 1
 *
 *    * - Component
 *      - Shape
 *      - Contents
 *    * - ``times``
 *      - ``(N,)``
 *      - Keyframe times
 *    * - ``scale``
 *      - ``(N, 3)``
 *      - Per-axis scale factors
 *    * - ``rotation``
 *      - ``(N, 4)``
 *      - Rotation quaternions in ``(x, y, z, w)`` order
 *    * - ``translation``
 *      - ``(N, 3)``
 *      - Translations
 *
 * The tensors must agree on ``N``. Changing the number of keyframes therefore
 * requires updating all four together.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB AnimatedTransform : public Object {
public:
    MI_IMPORT_CORE_TYPES()

    using FloatStorage = DynamicBuffer<Float>;

    /// Decomposed scale, rotation, and translation of a keyframe.
    struct Keyframe {
        ScalarVector3f S;
        ScalarQuaternion4f Q;
        ScalarVector3f T;
    };

    /// Initialize from a vector of time values and keyframes
    AnimatedTransform(
        const std::vector<std::pair<ScalarFloat, ScalarAffineTransform4f>>
            &keyframes);

    /**
     * Look up a transformation parameter that may be animated
     *
     * Returns a pair containing a constant transformation and an animation.
     * If the parameter ``name`` holds an animation, the pair contains the
     * identity and the animation. Otherwise, it contains the constant
     * transformation (or the identity, if the parameter is missing) and
     * ``nullptr``.
     */
    static std::pair<ScalarAffineTransform4f, ref<AnimatedTransform>>
    from_properties(const Properties &props, std::string_view name);

    /**
     * Evaluate the transformation at a specific time
     *
     * Interpolate keyframes from the device tensors. Times outside the
     * keyframe range are clamped to the first or last keyframe.
     */
    AffineTransform4f eval(Float time) const;

    /**
     * Scalar evaluation of the transformation
     *
     * Version of `eval` that reads the host-side keyframe list.
     */
    ScalarAffineTransform4f eval_scalar(ScalarFloat time) const;

    /// Return the host-side keyframes of the animated transform.
    const std::vector<std::pair<ScalarFloat, Keyframe>> &keyframes() const {
        return m_keyframes;
    }

    /// Approximate the swept bounds of ``bbox`` by sampling the transformation
    /// at regular intervals and at every keyframe. These bounds may not be
    /// conservative for nonlinear motion.
    ScalarBoundingBox3f get_spatial_bounds(const ScalarBoundingBox3f &bbox) const;

    /// Check if any keyframe has a scale component different from 1.
    bool has_scale() const;

    /// Raise an exception if the keyframes are not uniformly spaced in time.
    void ensure_uniform_keyframes() const;

    void traverse(TraversalCallback *cb) override;

    void parameters_changed(const std::vector<std::string> &keys) override;

    std::string to_string() const override;

    /// Return a string representation of ``anim``, or of the constant
    /// transformation ``trafo`` if ``anim`` is ``nullptr``
    static std::string transform_string(const ScalarAffineTransform4f &trafo,
                                        const AnimatedTransform *anim);

    MI_DECLARE_CLASS(AnimatedTransform)

protected:
    MI_TRAVERSE_CB(Object, m_times, m_scale, m_rotation, m_translation)

private:
    /// Read the (possibly user-written) keyframe tensors, validating that
    /// they agree on the number of keyframes
    std::vector<std::pair<ScalarFloat, Keyframe>> download() const;

    /// Sort and validate ``keyframes``, keep adjacent quaternions in the same
    /// hemisphere, and store them in ``m_keyframes`` and the keyframe tensors
    void upload(std::vector<std::pair<ScalarFloat, Keyframe>> keyframes);

    /// Host-side keyframes, used by `eval_scalar` and `keyframes`
    std::vector<std::pair<ScalarFloat, Keyframe>> m_keyframes;

    /// Device-side keyframes used by `eval`, see the class documentation
    TensorXf m_times, m_scale, m_rotation, m_translation;
};

MI_EXTERN_CLASS(AnimatedTransform)
NAMESPACE_END(mitsuba)
