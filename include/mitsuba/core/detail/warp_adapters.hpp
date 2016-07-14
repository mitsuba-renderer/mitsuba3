#pragma once

#include <mitsuba/core/vector.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(warp)
NAMESPACE_BEGIN(detail)

/**
 * This template class provides appropriate helper functions for all required
 * \c WarpAdapter methods to be implemented by subclasses.
 * These implementations are refactored to this helper class to avoid repetition
 * in \c WarpAdapter subclasses, which are now reduced to a simple call to
 * the appropriate helper.
 * TODO: any way to achieve the same goal without having to explicitly delegate
 *       each method to the helper class?
 */
template <typename Adapter, typename SampleType, typename DomainType>
class WarpAdapterHelper {
public:
    using PairType = typename Adapter::PairType;
    using VectorE = Eigen::Matrix<float, 3, 1>;

    static inline Vector3f toVector3f(const Float &p) {
        return Vector3f(p, 0.0, 0.0);
    }
    static inline Vector3f toVector3f(const Point2f &p) {
        return Vector3f(p.x(), p.y(), 0.0);
    }
    static inline Vector3f toVector3f(const Vector3f &p) {
        return Vector3f(p.x(), p.y(), p.z());
    }

    static inline SampleType toSampleType(const Point2f &p) {
        return p;
    }

    static std::pair<Vector3f, Float> warpSample(const Adapter *adapter,
        const Point2f& sample) {
        DomainType p;
        Float w;
        std::tie(p, w) = adapter->warp(sample);
        return std::make_pair(toVector3f(p), w);
    }

    static void generateWarpedPoints(const Adapter *adapter,
        Sampler *sampler, SamplingType strategy, size_t pointCount,
        Eigen::MatrixXf &positions, std::vector<Float> &weights) {

        const auto points = adapter->generatePoints(sampler, strategy, pointCount);

        positions.resize(3, points.size());
        weights.resize(points.size());
        for (size_t i = 0; i < points.size(); ++i) {
            const auto &p = points[i];
            positions.col(i) << static_cast<VectorE>(toVector3f(p.first));
            weights[i] = p.second;
        }
    }

    static std::vector<double> generateObservedHistogram(const Adapter *adapter,
        Sampler *sampler, SamplingType strategy,
        size_t pointCount, size_t gridWidth, size_t gridHeight) {

        const auto points = adapter->generatePoints(sampler, strategy, pointCount);
        return adapter->binPoints(points, gridWidth, gridHeight);
    }

    static std::function<Float (double, double)> getPdfIntegrand(const Adapter *adapter) {
        return [adapter](double y, double x) {
            return adapter->pdf(adapter->template pointToDomain<DomainType>(Point2f(x, y)));
        };
    }

    static std::vector<PairType> generatePoints(const Adapter *adapter,
        Sampler * sampler, SamplingType strategy, size_t &pointCount) {

        size_t sqrtVal = static_cast<size_t>(std::sqrt((float) pointCount) + 0.5f);
        float invSqrtVal = 1.f / sqrtVal;
        if (strategy == Grid || strategy == Stratified)
            pointCount = sqrtVal * sqrtVal;

        std::vector<PairType> warpedPoints(pointCount);
        for (size_t i = 0; i < pointCount; ++i) {
            size_t y = i / sqrtVal, x = i % sqrtVal;
            Point2f sample;

            // Sample a point following the sampling strategy
            switch (strategy) {
                case Independent:
                    sample = Point2f(sampler->nextFloat(), sampler->nextFloat());
                    break;
                case Grid:
                    sample = Point2f((x + 0.5f) * invSqrtVal, (y + 0.5f) * invSqrtVal);
                    break;
                case Stratified:
                    sample = Point2f((x + sampler->nextFloat()) * invSqrtVal,
                                     (y + sampler->nextFloat()) * invSqrtVal);
                    break;
                default:
                    Log(EError, "Unsupported sampling strategy: %d", strategy);
            }

            // Warp the sampled point
            warpedPoints[i] = adapter->warp(toSampleType(sample));
        }

        return warpedPoints;
    }

    static std::vector<double> binPoints(const Adapter *adapter,
        const std::vector<PairType> &points,
        size_t gridWidth, size_t gridHeight) {

        std::vector<double> hist(gridWidth * gridHeight, 0.0);

        for (size_t i = 0; i < static_cast<size_t>(points.size()); ++i) {
            const auto& p = points[i];
            if (p.second <= math::Epsilon) { // Sample has null weight
                continue;
            }

            Point2f observation = adapter->template domainToPoint<DomainType>(p.first);
            float x = observation[0],
                  y = observation[1];

            size_t xbin = std::min(gridWidth - 1,
                std::max(0lu, static_cast<size_t>(std::floor(x * gridWidth))));
            size_t ybin = std::min(gridHeight - 1,
                std::max(0lu, static_cast<size_t>(std::floor(y * gridHeight))));

            hist[ybin * gridWidth + xbin] += 1;
        }

        return hist;
    }
};

// Specializations for Float sample type
template <> std::pair<Vector3f, Float>
WarpAdapterHelper<LineWarpAdapter, Float, Float>::warpSample(
    const LineWarpAdapter *adapter, const Point2f& sample) {

    Float p;
    Float w;
    std::tie(p, w) = adapter->warp(sample.x());
    return std::make_pair(toVector3f(p), w);
}

template <> Float
WarpAdapterHelper<LineWarpAdapter, Float, Float>::toSampleType(const Point2f &p) {
    // TODO: we're wasting a some potential here by using only one of the two dimensions.
    return p.x();
}


NAMESPACE_END(detail)
NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
