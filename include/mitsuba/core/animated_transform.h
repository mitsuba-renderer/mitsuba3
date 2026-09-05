#pragma once

#include <sstream>
#include <vector>

#include <drjit/quaternion.h>
#include <drjit/tensor.h>
#include <drjit/transform.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/core/field.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

/// Number of floats per keyframe in the packed `AnimatedTransform4f` buffer.
constexpr uint32_t KeyframeStride = 12;

/**
 * Animated transformation
 *
 * This class stores a sequence of keyframes and interpolates between them
 * using linear interpolation for scale and translation and spherical linear
 * interpolation for rotation. Keyframes are stored in decomposed form, which
 * cannot express shear. Transformations with more than one keyframe must
 * therefore be free of shear. Constant (single-keyframe) transformations are
 * exempt, since they are evaluated as a plain matrix.
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
 * The tensors share one buffer and must agree on ``N``. Changing the number
 * of keyframes therefore requires updating all four together.
 *
 * A single-keyframe transformation also exposes a 4x4 matrix under its
 * parent's parameter name (e.g. ``"to_world"``). Evaluation uses this matrix,
 * which takes precedence when written alongside the component views.
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

        /// Write ``time`` and the components into ``out``, which must have
        /// room for ``KeyframeStride`` floats
        void pack(ScalarFloat time, ScalarFloat *out) const {
            out[0]  = time;
            out[1]  = S.x();
            out[2]  = S.y();
            out[3]  = S.z();
            out[4]  = Q.x();
            out[5]  = Q.y();
            out[6]  = Q.z();
            out[7]  = Q.w();
            out[8]  = T.x();
            out[9]  = T.y();
            out[10] = T.z();
            out[11] = 0.f; // padding
        }

        std::string to_string() const {
            std::ostringstream oss;
            oss << "Keyframe[S=" << S << ", Q=" << Q << ", T=" << T << "]";
            return oss.str();
        }
    };

    /// Create a transformation with a single identity keyframe
    AnimatedTransform() : AnimatedTransform(ScalarAffineTransform4f()) { }

    /// Initialize from a constant transformation
    AnimatedTransform(const ScalarAffineTransform4f &trafo);

    /// Initialize from a vector of time values and keyframes
    AnimatedTransform(
        const std::vector<std::pair<ScalarFloat, ScalarAffineTransform4f>>
            &keyframes);

    /**
     * Evaluate the transformation at a specific time
     *
     * Interpolate keyframes from the device buffer. Times outside the range
     * returned by `get_time_bounds` are clamped to the first or last keyframe.
     */
    AffineTransform4f eval(Float time) const;

    /**
     * Scalar evaluation of the transformation
     *
     * Version of `eval` that reads the host-side keyframe list.
     */
    ScalarAffineTransform4f eval_scalar(ScalarFloat time) const;

    /// Check if the transformation is animated
    bool is_animated() const { return m_n_keyframes > 1; }

    /// Make the single-keyframe matrix opaque to prevent its values from being
    /// baked into JIT kernels.
    void make_transform_opaque() { dr::make_opaque(m_transform); }

    /// Return the host-side keyframes of the animated transform.
    const std::vector<std::pair<ScalarFloat, Keyframe>> &keyframes() const {
        return m_keyframes;
    }

    /// Check whether gradients are enabled on the evaluated representation,
    /// either the single-keyframe matrix or the packed keyframe buffer.
    bool parameters_grad_enabled() const {
        if (is_animated())
            return dr::grad_enabled(m_data);
        return dr::grad_enabled(m_transform.value());
    }

    /// Return the time bounds of the animated transform.
    ScalarBoundingBox1f get_time_bounds() const;

    /// Return the bounding box of the translation component of the animated
    /// transform.
    ScalarBoundingBox3f get_translation_bounds() const;

    /// Approximate the swept bounds of ``bbox`` by sampling the transformation
    /// at regular intervals and at every keyframe. These bounds may not be
    /// conservative for nonlinear motion.
    ScalarBoundingBox3f get_spatial_bounds(const ScalarBoundingBox3f &bbox) const;

    /// Check if any keyframe has a scale component different from 1.
    bool has_scale() const;

    /// Check for shear, which is only supported by single-keyframe transforms.
    bool has_shear() const;

    /// Raise an exception if the keyframes are not uniformly spaced in time.
    void ensure_uniform_keyframes() const;

    void traverse(TraversalCallback *cb) override;

    void parameters_changed(const std::vector<std::string> &keys) override;

    std::string to_string() const override;

    MI_DECLARE_CLASS(AnimatedTransform)

protected:
    MI_TRAVERSE_CB(Object, m_transform, m_data, m_times, m_scale,
                   m_rotation, m_translation)

private:
    void add_keyframe(ScalarFloat time, const ScalarAffineTransform4f &trafo);

    /// Initialize keyframe storage and validate the animation.
    void initialize();

    /// Repack ``m_data`` from the host-side ``m_keyframes``
    void pack_data();

    /// Rebuild the host-side ``m_keyframes`` from ``m_data``
    void unpack_data();

    /// Point the ``times``/``scale``/``rotation``/``translation`` views at the
    /// current contents of ``m_data``
    void build_views();

    /// Rebuild ``m_data`` from the (user-written) views, validating that they
    /// agree on the number of keyframes
    void pack_views();

    /// Matrix form of a single-keyframe transformation
    field<AffineTransform4f, ScalarAffineTransform4f> m_transform;

    /// Host-side keyframes, used by `eval_scalar` and `keyframes`
    std::vector<std::pair<ScalarFloat, Keyframe>> m_keyframes;

    /// Packed device copy of ``m_keyframes`` (see `Keyframe::pack`), used by `eval`
    DynamicBuffer<Float> m_data;

    size_t m_n_keyframes = 1;

    /// Set when a keyframe transformation contained shear, which the
    /// decomposition above cannot represent (see `has_shear`)
    bool m_has_shear = false;

    /// Writable views into ``m_data``, see the class documentation
    TensorXf m_times, m_scale, m_rotation, m_translation;
};

template <typename T>
ref<T> Properties::get_animated_transform(std::string_view name) const {
    if (!has_property(name))
        return new T();

    if (type(name) == Type::Object) {
        ref<Object> obj = get<ref<Object>>(name);
        if (T *anim = dynamic_cast<T *>(obj.get()))
            return ref<T>(anim);
        Throw("Property \"%s\" must be a transformation or an <animation> "
              "element, but a '%s' was given.", name, obj->class_name());
    }

    return new T(get<typename T::ScalarAffineTransform4f>(name));
}

MI_EXTERN_CLASS(AnimatedTransform)
NAMESPACE_END(mitsuba)
