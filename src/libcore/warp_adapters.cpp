#include <mitsuba/core/warp_adapters.h>

#include <hypothesis.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(warp)

const BoundingBox3f WarpAdapter::kUnitSquareBoundingBox =
    BoundingBox3f(Point3f(0, 0, 0), Point3f(1, 1, 1));
const BoundingBox3f WarpAdapter::kCenteredSquareBoundingBox =
    BoundingBox3f(Point3f(-1, -1, -1), Point3f(1, 1, 1));

Point2f WarpAdapter::samplePoint(Sampler * sampler, SamplingType strategy,
                                 float) const {
    switch (strategy) {
        case Independent:
            return Point2f(sampler->nextFloat(), sampler->nextFloat());

        // case Grid:
        //     return Point2f((x + 0.5f) * invSqrtVal, (y + 0.5f) * invSqrtVal);

        // case Stratified:
        //     return Point2f((x + sampler->nextFloat()) * invSqrtVal,
        //                    (y + sampler->nextFloat()) * invSqrtVal);

        default:
            Log(EError, "Unsupported sampling strategy: %d", strategy);
            return Point2f();
    }
}

std::vector<double> WarpAdapter::generateExpectedHistogram(size_t pointCount,
    size_t gridWidth, size_t gridHeight) const {

    std::vector<double> hist(gridWidth * gridHeight, 0.0);
    double scale = pointCount * static_cast<double>(getPdfScalingFactor());

    auto integrand = getPdfIntegrand();

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

std::pair<Vector3f, Float> PlaneWarpAdapter::warpSample(const Point2f& sample) const {
    DomainType p;
    Float w;
    std::tie(p, w) = warp(sample);
    return std::make_pair(Vector3f(p.x(), p.y(), 0.0), w);
}

void PlaneWarpAdapter::generateWarpedPoints(Sampler *sampler, SamplingType strategy,
                                            size_t pointCount,
                                            Eigen::MatrixXf &positions,
                                            std::vector<Float> &weights) const {
    const auto points = generatePoints(sampler, strategy, pointCount);

    positions.resize(3, points.size());
    weights.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        positions.col(i) << p.first.x(), p.first.y(), 0.f;
        // TODO: also write out proper weights
        weights[i] = p.second;
    }
}

std::vector<double> PlaneWarpAdapter::generateObservedHistogram(Sampler *sampler,
    SamplingType strategy, size_t pointCount, size_t gridWidth, size_t gridHeight) const {

    const auto points = generatePoints(sampler, strategy, pointCount);
    return binPoints(points, gridWidth, gridHeight);
}

std::function<Float (double, double)> PlaneWarpAdapter::getPdfIntegrand() const {
    return [this](double y, double x) {
        return pdf(pointToDomain<DomainType>(Point2f(x, y)));
    };
}

std::vector<PlaneWarpAdapter::PairType>
PlaneWarpAdapter::generatePoints(Sampler * sampler, SamplingType strategy,
                                 size_t pointCount) const {
    size_t sqrtVal = static_cast<size_t>(std::sqrt((float) pointCount) + 0.5f);
    float invSqrtVal = 1.f / sqrtVal;
    if (strategy == Grid || strategy == Stratified)
        pointCount = sqrtVal * sqrtVal;

    std::vector<PairType> warpedPoints(pointCount);
    for (size_t i = 0; i < pointCount; ++i) {
        warpedPoints[i] = warp(samplePoint(sampler, strategy, invSqrtVal));
    }

    return warpedPoints;
}

std::vector<double> PlaneWarpAdapter::binPoints(
    const std::vector<PlaneWarpAdapter::PairType> &points,
    size_t gridWidth, size_t gridHeight) const {

    std::vector<double> hist(gridWidth * gridHeight, 0.0);

    for (size_t i = 0; i < static_cast<size_t>(points.size()); ++i) {
        const auto& p = points[i];
        if (p.second == static_cast<Float>(0.0)) {
            continue;
        }

        Point2f observation = domainToPoint<DomainType>(p.first);
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


// TODO: refactor, this is almost the same as in PlaneWarpAdapter
void SphereWarpAdapter::generateWarpedPoints(Sampler *sampler, SamplingType strategy,
                                             size_t pointCount,
                                             Eigen::MatrixXf &positions,
                                             std::vector<Float> &weights) const {
    const auto points = generatePoints(sampler, strategy, pointCount);

    positions.resize(3, points.size());
    weights.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        positions.col(i) << p.first.x(), p.first.y(), p.first.z();
        // TODO: also write out proper weights
        weights[i] = p.second;
    }
}

// TODO: refactor, this is almost the same as in PlaneWarpAdapter
std::vector<double> SphereWarpAdapter::generateObservedHistogram(Sampler *sampler,
    SamplingType strategy, size_t pointCount, size_t gridWidth, size_t gridHeight) const {

    const auto points = generatePoints(sampler, strategy, pointCount);
    return binPoints(points, gridWidth, gridHeight);
}

std::function<Float (double, double)> SphereWarpAdapter::getPdfIntegrand() const {
    return [this](double y, double x) {
        return pdf(pointToDomain<DomainType>(Point2f(x, y)));
    };
}

// TODO: refactor, this is almost the same as in PlaneWarpAdapter
std::vector<SphereWarpAdapter::PairType>
SphereWarpAdapter::generatePoints(Sampler * sampler, SamplingType strategy,
                                 size_t pointCount) const {
    size_t sqrtVal = static_cast<size_t>(std::sqrt((float) pointCount) + 0.5f);
    float invSqrtVal = 1.f / sqrtVal;
    if (strategy == Grid || strategy == Stratified)
        pointCount = sqrtVal * sqrtVal;

    std::vector<PairType> warpedPoints(pointCount);
    for (size_t i = 0; i < pointCount; ++i) {
        warpedPoints[i] = warp(samplePoint(sampler, strategy, invSqrtVal));
    }

    return warpedPoints;
}

// TODO: refactor, this is almost the same as in PlaneWarpAdapter
std::vector<double> SphereWarpAdapter::binPoints(
    const std::vector<SphereWarpAdapter::PairType> &points,
    size_t gridWidth, size_t gridHeight) const {

    std::vector<double> hist(gridWidth * gridHeight, 0.0);

    for (size_t i = 0; i < static_cast<size_t>(points.size()); ++i) {
        const auto& p = points[i];
        if (p.second <= math::Epsilon) { // Sample has null weight
            continue;
        }

        Point2f observation = domainToPoint<DomainType>(p.first);
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


NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
