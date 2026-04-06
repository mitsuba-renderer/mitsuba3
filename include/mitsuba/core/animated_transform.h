#pragma once

#include <map>

#include <drjit/quaternion.h>
#include <drjit/transform.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/field.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Animated transformation
 *
 * This struct stores a sequence of transformations and interpolates between them
 * using a combination of linear interpolation (for translation and scaling)
 * and spherical linear interpolation (for rotation).
 */
MI_VARIANT
struct MI_EXPORT_LIB AnimatedTransform : public Object {
public:
    MI_IMPORT_CORE_TYPES()

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
    AnimatedTransform(const ScalarAffineTransform4f &trafo);

    /// Initialize from a map of keyframes
    AnimatedTransform(const std::map<ScalarFloat, ScalarAffineTransform4f> &keyframes);

    /// Conversion constructor from another AnimatedTransform variant
    template <typename Float2, typename Spectrum2>
    AnimatedTransform(const AnimatedTransform<Float2, Spectrum2> &other) {
        for (auto const& [time, kf] : other.keyframes()) {
            m_keyframes[time] = {kf.S, kf.Q, kf.T};
        }
        initialize();
    }

    /**
     * \brief Evaluate the transformation at a specific time
     *
     * This method performs a vectorized interpolation between keyframes.
     */
    AffineTransform4f eval(Float time) const;

    /**
     * \brief Scalar evaluation of the transformation
     *
     * This version is for use on the host (e.g., during AABB construction).
     */
    ScalarAffineTransform4f eval_scalar(ScalarFloat time) const;

    /// Check if the transformation is animated
    bool is_animated() const { return m_keyframes.size() > 1; }

    /// Equality comparison operator
    bool operator==(const AnimatedTransform &other) const;

    /// Inequality comparison operator
    bool operator!=(const AnimatedTransform &other) const {
        return !operator==(other);
    }

    // Internal methods needed for hashing and bindings
    const std::map<ScalarFloat, Keyframe>& keyframes() const { return m_keyframes; }

    /// Returns the time bounds of the animated transform.
    ScalarBoundingBox1f get_time_bounds() const;

    /// Returns the bounding box of the translation component of the animated transform.
    ScalarBoundingBox3f get_translation_bounds() const;

    /// Evaluates the spatial bounds of the animated transform over the given bounding box.
    /// This is used to compute the AABB of animated objects.
    /// Note: This is an approximation computed by sampling the transformation at regular
    /// intervals. It may not be perfectly conservative for highly non-linear motion.
    ScalarBoundingBox3f get_spatial_bounds(const ScalarBoundingBox3f &bbox) const;

    /// Checks if any keyframe has a scale component different from 1.
    bool has_scale() const;

    /// Checks if all keyframes are uniformly spaced in time.
    void ensure_uniform_keyframes() const;

    void traverse(TraversalCallback *cb) override;

    void parameters_changed(const std::vector<std::string> &keys) override;

    std::string to_string() const override;

    MI_DECLARE_CLASS(AnimatedTransform)

protected:
    MI_TRAVERSE_CB(Object, m_transform, m_times, m_scales, m_translations, m_rotations)

private:
    void add_keyframe(ScalarFloat time, const ScalarAffineTransform4f &trafo);
    void initialize();

    field<AffineTransform4f, ScalarAffineTransform4f> m_transform;
    std::map<ScalarFloat, Keyframe> m_keyframes;
    DynamicBuffer<Float> m_times;
    DynamicBuffer<Vector3f> m_scales;
    DynamicBuffer<Quaternion4f> m_rotations;
    DynamicBuffer<Vector3f> m_translations;
};

MI_EXTERN_STRUCT(AnimatedTransform)
NAMESPACE_END(mitsuba)
