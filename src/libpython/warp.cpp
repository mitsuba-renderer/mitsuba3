#include <mitsuba/core/warp_adapters.h>
#include <mitsuba/core/warp.h>

#include "python.h"
PYBIND11_DECLARE_HOLDER_TYPE(T, std::shared_ptr<T>);

using mitsuba::warp::WarpAdapter;
using mitsuba::warp::SamplingType;
using Sampler = pcg32;

/// Trampoline class for WarpAdapter
class PyWarpAdapter : public WarpAdapter {
public:
    using WarpAdapter::WarpAdapter;

    std::pair<Vector3f, Float> warpSample(const Point2f &sample) const override {
        using ReturnType = std::pair<Vector3f, Float>;
        PYBIND11_OVERLOAD_PURE(ReturnType, WarpAdapter, warpSample, sample);
    }

    Point2f samplePoint(Sampler * sampler, SamplingType strategy,
                                float invSqrtVal) const override {
        PYBIND11_OVERLOAD(
            Point2f, WarpAdapter, samplePoint, sampler, strategy, invSqrtVal);
    }

    void generateWarpedPoints(Sampler *sampler, SamplingType strategy,
                                      size_t pointCount,
                                      Eigen::MatrixXf &positions,
                                      std::vector<Float> &weights) const override {
        PYBIND11_OVERLOAD_PURE(
            void, WarpAdapter, generateWarpedPoints,
            sampler, strategy, pointCount, positions, weights);
    }

    std::vector<double> generateObservedHistogram(Sampler *sampler,
        SamplingType strategy, size_t pointCount,
        size_t gridWidth, size_t gridHeight) const override {
        PYBIND11_OVERLOAD_PURE(
            std::vector<double>, WarpAdapter, generateObservedHistogram,
            sampler, strategy, pointCount, gridWidth, gridHeight);
    }

    std::vector<double> generateExpectedHistogram(size_t pointCount,
        size_t gridWidth, size_t gridHeight) const override {
        PYBIND11_OVERLOAD(
            std::vector<double>, WarpAdapter, generateExpectedHistogram,
            pointCount, gridWidth, gridHeight);
    }

    bool isIdentity() const override {
        PYBIND11_OVERLOAD(bool, WarpAdapter, isIdentity);
    }
    size_t inputDimensionality() const override {
        PYBIND11_OVERLOAD_PURE(size_t, WarpAdapter, inputDimensionality, /* no args */);
    }
    size_t domainDimensionality() const override {
        PYBIND11_OVERLOAD_PURE(size_t, WarpAdapter, domainDimensionality, /* no args */);
    }
    std::string toString() const override {
        PYBIND11_OVERLOAD_NAME(std::string, WarpAdapter, "__repr__", toString, /* no args */);
    }

protected:
    virtual std::function<Float (double, double)> getPdfIntegrand() const override {
        PYBIND11_OVERLOAD_PURE(
            std::function<Float (double, double)>, WarpAdapter,
            getPdfIntegrand, /* no args */);
    }

    virtual Float getPdfScalingFactor() const override {
        PYBIND11_OVERLOAD_PURE(Float, WarpAdapter, getPdfScalingFactor, /* no args */);
    }
};

MTS_PY_EXPORT(warp) {
    auto m2 = m.def_submodule("warp",
        "Common warping techniques that map from the unit square to other"
        " domains, such as spheres, hemispheres, etc. Also includes all"
        " necessary tools to design and test new warping functions.");

    m2.mdef(warp, squareToUniformSphere)
      .mdef(warp, squareToUniformSpherePdf)

      .mdef(warp, squareToUniformHemisphere)
      .mdef(warp, squareToUniformHemispherePdf)

      .mdef(warp, squareToCosineHemisphere)
      .mdef(warp, squareToCosineHemispherePdf)

      .def("squareToUniformCone", &warp::squareToUniformCone,
           py::arg("sample"), py::arg("cosCutoff"), DM(warp, squareToUniformCone))
      .def("squareToUniformConePdf", &warp::squareToUniformConePdf,
           py::arg("v"), py::arg("cosCutoff"), DM(warp, squareToUniformConePdf))

      .mdef(warp, squareToUniformDisk)
      .mdef(warp, squareToUniformDiskPdf)

      .mdef(warp, squareToUniformDiskConcentric)
      .mdef(warp, squareToUniformDiskConcentricPdf)

      .mdef(warp, uniformDiskToSquareConcentric)

      .mdef(warp, squareToUniformTriangle)
      .mdef(warp, squareToUniformTrianglePdf)

      .mdef(warp, squareToStdNormal)
      .mdef(warp, squareToStdNormalPdf)

      .mdef(warp, squareToTent)
      .mdef(warp, squareToTentPdf)

      .def("intervalToNonuniformTent", &warp::intervalToNonuniformTent,
           py::arg("sample"), py::arg("a"), py::arg("b"), py::arg("c"),
           DM(warp, intervalToNonuniformTent));

    // TODO: probably shouldn't need this anymore
    using mitsuba::warp::WarpType;
    py::enum_<WarpType>(m2, "WarpType")
        .value("NoWarp", WarpType::NoWarp)
        .value("UniformSphere", WarpType::UniformSphere)
        .value("UniformHemisphere", WarpType::UniformHemisphere)
        .value("CosineHemisphere", WarpType::CosineHemisphere)
        .value("UniformCone", WarpType::UniformCone)
        .value("UniformDisk", WarpType::UniformDisk)
        .value("UniformDiskConcentric", WarpType::UniformDiskConcentric)
        .value("UniformTriangle", WarpType::UniformTriangle)
        .value("StandardNormal", WarpType::StandardNormal)
        .value("UniformTent", WarpType::UniformTent)
        .value("NonUniformTent", WarpType::NonUniformTent)
        .export_values();

    using mitsuba::warp::SamplingType;
    py::enum_<SamplingType>(m2, "SamplingType")
        .value("Independent", SamplingType::Independent)
        .value("Grid", SamplingType::Grid)
        .value("Stratified", SamplingType::Stratified)
        .export_values();


    /// WarpAdapter class declaration
    using warp::WarpAdapter;
    auto w = py::class_<WarpAdapter, std::shared_ptr<WarpAdapter>, PyWarpAdapter>(
        m2, "WarpAdapter", DM(warp, WarpAdapter))
        .def(py::init<const std::string &,
                      const std::vector<WarpAdapter::Argument>,
                      const BoundingBox3f &>(),
             DM(warp, WarpAdapter, WarpAdapter))
        .def_readonly_static(
            "kUnitSquareBoundingBox", &WarpAdapter::kUnitSquareBoundingBox,
            "Bounding box corresponding to the first quadrant ([0..1]^n)")
        .def_readonly_static(
            "kCenteredSquareBoundingBox", &WarpAdapter::kCenteredSquareBoundingBox,
            "Bounding box corresponding to a disk of radius 1 centered at the origin ([-1..1]^n)")
        .def("samplePoint", &WarpAdapter::samplePoint, DM(warp, WarpAdapter, samplePoint))
        .def("warpSample", &WarpAdapter::warpSample, DM(warp, WarpAdapter, warpSample))
        .def("isIdentity", &WarpAdapter::isIdentity, DM(warp, WarpAdapter, isIdentity))
        .def("inputDimensionality", &WarpAdapter::inputDimensionality, DM(warp, WarpAdapter, inputDimensionality))
        .def("domainDimensionality", &WarpAdapter::domainDimensionality, DM(warp, WarpAdapter, domainDimensionality))
        .def("__repr__", &WarpAdapter::toString);

    /// Argument class
    py::class_<WarpAdapter::Argument>(w, "Argument", DM(warp, WarpAdapter, Argument))
        .def(py::init<const std::string &, Float, Float, Float, const std::string &>(),
             py::arg("name"), py::arg("minValue") = 0.0, py::arg("maxValue") = 1.0,
             py::arg("defaultValue") = 0.0, py::arg("description") = "",
             "Represents one argument to a warping function")
        .def("map", &WarpAdapter::Argument::map, DM(warp, WarpAdapter, Argument, map))
        .def("normalize", &WarpAdapter::Argument::normalize, DM(warp, WarpAdapter, Argument, normalize))
        .def("clamp", &WarpAdapter::Argument::clamp, DM(warp, WarpAdapter, Argument, clamp))
        .def_readonly("name", &WarpAdapter::Argument::name,
                      DM(warp, WarpAdapter, Argument, name))
        .def_readonly("minValue", &WarpAdapter::Argument::minValue,
                      DM(warp, WarpAdapter, Argument, minValue))
        .def_readonly("maxValue", &WarpAdapter::Argument::maxValue,
                      DM(warp, WarpAdapter, Argument, maxValue))
        .def_readonly("defaultValue", &WarpAdapter::Argument::defaultValue,
                      DM(warp, WarpAdapter, Argument, defaultValue))
        .def_readonly("description", &WarpAdapter::Argument::description,
                      DM(warp, WarpAdapter, Argument, description));

    //             return pdf_(*args, **kwargs);
    //         }

    //         return std::make_shared<???>(name_, f, pdf, );
    //     }

    //     std::string name() { return name_; }

    // protected:
    //     const std::string name_;
    //     const py::function f_, pdf_;
    //     const BoundingBox3f bbox_;
    // };

    // py::class_<WarpFactory>(m2, "WarpFactory", "TODO: docs")
    //     .def(py::init<>())

    using warp::LineWarpAdapter;
    py::class_<LineWarpAdapter, std::shared_ptr<LineWarpAdapter>>(
        m2, "LineWarpAdapter", py::base<WarpAdapter>(), DM(warp, LineWarpAdapter))
        .def(py::init<const std::string &,
                      const LineWarpAdapter::WarpFunctionType &,
                      const LineWarpAdapter::PdfFunctionType &,
                      const std::vector<WarpAdapter::Argument> &,
                      const BoundingBox3f &>(),
             py::arg("name"), py::arg("f"), py::arg("pdf"),
             py::arg("arguments") = std::vector<WarpAdapter::Argument>(),
             py::arg("bbox") = WarpAdapter::kCenteredSquareBoundingBox,
             DM(warp, LineWarpAdapter, LineWarpAdapter))
        .def("__repr__", [](const LineWarpAdapter &w) {
            return w.toString();
        });

    using warp::PlaneWarpAdapter;
    py::class_<PlaneWarpAdapter, std::shared_ptr<PlaneWarpAdapter>>(
        m2, "PlaneWarpAdapter", py::base<WarpAdapter>(), DM(warp, PlaneWarpAdapter))
        .def(py::init<const std::string &,
                      const PlaneWarpAdapter::WarpFunctionType &,
                      const PlaneWarpAdapter::PdfFunctionType &,
                      const std::vector<WarpAdapter::Argument> &,
                      const BoundingBox3f &>(),
             py::arg("name"), py::arg("f"), py::arg("pdf"),
             py::arg("arguments") = std::vector<WarpAdapter::Argument>(),
             py::arg("bbox") = WarpAdapter::kCenteredSquareBoundingBox,
             DM(warp, PlaneWarpAdapter, PlaneWarpAdapter))
        .def("__repr__", [](const PlaneWarpAdapter &w) {
            return w.toString();
        });

    using warp::IdentityWarpAdapter;
    py::class_<IdentityWarpAdapter, std::shared_ptr<IdentityWarpAdapter>>(
        m2, "IdentityWarpAdapter", py::base<PlaneWarpAdapter>(), DM(warp, IdentityWarpAdapter))
        .def(py::init<>(), DM(warp, IdentityWarpAdapter, IdentityWarpAdapter))
        .def("__repr__", [](const IdentityWarpAdapter &w) {
            return w.toString();
        });

    using warp::SphereWarpAdapter;
    py::class_<SphereWarpAdapter, std::shared_ptr<SphereWarpAdapter>>(
        m2, "SphereWarpAdapter", py::base<WarpAdapter>(), DM(warp, SphereWarpAdapter))
        .def(py::init<const std::string &,
                      const SphereWarpAdapter::WarpFunctionType &,
                      const SphereWarpAdapter::PdfFunctionType &,
                      const std::vector<WarpAdapter::Argument> &,
                      const BoundingBox3f &>(),
             py::arg("name"), py::arg("f"), py::arg("pdf"),
             py::arg("arguments") = std::vector<WarpAdapter::Argument>(),
             py::arg("bbox") = WarpAdapter::kCenteredSquareBoundingBox,
             DM(warp, SphereWarpAdapter, SphereWarpAdapter))
        .def("__repr__", [](const SphereWarpAdapter &w) {
            return w.toString();
        });



    m2.def("runStatisticalTest", &warp::detail::runStatisticalTest, DM(warp, detail, runStatisticalTest));
}
