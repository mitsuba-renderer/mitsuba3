#include <mitsuba/core/warp_adapters.h>
#include <mitsuba/gui/warp_visualizer.h>
#include <nanogui/python.h>

#include "python.h"

PYBIND11_DECLARE_HOLDER_TYPE(T, std::shared_ptr<T>);

/// Trampoline class for WarpVisualizationWidget
using mitsuba::warp::WarpVisualizationWidget;
class PyWarpVisualizationWidget : public WarpVisualizationWidget {
public:
    using WarpVisualizationWidget::WarpVisualizationWidget;
    NANOGUI_WIDGET_OVERLOADS(WarpVisualizationWidget);
    NANOGUI_SCREEN_OVERLOADS(WarpVisualizationWidget);
};

MTS_PY_EXPORT(WarpVisualizationWidget) {
    // Warp visualization widget, inherits from nanogui::Screen (which is
    // already exposed to Python in another module).
    py::module::import("nanogui");

    py::class_<WarpVisualizationWidget, std::unique_ptr<WarpVisualizationWidget>, PyWarpVisualizationWidget>(m, "WarpVisualizationWidget", py::base<nanogui::Screen>(), DM(warp, WarpVisualizationWidget))
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
