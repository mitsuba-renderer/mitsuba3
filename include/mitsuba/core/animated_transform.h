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

/// Number of floats per keyframe in the packed ``AnimatedTransform`` buffer.
constexpr uint32_t KeyframeStride = 16;

/**
 * Animated transformation
 *
 * This class stores a sequence of transformations and interpolates between them
 * using a combination of linear interpolation (for translation, scaling, and
 * shear) and spherical linear interpolation (for rotation).
 *
 * Internally, keyframes are packed into a flat buffer with a stride of
 * ``KeyframeStride`` floats per keyframe to optimize vectorized loads. The
 * layout per keyframe is:
 *
 * ``[time, scale.x, scale.y, scale.z, shear.x, shear.y, shear.z, unused,
 * quat.x, quat.y, quat.z, quat.w, trans.x, trans.y, trans.z, unused]``
 *
 * The class keeps two redundant representations of the same animation: the
 * device-side buffer ``m_data`` (read by ``eval()``) and the host-side keyframe
 * list ``m_keyframes`` (read by ``eval_scalar()`` and ``keyframes()``). They
 * are synchronized by the constructors and by ``parameters_changed()``.
 *
 * Through ``mitsuba::traverse()`` the transformation always exposes the
 * keyframes as tensor views into ``m_data``, one per component: ``"times"``
 * ``(N,)``, ``"scale"`` ``(N, 3)``, ``"shear"`` ``(N, 3)``, ``"rotation"``
 * ``(N, 4)`` and ``"translation"`` ``(N, 3)``. They share one buffer and must
 * agree on ``N``, so changing the number of keyframes means writing all five
 * together.
 *
 * While the transformation holds a *single* keyframe it additionally exposes
 * the plain 4x4 matrix under the empty name, which surfaces under the parent's
 * name (e.g. ``"to_world"``). That matrix is the representation evaluated in
 * that case, and takes precedence if it is written together with the views.
 */
MI_VARIANT
class MI_EXPORT_LIB AnimatedTransform : public Object {
public:
    MI_IMPORT_CORE_TYPES()

    using FloatStorage = DynamicBuffer<Float>;

    /// Helper struct to store individual, decomposed key frames.
    struct Keyframe {
        ScalarVector3f S;
        ScalarVector3f H;
        ScalarQuaternion4f Q;
        ScalarVector3f T;

        std::string to_string() const {
            std::ostringstream oss;
            oss << "Keyframe[S=" << S << ", H=" << H << ", Q=" << Q << ", T=" << T << "]";
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
     * This method performs a vectorized interpolation between keyframes,
     * reading from the packed device buffer. Times outside of
     * ``get_time_bounds()`` are clamped to the first/last keyframe.
     */
    AffineTransform4f eval(Float time) const;

    /**
     * Scalar evaluation of the transformation
     *
     * This version is for use on the host (e.g., during AABB construction) and
     * reads the host-side keyframe list rather than the device buffer.
     */
    ScalarAffineTransform4f eval_scalar(ScalarFloat time) const;

    /// Check if the transformation is animated
    bool is_animated() const { return m_n_keyframes > 1; }

    /// Promote the single-keyframe matrix to an opaque JIT variable. This is 
    /// used to prevent baking of static transforms into JIT kernels.
    void make_transform_opaque() { dr::make_opaque(m_transform); }

    /// Returns the host-side keyframes of the animated transform.
    const std::vector<std::pair<ScalarFloat, Keyframe>> &keyframes() const {
        return m_keyframes;
    }

    /// Checks if JIT AD gradients are enabled on the parameter that is actually
    /// evaluated: the static transform when there is a single keyframe, and the
    /// packed keyframe buffer otherwise.
    bool parameters_grad_enabled() const {
        if (is_animated())
            return dr::grad_enabled(m_data);
        return dr::grad_enabled(m_transform.value());
    }

    /// Returns the time bounds of the animated transform.
    ScalarBoundingBox1f get_time_bounds() const;

    /// Returns the bounding box of the translation component of the animated
    /// transform.
    ScalarBoundingBox3f get_translation_bounds() const;

    /// Evaluates the spatial bounds of the animated transform over the given
    /// bounding box. This is used to compute the AABB of animated objects.
    /// Note: This is an approximation computed by sampling the transformation
    /// at regular intervals. It may not be perfectly conservative for highly
    /// non-linear motion.
    ScalarBoundingBox3f get_spatial_bounds(const ScalarBoundingBox3f &bbox) const;

    /// Checks if any keyframe has a scale component different from 1.
    bool has_scale() const;

    /// Checks if any keyframe has a non-zero shear component.
    bool has_shear() const;

    /// Checks if all keyframes are uniformly spaced in time. Raises an
    /// exception if this is not the case.
    void ensure_uniform_keyframes() const;

    void traverse(TraversalCallback *cb) override;

    void parameters_changed(const std::vector<std::string> &keys) override;

    std::string to_string() const override;

    MI_DECLARE_CLASS(AnimatedTransform)

protected:
    MI_TRAVERSE_CB(Object, m_transform, m_data, m_times, m_scale,
                   m_shear, m_rotation, m_translation)

private:
    void add_keyframe(ScalarFloat time, const ScalarAffineTransform4f &trafo);

    /// One-time initialization call that is used by constructors.
    void initialize();

    /// Repack ``m_data`` from the host-side ``m_keyframes``
    void pack_data();

    /// Rebuild the host-side ``m_keyframes`` from ``m_data``
    void unpack_data();

    /// Point the ``times``/``scale``/``shear``/``rotation``/``translation`` views at the
    /// current contents of ``m_data``
    void build_views();

    /// Rebuild ``m_data`` from the (user-written) views, validating that they
    /// agree on the number of keyframes
    void pack_views();

    field<AffineTransform4f, ScalarAffineTransform4f> m_transform;
    std::vector<std::pair<ScalarFloat, Keyframe>> m_keyframes;
    DynamicBuffer<Float> m_data;
    size_t m_n_keyframes = 1;

    /// Writable views into ``m_data``, see the class documentation
    TensorXf m_times, m_scale, m_shear, m_rotation, m_translation;
};

/// Packs a decomposed keyframe into ``out``, which must have room for
/// ``KeyframeStride`` floats.
template <typename ScalarFloat_, typename Keyframe_>
void pack_keyframe(ScalarFloat_ time, const Keyframe_ &kf, ScalarFloat_ *out) {
    out[0]  = time;
    out[1]  = kf.S.x();
    out[2]  = kf.S.y();
    out[3]  = kf.S.z();
    out[4]  = kf.H.x();
    out[5]  = kf.H.y();
    out[6]  = kf.H.z();
    out[7]  = 0.f; // padding
    out[8]  = kf.Q.x();
    out[9]  = kf.Q.y();
    out[10] = kf.Q.z();
    out[11] = kf.Q.w();
    out[12] = kf.T.x();
    out[13] = kf.T.y();
    out[14] = kf.T.z();
    out[15] = 0.f; // padding
}


/// Helper function to parse an AnimatedTransform from Properties.
template <typename Float, typename Spectrum>
ref<AnimatedTransform<Float, Spectrum>> parse_animated_transform(
    const Properties &props, const std::string &name = "to_world") {
    using AnimatedTransform4f = AnimatedTransform<Float, Spectrum>;
    using ScalarAffineTransform4f = typename AnimatedTransform4f::ScalarAffineTransform4f;

    if (props.has_property(name)) {
        if (props.type(name) == Properties::Type::Object) {
            ref<Object> obj = props.get<ref<Object>>(name);
            if (auto *anim = dynamic_cast<AnimatedTransform4f *>(obj.get())) {
                return anim;
            } else {
                Throw("Property '%s' must be a transformation or an "
                      "<animation> element, but a '%s' was given.",
                      name, obj->class_name());
            }
        } else {
            ScalarAffineTransform4f trafo = props.get<ScalarAffineTransform4f>(name);
            return new AnimatedTransform4f(trafo);
        }
    } else {
        return new AnimatedTransform4f();
    }
}

MI_EXTERN_CLASS(AnimatedTransform)
NAMESPACE_END(mitsuba)
