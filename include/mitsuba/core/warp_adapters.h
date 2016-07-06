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

    /// Only supports Float type argument
    struct Argument {
        Argument(std::string name, Float minValue, Float maxValue, std::string description = "")
            : name(name), minValue(minValue), maxValue(maxValue)
            , description(description.length() <= 0 ? name : description) {}

        /// Formal name of the parameter
        std::string name;
        /// Range for the parameters
        Float minValue, maxValue;
        /// Human-readable description of the parameter
        std::string description;
    };

    // TODO: support construction with:
    // - No domain function (inferred from input / output types)
    // - Warping function that doesn't return a weight (then weight is 1.)
    WarpAdapter(std::string name, const std::vector<Argument> &arguments)
        : name_(name), arguments_(arguments) {
    }

    virtual Point2f samplePoint(Sampler * sampler, SamplingType strategy,
                                float invSqrtVal) const;

    virtual std::pair<Vector3f, Float> warpSample(const Point2f& sample) const = 0;

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
    std::string toString() { return name_; }

protected:
    /// Human-readable name
    std::string name_;

    // TODO: actually we shouldn't care about this, the user writes an adapter.
    // std::function<SampleType (SampleType)> sampleToInputDomain;

    std::vector<Argument> arguments_;
};

class MTS_EXPORT_CORE PlaneWarpAdapter : public WarpAdapter {
protected:
    static constexpr Float kPdfScalingFactor = 4.0;

public:
    using SampleType = Point2f;
    using DomainType = Point2f;
    using WarpFunctionType = std::function<std::pair<DomainType, Float> (const SampleType&)>;
    using PdfFunctionType = std::function<Float (const DomainType&)>;

    PlaneWarpAdapter(const std::string &name,
                     const WarpFunctionType &f, const PdfFunctionType &pdf,
                     const std::vector<Argument> &arguments = {})
        : WarpAdapter(name, arguments), f_(f), pdf_(pdf) { }

    // PlaneWarpAdapter(const std::string &name,
    //                 const std::function<DomainType (const SampleType&)> &f,
    //                 const PdfFunctionType &pdf,
    //                 const std::vector<Argument> &arguments = {})
    //     : WarpAdapter(name, arguments), pdf_(pdf) {

    //     Log(EInfo, "Lazy constructor called");
    //     f_ = [f](const SampleType &p) {
    //         return std::make_pair(f(p), 1.0);
    //     };
    // }

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

    virtual std::vector<DomainType> generatePoints(Sampler * sampler, SamplingType strategy, size_t pointCount) const;

    virtual std::vector<double> binPoints(const std::vector<DomainType> &points,
        size_t gridWidth, size_t gridHeight) const;


    /// TODO: doc
    virtual std::pair<DomainType, Float> warp(SampleType p) const {
        return f_(p);
    }

    /// TODO: doc
    virtual Float pdf(DomainType p) const {
        return pdf_(p);
    }

    /**
     * Warping function.
     * Will be called with the sample only, so any parameter will be bound
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
protected:
    static constexpr Float kPdfScalingFactor = 1.0;

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

    // const std::string &name,
    // const WarpFunctionType &f, const PdfFunctionType &pdf,
    // const std::vector<Argument> &arguments = {}

    bool isIdentity() const override { return true; }

    // virtual std::vector<double> generateObservedHistogram(Sampler *sampler,
    //     SamplingType strategy, size_t pointCount,
    //     size_t gridWidth, size_t gridHeight) const override;

    // virtual std::vector<double> generateExpectedHistogram(size_t pointCount,
    //     size_t gridWidth, size_t gridHeight) const override;

protected:
    virtual Point2f domainToPoint(const DomainType &v) const override {
        return v;
    }
    virtual DomainType pointToDomain(const Point2f &p) const override {
        return p;
    }
};

NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
