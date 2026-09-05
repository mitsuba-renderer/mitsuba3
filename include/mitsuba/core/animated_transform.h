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
/// The size is padded to 12 so we can read the data with 3 aligned Vector4f
/// gathers.
constexpr uint32_t KeyframeStride = 12;

/**
 * Animated transformation
 *
 * This class stores a sequence of transformations and interpolates between
 * them using a combination of linear interpolation (for translation and
 * scaling) and spherical linear interpolation (for rotation).
 *
 * Internally, keyframes are packed into a single DynamicBuffer ``m_data`` with
 * a stride of ``KeyframeStride`` floats per keyframe to optimize vectorized
 * loads. The layout per keyframe is:
 *
 * ``[time, scale.x, scale.y, scale.z, quat.x, quat.y, quat.z, quat.w, trans.x,
 * trans.y, trans.z, unused]``
 *
 * The class keeps two redundant representations of the same animation: the
 * device-side buffer ``m_data`` (read by ``eval()``) and the host-side keyframe
 * list ``m_keyframes`` (read by ``eval_scalar()`` and ``keyframes()``). They
 * are synchronized by the constructors and by ``parameters_changed()``.
 *
 * Through ``mitsuba::traverse()`` the transformation always exposes ``"data"``,
 * and additionally ``"transform"`` (the plain 4x4 matrix) while it holds a
 * single keyframe. That matrix is the representation evaluated in that case,
 * and takes precedence if both parameters are written. Writing ``"data"`` can
 * change the number of keyframes and hence the set of exposed parameters, so
 * ``mitsuba::traverse()`` has to be called again afterwards.
 *
 * Writing ``"data"`` means writing the packed layout above by hand. Its
 * invariants are not re-checked and are the caller's responsibility: the width
 * is a multiple of ``KeyframeStride``, keyframe times strictly increase, and
 * consecutive quaternions lie on the same hemisphere.
 */
MI_VARIANT
class MI_EXPORT_LIB AnimatedTransform : public Object {
public:
    MI_IMPORT_CORE_TYPES()

    using FloatStorage = DynamicBuffer<Float>;

    /// Helper struct to store individual, decomposed key frames.
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
    AnimatedTransform(const ScalarAffineTransform4f &trafo);

    /// Initialize from a map of keyframes
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
    bool is_animated() const { return m_keyframes.size() > 1; }

    /**
     * Promote the single-keyframe matrix to an opaque JIT variable
     *
     * Callers that actually evaluate ``to_world`` at render time invoke this so
     * that the matrix does not get baked into the kernel (which would force a
     * recompilation whenever it changes). Shapes that instead bake the
     * transformation into their geometry (meshes, curves) skip it. A no-op for
     * animated transformations, whose keyframes already live in ``m_data``.
     */
    void make_transform_opaque() { dr::make_opaque(m_transform); }

    /// Equality comparison operator
    bool operator==(const AnimatedTransform &other) const;

    /// Inequality comparison operator
    bool operator!=(const AnimatedTransform &other) const {
        return !operator==(other);
    }

    /// Returns the host-allocated keyframes of the animated transform.
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
    ScalarBoundingBox3f
    get_spatial_bounds(const ScalarBoundingBox3f &bbox) const;

    /// Checks if any keyframe has a scale component different from 1.
    bool has_scale() const;

    /// Checks if all keyframes are uniformly spaced in time. Raises an
    /// exception if this is not the case.
    void ensure_uniform_keyframes() const;

    /// Row count of the keyframe views (i.e. the number of keyframes)
    size_t keyframe_count() const { return m_keyframes.size(); }

    void traverse(TraversalCallback *cb) override;

    void parameters_changed(const std::vector<std::string> &keys) override;

    std::string to_string() const override;

    MI_DECLARE_CLASS(AnimatedTransform)

protected:
    MI_TRAVERSE_CB(Object, m_transform, m_data, m_times, m_scale,
                   m_rotation, m_translation)

private:
    void add_keyframe(ScalarFloat time, const ScalarAffineTransform4f &trafo);

    /// One-time initialization call that is used by constructors.
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

    field<AffineTransform4f, ScalarAffineTransform4f> m_transform;
    std::vector<std::pair<ScalarFloat, Keyframe>> m_keyframes;
    DynamicBuffer<Float> m_data;

    /// Writable views into ``m_data``, see the class documentation
    TensorXf m_times, m_scale, m_rotation, m_translation;

    /// Recorded as the keyframes are built, since the decomposition in
    /// ``m_keyframes`` discards the off-diagonal scale terms.
    bool m_sheared = false;
};

/// Packs a decomposed keyframe into ``out``, which must have room for
/// ``KeyframeStride`` floats. Shared by ``AnimatedTransform::initialize()``
/// and by the per-instance keyframe buffers built in ``Scene``, so that the
/// layout documented above is defined in exactly one place.
template <typename ScalarFloat_, typename Keyframe_>
void pack_keyframe(ScalarFloat_ time, const Keyframe_ &kf, ScalarFloat_ *out) {
    out[0]  = time;
    out[1]  = kf.S.x();
    out[2]  = kf.S.y();
    out[3]  = kf.S.z();
    out[4]  = kf.Q.x();
    out[5]  = kf.Q.y();
    out[6]  = kf.Q.z();
    out[7]  = kf.Q.w();
    out[8]  = kf.T.x();
    out[9]  = kf.T.y();
    out[10] = kf.T.z();
    out[11] = 0.f; // padding, see KeyframeStride
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
        return new AnimatedTransform4f(ScalarAffineTransform4f());
    }
}

MI_EXTERN_CLASS(AnimatedTransform)
NAMESPACE_END(mitsuba)
