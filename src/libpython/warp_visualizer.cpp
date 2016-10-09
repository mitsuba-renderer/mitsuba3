#include <mitsuba/core/warp_adapters.h>
#include <mitsuba/ui/warp_visualizer.h>
#include <nanogui/python.h>

#include "python.h"

/// Trampoline class for WarpVisualizationWidget
using mitsuba::ui::WarpVisualizationWidget;

class PyWarpVisualizationWidget : public WarpVisualizationWidget {
public:
    using WarpVisualizationWidget::WarpVisualizationWidget;
    NANOGUI_WIDGET_OVERLOADS(WarpVisualizationWidget);
    NANOGUI_SCREEN_OVERLOADS(WarpVisualizationWidget);
};

MTS_PY_EXPORT(WarpVisualizationWidget) {
    // WarpVisualizationWidget extends from a NanoGUI type, so let's import that first
    py::module::import("nanogui");

    py::class_<WarpVisualizationWidget, nanogui::Screen, std::unique_ptr<WarpVisualizationWidget>, PyWarpVisualizationWidget>(m, "WarpVisualizationWidget", DM(warp, WarpVisualizationWidget))
        .def(py::init<int, int, std::string>(), DM(warp, WarpVisualizationWidget, WarpVisualizationWidget))
        .mdef(WarpVisualizationWidget, runTest)
        .mdef(WarpVisualizationWidget, refresh)
        .mdef(WarpVisualizationWidget, setSamplingType)
        .mdef(WarpVisualizationWidget, setWarpAdapter)
        .mdef(WarpVisualizationWidget, setPointCount)
        .mdef(WarpVisualizationWidget, isDrawingHistogram)
        .mdef(WarpVisualizationWidget, setDrawHistogram)
        .mdef(WarpVisualizationWidget, isDrawingGrid)
        .mdef(WarpVisualizationWidget, setDrawGrid);
}
