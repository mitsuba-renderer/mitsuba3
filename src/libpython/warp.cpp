#include <mitsuba/core/warp.h>
#include <mitsuba/gui/warp_visualizer.h>
#include "python.h"

MTS_PY_EXPORT(warp) {
    auto m2 = m.def_submodule("warp", "Common warping techniques that map from the unit"
                                      "square to other domains, such as spheres,"
                                      " hemispheres, etc.");

    m2.mdef(warp, unitSphereIndicator)
      .mdef(warp, squareToUniformSphere)
      .mdef(warp, squareToUniformSpherePdf)

      .mdef(warp, unitHemisphereIndicator)
      .mdef(warp, squareToUniformHemisphere)
      .mdef(warp, squareToUniformHemispherePdf)

      .mdef(warp, squareToCosineHemisphere)
      .mdef(warp, squareToCosineHemispherePdf)

      .mdef(warp, unitConeIndicator)
      .mdef(warp, squareToUniformCone)
      .mdef(warp, squareToUniformConePdf)

      .mdef(warp, unitDiskIndicator)
      .mdef(warp, squareToUniformDisk)
      .mdef(warp, squareToUniformDiskPdf)

      .mdef(warp, squareToUniformDiskConcentric)
      .mdef(warp, squareToUniformDiskConcentricPdf)

      .mdef(warp, unitSquareIndicator)
      .mdef(warp, uniformDiskToSquareConcentric)

      .mdef(warp, triangleIndicator)
      .mdef(warp, squareToUniformTriangle)
      .mdef(warp, squareToUniformTrianglePdf)

      .mdef(warp, squareToStdNormal)
      .mdef(warp, squareToStdNormalPdf)

      .mdef(warp, squareToTent)
      .mdef(warp, squareToTentPdf)
      .mdef(warp, intervalToNonuniformTent);

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

    m2.def("runStatisticalTest", &warp::detail::runStatisticalTest,
           "Runs a Chi^2 statistical test verifying the given warping type"
           "against its PDF. Returns (passed, reason)");

    // Warp visualization widget, inherits from nanogui::Screen (which is
    // already exposed to Python in another module).
    const auto PyScreen = static_cast<py::object>(py::module::import("nanogui").attr("Screen"));
    using mitsuba::warp::WarpVisualizationWidget;
    // TODO: use docstrings from the class (DM, etc)
    py::class_<WarpVisualizationWidget>(m2, "WarpVisualizationWidget", PyScreen)
        .def(py::init<int, int, std::string>(), "Default constructor.")
        .def("runTest", &WarpVisualizationWidget::runTest,
            "Run the Chi^2 test for the selected parameters and displays the histograms.")
        .def("refresh", &WarpVisualizationWidget::refresh,
             "Should be called upon UI interaction.")
        .def("setSamplingType", &WarpVisualizationWidget::setSamplingType, "")
        .def("setWarpType", &WarpVisualizationWidget::setWarpType, "")
        .def("setParameterValue", &WarpVisualizationWidget::setParameterValue, "")
        .def("setPointCount", &WarpVisualizationWidget::setPointCount, "")
        .def("isDrawingHistogram", &WarpVisualizationWidget::isDrawingHistogram, "")
        .def("setDrawHistogram", &WarpVisualizationWidget::setDrawHistogram, "")
        .def("isDrawingGrid", &WarpVisualizationWidget::isDrawingGrid, "")
        .def("setDrawGrid", &WarpVisualizationWidget::setDrawGrid, "")
        .def_readwrite("window", &WarpVisualizationWidget::window);
}
