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

    bool isIdentity() const {
        PYBIND11_OVERLOAD(bool, WarpAdapter, isIdentity);
    }
    size_t inputDimensionality() const {
        PYBIND11_OVERLOAD(size_t, WarpAdapter, inputDimensionality);
    }
    size_t domainDimensionality() const {
        PYBIND11_OVERLOAD(size_t, WarpAdapter, domainDimensionality);
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

    using warp::WarpAdapter;
    py::class_<WarpAdapter, ref<WarpAdapter>, PyWarpAdapter>(m2, "WarpAdapter",
                                                             DM(warp, WarpAdapter))
        .def("isIdentity", &WarpAdapter::isIdentity, DM(warp, WarpAdapter, isIdentity))
        .def("inputDimensionality", &WarpAdapter::inputDimensionality, DM(warp, WarpAdapter, inputDimensionality))
        .def("domainDimensionality", &WarpAdapter::domainDimensionality, DM(warp, WarpAdapter, domainDimensionality))
        .def("__repr__", &WarpAdapter::toString);

    using warp::PlaneWarpAdapter;
    // TODO: build a trampoline if there are new virtual functions
    // TODO: check that virtual methods there are forwarded correctly
    py::class_<PlaneWarpAdapter, ref<PlaneWarpAdapter>>(m2, "PlaneWarpAdapter",
                                                        py::base<WarpAdapter>(),
                                                        DM(warp, PlaneWarpAdapter))
        .def(py::init<const std::string &,
                      const PlaneWarpAdapter::WarpFunctionType &,
                      const PlaneWarpAdapter::PdfFunctionType &,
                      const std::vector<WarpAdapter::Argument> &>(),
             py::arg("name"), py::arg("f"), py::arg("pdf"),
             py::arg("arguments") = std::vector<WarpAdapter::Argument>(),
             DM(warp, PlaneWarpAdapter, PlaneWarpAdapter));

    using warp::IdentityWarpAdapter;
    py::class_<IdentityWarpAdapter, ref<IdentityWarpAdapter>>(m2, "IdentityWarpAdapter",
                                                              py::base<PlaneWarpAdapter>(),
                                                              DM(warp, IdentityWarpAdapter))
        .def(py::init<>(), DM(warp, IdentityWarpAdapter, IdentityWarpAdapter));


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
