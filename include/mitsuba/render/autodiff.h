#pragma once

#include <mitsuba/render/fwd.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/simd.h>

NAMESPACE_BEGIN(mitsuba)

#if defined(MTS_ENABLE_AUTODIFF)
/**
 * \brief Container for differentiable scene parameters
 *
 * This data structure stores a list of differentiable scene parameters.
 * Mitsuba objects (e.g. BRDFs, textures, etc.) can deposit information about
 * differentiable scene parameters (via \ref put()) that are then accessible in
 * an optimization context (e.g. via PyTorch).
 */
class MTS_EXPORT_RENDER DifferentiableParameters : public Object {
    struct Details;
public:
    DifferentiableParameters();
    DifferentiableParameters(const DifferentiableParameters &other);
    DifferentiableParameters &operator=(const DifferentiableParameters &) = delete;

    /// Set a prefix that will be appended to subsequent \put() calls
    void set_prefix(const std::string &prefix);

    /// Record a reference to a differentiable parameter
    template <typename T, enable_if_diff_array_t<T> = 0>
    void put(DifferentiableObject *obj, const std::string &name, T &value) {
        if constexpr (array_depth_v<T> == 1)
            put(obj, name, (void *) &value, 1);
        else if constexpr (array_depth_v<T> == 2)
            put(obj, name, (void *) &value, array_size_v<T>);
        else if constexpr (array_depth_v<T> == 3)
            put(obj, name, (void *) &value, array_size_v<T> * array_size_v<value_t<T>>);
        else
            throw std::runtime_error("DifferentiableParameters::put(): unsupported array type!");
    }

    /// Return a human-readable string representation
    std::string to_string() const override;

    MTS_DECLARE_CLASS()
protected:
    virtual ~DifferentiableParameters();

    void put(DifferentiableObject *obj, const std::string &name,
             void *ptr, size_t dim);

protected:
    std::unique_ptr<Details> d;
};

/**
 * This macro generates a function (non-const & const) of name 'name()'' that
 * either returns the attribute 'attr' or 'attr_d' depending on whether the
 * function's template parameter is a differentiable array.
 *
 * It is used to introduce differentiable renderer parameters without affecting
 * performance in the normal case.
 */
#define MTS_AUTODIFF_GETTER(value, attr, attr_d)                               \
    template <typename Value> MTS_INLINE auto &value() {                       \
        if constexpr (is_diff_array_v<Value>)                                  \
            return attr_d;                                                     \
        else                                                                   \
            return attr;                                                       \
    }                                                                          \
    template <typename Value> MTS_INLINE const auto &value() const {           \
        if constexpr (is_diff_array_v<Value>)                                  \
            return attr_d;                                                     \
        else                                                                   \
            return attr;                                                       \
    }

#else // !MTS_ENABLE_AUTODIFF

#define MTS_AUTODIFF_GETTER(value, attr, attr_d)                               \
    template <typename Value> MTS_INLINE auto &value() { return attr; }        \
    template <typename Value> MTS_INLINE const auto &value() const {           \
        return attr;                                                           \
    }

#endif // MTS_ENABLE_AUTODIFF

/**
 * \brief Generic object with differentiable parameters
 *
 * This class extends the basic \ref Object interface with a method that can be
 * used to enumerate differentiable model parameters.
 */
class MTS_EXPORT_RENDER DifferentiableObject : public Object {
public:
#if defined(MTS_ENABLE_AUTODIFF)
    ENOKI_MANAGED_OPERATOR_NEW()

    /// Register all differentiable parameters with the container \c dp
    virtual void put_parameters(DifferentiableParameters &dp);
#endif

    /// Update internal data structures after applying changes to parameters
    virtual void parameters_changed();

    MTS_DECLARE_CLASS()
};

NAMESPACE_END(mitsuba)
