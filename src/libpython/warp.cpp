#include <mitsuba/core/warp_adapters.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/gui/warp_visualizer.h>
#include <nanogui/python.h>

#include "python.h"

/// Trampoline class
using mitsuba::warp::WarpAdapter;
using mitsuba::warp::SamplingType;
using Sampler = pcg32;

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
        PYBIND11_OVERLOAD_PURE(
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
    virtual Float getPdfScalingFactor() const override {
        PYBIND11_OVERLOAD_PURE(Float, WarpAdapter, getPdfScalingFactor, /* no args */);
    }
};

/// Trampoline class
using mitsuba::warp::WarpVisualizationWidget;
class PyWarpVisualizationWidget : public WarpVisualizationWidget {
public:
    using WarpVisualizationWidget::WarpVisualizationWidget;
    NANOGUI_WIDGET_OVERLOADS(WarpVisualizationWidget);
    NANOGUI_SCREEN_OVERLOADS(WarpVisualizationWidget);
};

MTS_PY_EXPORT(warp) {
    auto m2 = m.def_submodule("warp", "Common warping techniques that map from the unit"
                                      "square to other domains, such as spheres,"
                                      " hemispheres, etc.");

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
    auto w = py::class_<WarpAdapter, std::unique_ptr<WarpAdapter>, PyWarpAdapter>(
        m2, "WarpAdapter", DM(warp, WarpAdapter))
        .def(py::init<const std::string &,
                      const std::vector<WarpAdapter::Argument>,
                      const BoundingBox3f &>(),
             DM(warp, WarpAdapter, WarpAdapter))
        .def_readonly_static("kUnitSquareBoundingBox", &WarpAdapter::kUnitSquareBoundingBox,
                             "Bounding box corresponding to the first quadrant ([0..1]^n)")
        .def_readonly_static("kCenteredSquareBoundingBox", &WarpAdapter::kCenteredSquareBoundingBox,
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


    using warp::PlaneWarpAdapter;
    py::class_<PlaneWarpAdapter>(
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
    py::class_<IdentityWarpAdapter>(
        m2, "IdentityWarpAdapter", py::base<PlaneWarpAdapter>(), DM(warp, IdentityWarpAdapter))
        .def(py::init<>(), DM(warp, IdentityWarpAdapter, IdentityWarpAdapter))
        .def("__repr__", [](const IdentityWarpAdapter &w) {
            return w.toString();
        });


    m2.def("runStatisticalTest", &warp::detail::runStatisticalTest, DM(warp, detail, runStatisticalTest));


    // Warp visualization widget, inherits from nanogui::Screen (which is
    // already exposed to Python in another module).
    py::module::import("nanogui");
    py::class_<WarpVisualizationWidget, ref<WarpVisualizationWidget>, PyWarpVisualizationWidget>(m2, "WarpVisualizationWidget", py::base<nanogui::Screen>(), DM(warp, WarpVisualizationWidget))
        .def(py::init<int, int, std::string>(), DM(warp, WarpVisualizationWidget, WarpVisualizationWidget))
        .def("runTest", &WarpVisualizationWidget::runTest, DM(warp, WarpVisualizationWidget, runTest))
        .def("refresh", &WarpVisualizationWidget::refresh, DM(warp, WarpVisualizationWidget, refresh))

        .def("mouseMotionEvent", &WarpVisualizationWidget::mouseMotionEvent, DM(warp, WarpVisualizationWidget, mouseMotionEvent))
        .def("mouseButtonEvent", &WarpVisualizationWidget::mouseButtonEvent, DM(warp, WarpVisualizationWidget, mouseButtonEvent))

        .def("setSamplingType", &WarpVisualizationWidget::setSamplingType, DM(warp, WarpVisualizationWidget, setSamplingType))
        .def("setWarpAdapter", &WarpVisualizationWidget::setWarpAdapter, DM(warp, WarpVisualizationWidget, setWarpAdapter))
        .def("setPointCount", &WarpVisualizationWidget::setPointCount, DM(warp, WarpVisualizationWidget, setPointCount))
        .def("isDrawingHistogram", &WarpVisualizationWidget::isDrawingHistogram, DM(warp, WarpVisualizationWidget, isDrawingHistogram))
        .def("setDrawHistogram", &WarpVisualizationWidget::setDrawHistogram, DM(warp, WarpVisualizationWidget, setDrawHistogram))
        .def("isDrawingGrid", &WarpVisualizationWidget::isDrawingGrid, DM(warp, WarpVisualizationWidget, isDrawingGrid))
        .def("setDrawGrid", &WarpVisualizationWidget::setDrawGrid, DM(warp, WarpVisualizationWidget, setDrawGrid));
}
