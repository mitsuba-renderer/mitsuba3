#pragma once
#include <mitsuba/mitsuba.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/warp.h>

#include <vector>
#include <pcg32.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(warp)

// TODO: use the actual Sampler interface when it is available
using Sampler = pcg32;

// Forward declaration of helper class.
NAMESPACE_BEGIN(detail)
template <typename Adapter, typename SampleType, typename DomainType>
class WarpAdapterHelper;
NAMESPACE_END(detail)

/**
 * TODO: doc, purpose, why we use this design
 *
 * \note In practice, most implementations are delegated to a \c WarpHelper
 * class nested in the \c detail namespace.
 */
class MTS_EXPORT_CORE WarpAdapter {
public:
    /// Bounding box corresponding to the first quadrant ([0..1]^n)
    static const BoundingBox3f kUnitSquareBoundingBox;
    /// Bounding box corresponding to a disk of radius 1 centered at the origin ([-1..1]^n)
    // TODO: better name
    static const BoundingBox3f kCenteredSquareBoundingBox;

public:

    /**
     * Represents a single parameter to a warping function, including its
     * formal name, a human-readable description and a domain of valid values.
     * Only Float arguments are supported.
     * This description is then used to automatically test warping functions
     * for different combinations of their parameters.
     *
     * \param _name Should match the formal parameter name to the warping and
     *              PDF functions, since the argument may be passed as a
     *              keyword argument in Python.
     */
    // TODO: this could actually be Python-only
    struct Argument {
        Argument(const std::string &_name,
                 Float _minValue = 0.0, Float _maxValue = 1.0,
                 Float _defaultValue = 0.0,
                 const std::string &_description = "")
            : name(_name), minValue(_minValue), maxValue(_maxValue)
            , description(_description.length() <= 0 ? _name : _description) {
                defaultValue = clamp(_defaultValue);
            }

        /// Formal name of the parameter
        std::string name;
        /// Range and default value for the parameter
        Float minValue, maxValue, defaultValue;
        /// Human-readable description of the parameter
        std::string description;

        /// Returns \p value (in [0..1]) mapped to this argument's range
        Float map(Float value) const {
            return value * (maxValue - minValue) + minValue;
        }

        /// Returns \p value (in [minValue..maxValue]), mapped to [0..1]
        Float normalize(Float value) const {
            return  (value - minValue) / (maxValue - minValue);
        }

        /// Clamps \p value to the authorized range of this argument
        Float clamp(Float value) const {
            return std::min(maxValue, std::max(minValue, value));
        }
    };

    // TODO: support construction with:
    // - Warping function that doesn't return a weight (then weight is 1.)
    // TODO: actually we don't really need to know about the arguments here.
    WarpAdapter(const std::string &name, const std::vector<Argument> &arguments,
                const BoundingBox3f bbox)
        : name_(name), arguments_(arguments), bbox_(bbox) { }

    /**
     * Warps a \c Point2f sample (sampled uniformly on the unit square) to a
     * \c Vector3f. If the warping function outputs 2D or 1D points, the
     * remaining entries of the results are left undefined or set to 0.0.
     *
     * \return Pair (warped point, weight)
     */
    virtual std::pair<Vector3f, Float> warpSample(const Point2f &sample) const = 0;

    /**
     * Writes out generated points into \p positions and associated weights
     * into \p weights. This method's role is mostly to package the results of
     * a point generation function into a general Eigen matrix.
     */
    virtual void generateWarpedPoints(Sampler *sampler, SamplingType strategy,
                                      size_t pointCount,
                                      Eigen::MatrixXf &positions,
                                      std::vector<Float> &weights) const = 0;

    /**
     * Given a sampler, sampling strategy and histogram description, generates
     * random samples and bins them to a 2D histogram.
     * This involves mapping the warped points from the output domain of the
     * warping function back onto the unit square where the bins are defined
     * (\see \r domainToPoint).
     *
     * @return An unrolled vector of \p gridWidth times \p gridHeight histogram
     *         bin values.
     */
    virtual std::vector<double> generateObservedHistogram(Sampler *sampler,
        SamplingType strategy, size_t pointCount,
        size_t gridWidth, size_t gridHeight) const = 0;

    /**
     * Sampling the PDF over the warping functions output domain, generates
     * the expected histogram of the warping function. It can then be compared
     * to the observed histogram.
     * This involves mapping regular 2D grid points into the output domain
     * of the warping function (\see \r pointToDomain).
     *
     * @return An unrolled vector of \p gridWidth times \p gridHeight histogram
     *         bin values.
     */
    virtual std::vector<double> generateExpectedHistogram(size_t pointCount,
        size_t gridWidth, size_t gridHeight) const;


    /// Returns true if the warping function is the identity function.
    virtual bool isIdentity() const { return false; };
    /// Returns the number of dimensions of the input domain.
    virtual size_t inputDimensionality() const = 0;
    /// Returns the number of dimensions of the ouput domain.
    virtual size_t domainDimensionality() const = 0;

    // TODO: more informative string representation
    virtual std::string toString() const { return name_; }

protected:
    /// TODO: docs
    virtual Float getPdfScalingFactor() const = 0;

    /// TODO: docs
    virtual std::function<Float (double, double)> getPdfIntegrand() const = 0;

    /**
     * Maps a point on the warping function's output domain to a 2D point in [0..1]^2.
     * This is used when aggregating warped points into a 2D histogram.
     * For technical reasons, it always takes a 3D vector but may use only
     * some of it components.
     */
    template <typename DomainType> inline
    Point2f domainToPoint(const DomainType &v) const;

    /**
     * Maps a regularly sampled 2D point to the warping function's output domain.
     * This is used when querying the PDF function at various points of the domain.
     */
    template <typename DomainType> inline
    DomainType pointToDomain(const Point2f &p) const;

    /// Human-readable name
    std::string name_;

    // TODO: actually, we may not even need to know about those on the C++ side.
    std::vector<Argument> arguments_;

    /// Bounding box of the output domain (may not use all 3 components of the points)
    const BoundingBox3f bbox_;
};

// Template specializations
// TODO: could probably be made more efficient
// TODO: these assume bounding boxes centered around zero
template <> inline
Point2f WarpAdapter::domainToPoint<Float>(const Float &v) const {
    return Point2f(
        ((Float)1.0 / bbox_.extents().x()) * (v - bbox_.min.x()),
        0.0);
}
template <> inline
Point2f WarpAdapter::domainToPoint<Point2f>(const Point2f &v) const {
    const auto &delta = bbox_.extents();
    return Point2f(
        ((Float)1.0 / delta.x()) * (v.x() - bbox_.min.x()),
        ((Float)1.0 / delta.y()) * (v.y() - bbox_.min.y()));
}
template <> inline
Point2f WarpAdapter::domainToPoint<Vector3f>(const Vector3f &v) const {
    Point2f p;
    // TODO: this assumes a bounding box of [-1..1]^3, generalize
    p[0] = std::atan2(v.y(), v.x()) * math::InvTwoPi;
    if (p[0] < 0) p[0] += 1;
    p[1] = 0.5f * v.z() + 0.5f;
    return p;
}

template <> inline
Float WarpAdapter::pointToDomain(const Point2f &p) const {
    return bbox_.extents().x() * p.x() + bbox_.min.x();
}
template <> inline
Point2f WarpAdapter::pointToDomain(const Point2f &p) const {
    const auto &delta = bbox_.extents();
    return Point2f(
        delta.x() * p.x() + bbox_.min.x(),
        delta.y() * p.y() + bbox_.min.y());
}
template <> inline
Vector3f WarpAdapter::pointToDomain(const Point2f &p) const {
    // TODO: this assumes a bounding box of [-1..1]^3, generalize
    const Float x = 2 * math::Pi * p.x();
    const Float y = 2 * p.y() - 1;
    Float sinTheta = std::sqrt(1 - y * y);
    Float sinPhi, cosPhi;
    math::sincos(x, &sinPhi, &cosPhi);

    return Vector3f(
        sinTheta * cosPhi,
        sinTheta * sinPhi,
        y);
}
// TODO: avoid multiple repeated declarations

/// TODO: docs
/// TODO: only uses the first coordinate from the 2D samples, which is wasteful.
class MTS_EXPORT_CORE LineWarpAdapter : public WarpAdapter {
public:
    using SampleType = Float;
    using DomainType = Float;
    using PairType = std::pair<DomainType, Float>;
    using WarpFunctionType = std::function<PairType (const SampleType&)>;
    using PdfFunctionType = std::function<Float (const DomainType&)>;
    friend class detail::WarpAdapterHelper<LineWarpAdapter, SampleType, DomainType>;
    using Helper = detail::WarpAdapterHelper<LineWarpAdapter, SampleType, DomainType>;

    LineWarpAdapter(const std::string &name,
                    const WarpFunctionType &f, const PdfFunctionType &pdf,
                    const std::vector<Argument> &arguments = {},
                    const BoundingBox3f &bbox = WarpAdapter::kUnitSquareBoundingBox)
        : WarpAdapter(name, arguments, bbox), f_(f), pdf_(pdf) {
    }

    virtual std::pair<Vector3f, Float>
    warpSample(const Point2f& sample) const override;

    virtual void generateWarpedPoints(Sampler *sampler, SamplingType strategy,
                                      size_t pointCount,
                                      Eigen::MatrixXf &positions,
                                      std::vector<Float> &weights) const override;

    virtual std::vector<double> generateObservedHistogram(Sampler *sampler,
        SamplingType strategy, size_t pointCount,
        size_t gridWidth, size_t gridHeight) const override;

    virtual size_t inputDimensionality() const override { return 1; }
    virtual size_t domainDimensionality() const override { return 1; }

protected:
    virtual std::function<Float (double, double)> getPdfIntegrand() const override;

    virtual Float getPdfScalingFactor() const override {
        return (Float)1.0;  // TODO: check
    }

    /// Returns a list of warped points
    virtual std::vector<PairType> generatePoints(Sampler * sampler, SamplingType strategy, size_t &pointCount) const;

    virtual std::vector<double> binPoints(const std::vector<PairType> &points,
        size_t gridWidth, size_t gridHeight) const;


    /// TODO: doc
    virtual PairType warp(SampleType p) const {
        return f_(p);
    }

    /// TODO: doc
    virtual Float pdf(DomainType p) const {
        return pdf_(p);
    }

    /**
     * Warping function.
     * Will be called with the sample only, so any parameter needs to be bound
     * in advance.
     * Returns a pair (warped point on the domain; weight).
     */
    WarpFunctionType f_;
    /**
     * PDF function.
     * Will be called with a domain point only, so any parameter needs to be
     * bound in advance.
     * Should return the PDF associated with that point.
     */
    PdfFunctionType pdf_;
};

/// TODO: docs
class MTS_EXPORT_CORE PlaneWarpAdapter : public WarpAdapter {
public:
    using SampleType = Point2f;
    using DomainType = Point2f;
    using PairType = std::pair<DomainType, Float>;
    using WarpFunctionType = std::function<PairType (const SampleType&)>;
    using PdfFunctionType = std::function<Float (const DomainType&)>;
    friend class detail::WarpAdapterHelper<PlaneWarpAdapter, SampleType, DomainType>;
    using Helper = detail::WarpAdapterHelper<PlaneWarpAdapter, SampleType, DomainType>;


    PlaneWarpAdapter(const std::string &name,
                     const WarpFunctionType &f, const PdfFunctionType &pdf,
                     const std::vector<Argument> &arguments = {},
                     const BoundingBox3f &bbox = WarpAdapter::kCenteredSquareBoundingBox)
        : WarpAdapter(name, arguments, bbox), f_(f), pdf_(pdf) {
    }

    virtual std::pair<Vector3f, Float>
    warpSample(const Point2f& sample) const override;

    virtual void generateWarpedPoints(Sampler *sampler, SamplingType strategy,
                                      size_t pointCount,
                                      Eigen::MatrixXf &positions,
                                      std::vector<Float> &weights) const override;

    virtual std::vector<double> generateObservedHistogram(Sampler *sampler,
        SamplingType strategy, size_t pointCount,
        size_t gridWidth, size_t gridHeight) const override;

    virtual size_t inputDimensionality() const override { return 2; }
    virtual size_t domainDimensionality() const override { return 2; }

protected:
    virtual std::function<Float (double, double)> getPdfIntegrand() const override;

    virtual Float getPdfScalingFactor() const override {
        return 4.0;
    }

    /// Returns a list of warped points
    virtual std::vector<PairType> generatePoints(Sampler * sampler, SamplingType strategy, size_t &pointCount) const;

    virtual std::vector<double> binPoints(const std::vector<PairType> &points,
        size_t gridWidth, size_t gridHeight) const;


    /// TODO: doc
    virtual PairType warp(SampleType p) const {
        return f_(p);
    }

    /// TODO: doc
    virtual Float pdf(DomainType p) const {
        return pdf_(p);
    }

    /**
     * Warping function.
     * Will be called with the sample only, so any parameter needs to be bound
     * in advance.
     * Returns a pair (warped point on the domain; weight).
     */
    WarpFunctionType f_;
    /**
     * PDF function.
     * Will be called with a domain point only, so any parameter needs to be
     * bound in advance.
     * Should return the PDF associated with that point.
     */
    PdfFunctionType pdf_;
};

/// TODO: docs
class MTS_EXPORT_CORE IdentityWarpAdapter : public PlaneWarpAdapter {
public:
    using SampleType = Point2f;
    using DomainType = Point2f;

    IdentityWarpAdapter()
        : PlaneWarpAdapter("Identity",
                           [](const SampleType& s) {
                               return std::make_pair(s, 1.0);
                           },
                           [](const DomainType& p) {
                               return (p.x() >= 0 && p.x() <= 1
                                    && p.y() >= 0 && p.y() <= 1) ? 1.0 : 0.0;
                           },
                           {} /* No arguments to warping function */,
                           kUnitSquareBoundingBox) { }

    bool isIdentity() const override { return true; }

protected:
    virtual Float getPdfScalingFactor() const override {
        return 1.0;
    }
};


/// TODO: docs
class MTS_EXPORT_CORE SphereWarpAdapter : public WarpAdapter {
public:
    using SampleType = Point2f;
    using DomainType = Vector3f;
    using PairType = std::pair<DomainType, Float>;
    using WarpFunctionType = std::function<PairType (const SampleType&)>;
    using PdfFunctionType = std::function<Float (const DomainType&)>;
    friend class detail::WarpAdapterHelper<SphereWarpAdapter, SampleType, DomainType>;
    using Helper = detail::WarpAdapterHelper<SphereWarpAdapter, SampleType, DomainType>;

    SphereWarpAdapter(const std::string &name,
                      const WarpFunctionType &f, const PdfFunctionType &pdf,
                      const std::vector<Argument> &arguments = {},
                      const BoundingBox3f &bbox = WarpAdapter::kCenteredSquareBoundingBox)
        : WarpAdapter(name, arguments, bbox), f_(f), pdf_(pdf) {
    }

    virtual std::pair<Vector3f, Float>
    warpSample(const Point2f& sample) const override;

    virtual void generateWarpedPoints(Sampler *sampler, SamplingType strategy,
                                      size_t pointCount,
                                      Eigen::MatrixXf &positions,
                                      std::vector<Float> &weights) const override;

    virtual std::vector<double> generateObservedHistogram(Sampler *sampler,
        SamplingType strategy, size_t pointCount,
        size_t gridWidth, size_t gridHeight) const override;

    virtual size_t inputDimensionality() const override { return 2; }
    virtual size_t domainDimensionality() const override { return 3; }

protected:
    virtual std::function<Float (double, double)> getPdfIntegrand() const override;

    virtual Float getPdfScalingFactor() const override {
        return (Float)4.0 * math::Pi;
    }

    /// Returns a list of warped points
    virtual std::vector<PairType> generatePoints(Sampler * sampler, SamplingType strategy, size_t &pointCount) const;

    virtual std::vector<double> binPoints(const std::vector<PairType> &points,
        size_t gridWidth, size_t gridHeight) const;


    /// TODO: doc
    virtual PairType warp(SampleType p) const {
        return f_(p);
    }

    /// TODO: doc
    virtual Float pdf(DomainType p) const {
        return pdf_(p);
    }

    /**
     * Warping function.
     * Will be called with the sample only, so any parameter needs to be bound
     * in advance.
     * Returns a pair (warped point on the domain; weight).
     */
    WarpFunctionType f_;
    /**
     * PDF function.
     * Will be called with a domain point only, so any parameter needs to be
     * bound in advance.
     * Should return the PDF associated with that point.
     */
    PdfFunctionType pdf_;
};

NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
