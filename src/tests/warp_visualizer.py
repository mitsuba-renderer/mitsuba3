import gc
import math
import mitsuba
from mitsuba.warp import WarpType, SamplingType, WarpVisualizationWidget

import nanogui
from nanogui import Color, Screen, Window, Widget, GroupLayout, BoxLayout, \
                    Vector2i, Label, Button, TextBox, CheckBox, MessageDialog, \
                    ComboBox, Slider, Alignment, Orientation
from nanogui import glfw, entypo

# TODO: should this move to libgui?
class WarpVisualizer(WarpVisualizationWidget):
    """TODO: docstring for this class"""

    # Default values for UI controls
    pointCountDefaultValue = 7.0 / 15.0
    warpParameterDefaultValue = 0.5
    angleDefaultValue = 0.5

    # Default values for statistical test
    minExpFrequency = 5.0
    significanceLevel = 0.01

    def __init__(self):
        super(WarpVisualizer, self).__init__(800, 600, "Warp visualizer")
        self.initializeGUI()

    def runTest(self):
        super(WarpVisualizer, self).runTest(
            WarpVisualizer.minExpFrequency, WarpVisualizer.significanceLevel)
        self.setDrawHistogram(True)
        self.window.setVisible(False)

    def mapParameter(self, warpType, value):
        """Converts the parameter value to the appropriate domain
            depending on the warp type."""
        return value

    def initializeGUI(self):
        # Main window
        window = Window(self, "Warp tester")
        window.setPosition(Vector2i(15, 15))
        window.setLayout(GroupLayout())

        _ = Label(window, "Input point set", "sans-bold")

        # ---------- First panel
        panel = Widget(window)
        panel.setLayout(BoxLayout(Orientation.Horizontal, Alignment.Middle, 0, 20))

        # Point count slider
        pointCountSlider = Slider(panel)
        pointCountSlider.setFixedWidth(55)
        pointCountSlider.setValue(WarpVisualizer.pointCountDefaultValue)
        pointCountSlider.setCallback(lambda _: self.refresh())
        # Companion text box
        pointCountBox = TextBox(panel)
        pointCountBox.setFixedSize(Vector2i(80, 25))

        # Selection of sampling strategy
        samplingTypeBox = ComboBox(window, ["Independent", "Grid", "Stratified"])
        samplingTypeBox.setCallback(lambda _: self.refresh())

        # Selection of warping method
        _ = Label(window, "Warping method", "sans-bold")
        warpTypeBox = ComboBox(window, ["None", "Sphere", "Hemisphere (unif.)",
                                        "Hemisphere (cos)", "Cone", "Disk",
                                        "Disk (concentric)", "Triangle",
                                        "Standard normal", "Tent (unif.)",
                                        "Tent (non unif.)"])
        warpTypeBox.setCallback(lambda _: self.refresh())

        # ---------- Second panel
        panel = Widget(window)
        panel.setLayout(BoxLayout(Orientation.Horizontal, Alignment.Middle, 0, 20))

        # Parameter value of the warping function
        parameterSlider = Slider(panel)
        parameterSlider.setFixedWidth(55)
        parameterSlider.setValue(WarpVisualizer.warpParameterDefaultValue)
        parameterSlider.setCallback(lambda _: self.refresh())
        # Companion text box
        parameterBox = TextBox(panel)
        parameterBox.setFixedSize(Vector2i(80, 25))

        # Option to visualize the warped grid
        warpedGridCheckBox = CheckBox(window, "Visualize warped grid")
        warpedGridCheckBox.setCallback(lambda _: self.refresh())

        _ = Label(window, "BSDF parameters", "sans-bold")

        # ---------- Third panel
        panel = Widget(window)
        panel.setLayout(BoxLayout(Orientation.Horizontal, Alignment.Middle, 0, 20))

        # Angle BSDF parameter
        angleSlider = Slider(panel)
        angleSlider.setFixedWidth(55)
        angleSlider.setValue(WarpVisualizer.angleDefaultValue)
        angleSlider.setCallback(lambda _: self.refresh())
        # Companion text box
        angleBox = TextBox(panel)
        angleBox.setFixedSize(Vector2i(80, 25))
        angleBox.setUnits(u"\u00B0")
        # Option to visualize the BRDF values
        brdfValuesCheckBox = CheckBox(window, "Visualize BRDF values")
        brdfValuesCheckBox.setCallback(lambda _: self.refresh())

        # Chi-2 test button
        _ = Label(window, u"\u03C7\u00B2 hypothesis test", "sans-bold")
        testButton = Button(window, "Run", entypo.ICON_CHECK)
        testButton.setBackgroundColor(Color(0, 1., 0., 0.25))

        def tryTest():
            try:
                self.runTest()
            except Exception as e:
                _ = MessageDialog(self, MessageDialog.Type.Warning, "Error",
                                  "An error occurred: " + str(e))
        testButton.setCallback(tryTest)

        self.performLayout()
        # Keep references to the important UI elements
        self.window = window
        self.pointCountSlider = pointCountSlider
        self.pointCountBox = pointCountBox
        self.samplingTypeBox = samplingTypeBox
        self.warpTypeBox = warpTypeBox
        self.parameterSlider = parameterSlider
        self.parameterBox = parameterBox
        self.warpedGridCheckBox = warpedGridCheckBox
        self.angleSlider = angleSlider
        self.angleBox = angleBox
        self.brdfValuesCheckBox = brdfValuesCheckBox
        self.testButton = testButton

        self.refresh()
        self.drawAll()
        self.setVisible(True)

    def refresh(self):
        samplingType = SamplingType(self.samplingTypeBox.selectedIndex())
        warpType = WarpType(self.warpTypeBox.selectedIndex())
        parameterValue = self.mapParameter(warpType, self.parameterSlider.value())
        angle = 180 * self.angleSlider.value() - 90

        # Point count slider input is not linear
        pointCount = int(math.pow(2.0, 15.0 * self.pointCountSlider.value() + 5))
        self.pointCountSlider.setValue(
            (math.log(pointCount) / math.log(2.0) - 5) / 15.0);

        # Update the user interface
        def formattedPointCount(n):
            if (n >= 1e6):
                self.pointCountBox.setUnits("M")
                return "{:.2f}".format(n * 1e-6)
            if (n >= 1e3):
                self.pointCountBox.setUnits("K")
                return "{:.2f}".format(n * 1e-3)

            self.pointCountBox.setUnits(" ")
            return str(n)
        self.pointCountBox.setValue(formattedPointCount(pointCount))

        self.parameterBox.setValue("{:.1g}".format(parameterValue))
        self.angleBox.setValue("{:.1f}".format(angle))
        # TODO
        self.parameterSlider.setEnabled(warpType in [])
        self.angleSlider.setEnabled(warpType is False)
        self.angleBox.setEnabled(warpType is False)
        self.parameterBox.setEnabled(warpType is False)
        self.brdfValuesCheckBox.setEnabled(warpType is False)

        # Now trigger refresh in WarpVisualizationWidget
        self.setSamplingType(samplingType)
        self.setWarpType(warpType)
        self.setParameterValue(parameterValue)
        self.setPointCount(pointCount)
        self.setDrawGrid(self.warpedGridCheckBox.checked())
        return super(WarpVisualizer, self).refresh()

    def draw(self, ctx):
        super(WarpVisualizer, self).draw(ctx)


if __name__ == "__main__":
    nanogui.init()
    _ = WarpVisualizer()
    nanogui.mainloop()
    gc.collect()
    nanogui.shutdown()
