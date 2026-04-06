#pragma once

#if defined(__GNUG__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdouble-promotion"
#elif defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdouble-promotion"
#endif

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

    using Transform = mitsuba::Transform<Point4f, true>;
    using Matrix    = typename Transform::Matrix;

    using ScalarTransform = mitsuba::Transform<Point<ScalarFloat, 4>, true>;
    using ScalarMatrix    = typename ScalarTransform::Matrix;
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
    AnimatedTransform(const ScalarTransform &trafo);

    /// Initialize from a map of keyframes
    AnimatedTransform(const std::map<ScalarFloat, ScalarTransform> &keyframes);

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
    Transform eval(Float time) const;

    /**
     * \brief Scalar evaluation of the transformation
     *
     * This version is for use on the host (e.g., during AABB construction).
     */
    ScalarTransform eval_scalar(ScalarFloat time) const;

    /// Check if the transformation is animated
    bool is_animated() const {
        return m_keyframes.size() > 1;
    }

    /// Equality comparison operator
    bool operator==(const AnimatedTransform &other) const {
        if (m_keyframes.size() != other.m_keyframes.size())
            return false;

        auto it1 = m_keyframes.begin();
        auto it2 = other.m_keyframes.begin();
        for (; it1 != m_keyframes.end(); ++it1, ++it2) {
            if (it1->first != it2->first)
                return false;
            const auto &kf1 = it1->second;
            const auto &kf2 = it2->second;
            if (dr::any_nested(kf1.S != kf2.S) ||
                dr::any_nested(kf1.Q != kf2.Q) ||
                dr::any_nested(kf1.T != kf2.T))
                return false;
        }
        return true;
    }

    /// Inequality comparison operator
    bool operator!=(const AnimatedTransform &other) const {
        return !operator==(other);
    }

    // Internal methods needed for hashing and bindings
    const std::map<ScalarFloat, Keyframe>& keyframes() const { return m_keyframes; }

    /// Returns the time bounds of the animated transform.
    ScalarBoundingBox1f get_time_bounds() const {
        if (m_keyframes.empty())
            return {0.f, 0.f};
        return {m_keyframes.begin()->first, m_keyframes.rbegin()->first};
    }

    /// Returns the bounding box of the translation component of the animated transform.
    ScalarBoundingBox3f get_translation_bounds() const {
        ScalarBoundingBox3f bbox;
        for (auto const& [time, kf] : m_keyframes) {
            bbox.expand(ScalarPoint3f(kf.T));
        }
        return bbox;
    }

    /// Evaluates the spatial bounds of the animated transform over the given bounding box.
    /// This is used to compute the AABB of animated objects.
    ScalarBoundingBox3f get_spatial_bounds(const ScalarBoundingBox3f &bbox) const {
        if (m_keyframes.empty()) {
            return bbox;
        }
        ScalarBoundingBox3f res;
        if (m_keyframes.size() <= 1) {
            auto trafo = eval_scalar(m_keyframes.begin()->first);
            for (int j = 0; j < 8; ++j)
                res.expand(trafo * bbox.corner(j));
        } else {
            int n_steps = 100;
            ScalarBoundingBox1f time_bounds = get_time_bounds();
            ScalarFloat step = time_bounds.extents()[0] / (n_steps - 1);
            for (int i = 0; i < n_steps; ++i) {
                ScalarFloat t = time_bounds.min[0] + step * i;
                ScalarTransform trafo = eval_scalar(t);
                for (int j = 0; j < 8; ++j)
                    res.expand(trafo * bbox.corner(j));
            }
        }
        return res;
    }

    /// Checks if any keyframe has a scale component different from 1.
    bool has_scale() const {
        for (auto const& [time, kf] : m_keyframes) {
            if (dr::any_nested(dr::abs(kf.S - ScalarVector3f(1.f)) > 1e-3f))
                return true;
        }
        return false;
    }

    void traverse(TraversalCallback *cb) override {
        if (m_keyframes.size() == 1) {
            cb->put("transform",     m_transform,    ParamFlags::Differentiable);
        }
        cb->put("times",         m_times,        ParamFlags::Differentiable);
        cb->put("scales",        m_scales,       ParamFlags::Differentiable);
        cb->put("translations",  m_translations, ParamFlags::Differentiable);
        cb->put("rotations",     m_rotations,    ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys) override;

    std::string to_string() const override {
        std::ostringstream oss;
        oss << class_name() << "[";
        if (!m_keyframes.empty()) {
            oss << std::endl;
        }
        for (auto const& [time, kf] : m_keyframes) {
            oss << "  " << time << ": " << kf.to_string() << "," << std::endl;
        }
        oss << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(AnimatedTransform)

protected:
    MI_TRAVERSE_CB(Object, m_transform, m_times, m_scales, m_translations, m_rotations)

private:

    void add_keyframe(ScalarFloat time, const ScalarTransform &trafo);
    void initialize();

    field<Transform, ScalarTransform> m_transform;
    std::map<ScalarFloat, Keyframe> m_keyframes;
    DynamicBuffer<Float> m_times;
    DynamicBuffer<Vector3f> m_scales;
    DynamicBuffer<Quaternion4f> m_rotations;
    DynamicBuffer<Vector3f> m_translations;
};

MI_EXTERN_STRUCT(AnimatedTransform)
NAMESPACE_END(mitsuba)

#if defined(__GNUG__)
#  pragma GCC diagnostic pop
#elif defined(__clang__)
#  pragma clang diagnostic pop
#endif
