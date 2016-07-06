#include <mitsuba/core/warp_adapters.h>

#include <hypothesis.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(warp)

Point2f WarpAdapter::samplePoint(Sampler * sampler, SamplingType strategy,
                                 float invSqrtVal) const {
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
    }
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
        positions.col(i) << p.x(), p.y(), 0.f;
        // TODO: also write out proper weights
        weights[i] = 1.0;
    }
}

std::vector<double> PlaneWarpAdapter::generateObservedHistogram(Sampler *sampler,
    SamplingType strategy, size_t pointCount, size_t gridWidth, size_t gridHeight) const {

    const auto points = generatePoints(sampler, strategy, pointCount);
    return binPoints(points, gridWidth, gridHeight);
}

std::vector<double> PlaneWarpAdapter::generateExpectedHistogram(size_t pointCount,
    size_t gridWidth, size_t gridHeight) const {

    std::vector<double> hist(gridWidth * gridHeight, 0.0);
    double scale = pointCount * static_cast<double>(kPdfScalingFactor);

    auto integrand = [this](double y, double x) {
        return pdf(pointToDomain(Point2f(x, y)));
    };

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

Point2f PlaneWarpAdapter::domainToPoint(const PlaneWarpAdapter::DomainType &v) const {
    return Point2f(0.5f * v.x() + 0.5f, 0.5f * v.y() + 0.5f);
}
PlaneWarpAdapter::DomainType PlaneWarpAdapter::pointToDomain(const Point2f &p) const {
    return Point2f(2 * p.x() - 1, 2 * p.y() - 1);
}

std::vector<PlaneWarpAdapter::DomainType> PlaneWarpAdapter::generatePoints(Sampler * sampler, SamplingType strategy, size_t pointCount) const {
    size_t sqrtVal = static_cast<size_t>(std::sqrt((float) pointCount) + 0.5f);
    float invSqrtVal = 1.f / sqrtVal;
    if (strategy == Grid || strategy == Stratified)
        pointCount = sqrtVal * sqrtVal;

    std::vector<DomainType> points(pointCount);
    for (size_t i = 0; i < pointCount; ++i) {
        const auto s = samplePoint(sampler, strategy, invSqrtVal);
        // TODO: we'll need to export that weight
        Float weight;
        std::tie(points[i], weight) = warp(s);
    }

    return points;
}

std::vector<double> PlaneWarpAdapter::binPoints(const std::vector<PlaneWarpAdapter::DomainType> &points,
    size_t gridWidth, size_t gridHeight) const {

    std::vector<double> hist(gridWidth * gridHeight, 0.0);

    for (size_t i = 0; i < static_cast<size_t>(points.size()); ++i) {
        // TODO: support weights
        // if (weights[i] == 0) {
        //     continue;
        // }

        Point2f observation = domainToPoint(points[i]);
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
