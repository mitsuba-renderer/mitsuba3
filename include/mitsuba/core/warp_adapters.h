#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/warp.h>

#include <vector>
#include <pcg32.h>

// TODO: needs sampler interface
using Sampler = pcg32;

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(warp)

/**
 * TODO: doc, purpose, why we use this design
 */
class MTS_EXPORT_CORE WarpAdapter {
public:

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

        Float clamp(Float value) const {
            return std::min(maxValue, std::max(minValue, value));
        }
    };

    // TODO: support construction with:
    // - No domain function (inferred from input / output types)
    // - Warping function that doesn't return a weight (then weight is 1.)
    WarpAdapter(const std::string &name, const std::vector<Argument> &arguments)
        : name_(name), arguments_(arguments) { }

    virtual Point2f samplePoint(Sampler * sampler, SamplingType strategy,
                                float invSqrtVal) const;

    virtual std::pair<Vector3f, Float> warpSample(const Point2f &sample) const = 0;

    /**
     * Writes out generated points into \p positions and associated weights
     * into \p weights. This method's job is mostly to package the results of
     * a point generation function into a general Eigen matrix.
     */
    virtual void generateWarpedPoints(Sampler *sampler, SamplingType strategy,
                                      size_t pointCount,
                                      Eigen::MatrixXf &positions,
                                      std::vector<Float> &weights) const = 0;

    virtual std::vector<double> generateObservedHistogram(Sampler *sampler,
        SamplingType strategy, size_t pointCount,
        size_t gridWidth, size_t gridHeight) const = 0;

    virtual std::vector<double> generateExpectedHistogram(size_t pointCount,
        size_t gridWidth, size_t gridHeight) const = 0;


    /// Returns true if the warping function is the identity function.
    virtual bool isIdentity() const { return false; };
    /// Returns the number of dimensions of the input domain.
    virtual size_t inputDimensionality() const = 0;
    /// Returns the number of dimensions of the ouput domain.
    virtual size_t domainDimensionality() const = 0;

    // TODO: more informative string representation
    virtual std::string toString() const { return name_; }

protected:
    virtual Float getPdfScalingFactor() const = 0;

    /// Human-readable name
    std::string name_;

    // TODO: actually, do we even need to know about those on the C++ side?
    std::vector<Argument> arguments_;
};

/// TODO: docs
class MTS_EXPORT_CORE PlaneWarpAdapter : public WarpAdapter {
public:
    using SampleType = Point2f;
    using DomainType = Point2f;
    using PairType = std::pair<DomainType, Float>;
    using WarpFunctionType = std::function<PairType (const SampleType&)>;
    using PdfFunctionType = std::function<Float (const DomainType&)>;

    PlaneWarpAdapter(const std::string &name,
                     const WarpFunctionType &f, const PdfFunctionType &pdf,
                     const std::vector<Argument> &arguments = {})
        : WarpAdapter(name, arguments), f_(f), pdf_(pdf) {
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

    virtual std::vector<double> generateExpectedHistogram(size_t pointCount,
        size_t gridWidth, size_t gridHeight) const override;

    virtual size_t inputDimensionality() const override { return 2; }
    virtual size_t domainDimensionality() const override { return 2; }

protected:
    virtual Float getPdfScalingFactor() const override {
        return 4.0;
    }

    /**
     * Maps a point on the warping function's output domain to a 2D point.
     * This is used when aggregating warped points into a 2D histogram.
     * For technical reasons, it always takes a 3D vector but may use only
     * some of it components.
     */
    // TODO: consider domainToBin directly instead
    virtual Point2f domainToPoint(const DomainType &v) const;

    /**
     * Maps a regularly sampled 2D point to the warping function's output domain.
     * This is used when querying the PDF function at various points of the domain.
     */
    virtual DomainType pointToDomain(const Point2f &p) const;

    /// Returns a list of warped points
    virtual std::vector<PairType> generatePoints(Sampler * sampler, SamplingType strategy, size_t pointCount) const;

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
     * PDF function. Will be called with a domain point and all arguments.
     * Should return the PDF associated with that point.
     */
    PdfFunctionType pdf_;
};


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
                           }) { }

    bool isIdentity() const override { return true; }

    // virtual std::vector<double> generateObservedHistogram(Sampler *sampler,
    //     SamplingType strategy, size_t pointCount,
    //     size_t gridWidth, size_t gridHeight) const override;

    // virtual std::vector<double> generateExpectedHistogram(size_t pointCount,
    //     size_t gridWidth, size_t gridHeight) const override;

protected:
    virtual Float getPdfScalingFactor() const override {
        return 1.0;
    }

    virtual Point2f domainToPoint(const DomainType &v) const override {
        return v;
    }
    virtual DomainType pointToDomain(const Point2f &p) const override {
        return p;
    }
};

NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
