#include <mitsuba/core/warp_adapters.h>
#include <mitsuba/core/detail/warp_adapters.hpp>

#include <hypothesis.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(warp)

using detail::WarpAdapterHelper;

const BoundingBox3f WarpAdapter::kUnitSquareBoundingBox =
    BoundingBox3f(Point3f(0, 0, 0), Point3f(1, 1, 1));
const BoundingBox3f WarpAdapter::kCenteredSquareBoundingBox =
    BoundingBox3f(Point3f(-1, -1, -1), Point3f(1, 1, 1));

std::vector<double> WarpAdapter::generateExpectedHistogram(size_t pointCount,
    size_t gridWidth, size_t gridHeight) const {

    std::vector<double> hist(gridWidth * gridHeight, 0.0);
    double scale = pointCount * static_cast<double>(getPdfScalingFactor());

    auto integrand = getPdfIntegrand();

    Log(EInfo, "%f x %f", gridWidth, gridHeight);

    for (size_t y = 0; y < gridHeight; ++y) {
        double yStart = y       / static_cast<double>(gridHeight);
        double yEnd   = (y + 1) / static_cast<double>(gridHeight);
        for (size_t x = 0; x < gridWidth; ++x) {
            double xStart =  x      / static_cast<double>(gridWidth);
            double xEnd   = (x + 1) / static_cast<double>(gridWidth);

            hist[y * gridWidth + x] = scale *
                hypothesis::adaptiveSimpson2D(integrand,
                                              yStart, xStart, yEnd, xEnd);
            if (hist[y * gridWidth + x] < 0)
                Log(EError, "The Pdf() function returned negative values!");
        }
    }

    return hist;
}

// -----------------------------------------------------------------------------

#define FORWARD_TO_HELPER(AdapterType) \
    std::pair<Vector3f, Float> AdapterType::warpSample(const Point2f& sample) const { \
        return Helper::warpSample(this, sample);                                      \
    }                                                                                 \
                                                                                      \
    void AdapterType::generateWarpedPoints(Sampler *sampler, SamplingType strategy,   \
                                                size_t pointCount,                    \
                                                Eigen::MatrixXf &positions,           \
                                                std::vector<Float> &weights) const {  \
        Helper::generateWarpedPoints(this,                                            \
            sampler, strategy, pointCount, positions, weights);                       \
    }                                                                                 \
                                                                                      \
    std::vector<double> AdapterType::generateObservedHistogram(Sampler *sampler,      \
        SamplingType strategy, size_t pointCount, size_t gridWidth, size_t gridHeight) const { \
                                                                                      \
        return Helper::generateObservedHistogram(this,                                \
            sampler, strategy, pointCount, gridWidth, gridHeight);                    \
    }                                                                                 \
                                                                                      \
    std::function<Float (double, double)> AdapterType::getPdfIntegrand() const {      \
        return Helper::getPdfIntegrand(this);                                         \
    }                                                                                 \
                                                                                      \
    std::vector<AdapterType::PairType>                                                \
    AdapterType::generatePoints(Sampler * sampler, SamplingType strategy,             \
                                     size_t &pointCount) const {                      \
        return Helper::generatePoints(this, sampler, strategy, pointCount);           \
    }                                                                                 \
                                                                                      \
    std::vector<double> AdapterType::binPoints(                                       \
        const std::vector<AdapterType::PairType> &points,                             \
        size_t gridWidth, size_t gridHeight) const {                                  \
                                                                                      \
        return Helper::binPoints(this, points, gridWidth, gridHeight);                \
    }


// -----------------------------------------------------------------------------

FORWARD_TO_HELPER(LineWarpAdapter)
FORWARD_TO_HELPER(PlaneWarpAdapter)
FORWARD_TO_HELPER(SphereWarpAdapter)

#undef FORWARD_TO_HELPER

NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
