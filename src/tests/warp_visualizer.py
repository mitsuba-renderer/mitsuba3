import gc
import math
from enum import Enum
import mitsuba
from mitsuba.warp import WarpType, SamplingType, WarpVisualizationWidget

import nanogui
# TODO: remove unused imports
from nanogui import Color, Screen, Window, GroupLayout, BoxLayout, \
                    ToolButton, Vector2i, Label, Button, Widget, \
                    PopupButton, CheckBox, MessageDialog, VScrollPanel, \
                    ImagePanel, ImageView, ComboBox, ProgressBar, Slider, \
                    TextBox, ColorWheel, Graph, VectorXf, GridLayout, \
                    Alignment, Orientation
from nanogui import glfw, entypo


class WarpVisualizer(Screen):
    """TODO: docstring for this class"""

    # Default values for UI controls
    pointCountDefaultValue = 7.0 / 15.0
    warpParameterDefaultValue = 0.5
    angleDefaultValue = 0.5

    def __init__(self):
        print("hey")
        super(WarpVisualizer, self).__init__(Vector2i(800, 600), "Warp visualizer")
        print("ho")
        self.initializeGUI()

    def runTest(self):
        print("Test will be run")
        raise Exception("Not implemented yet.")

    def mapParameter(self, warpType, value):
        """Converts the parameter value to the appropriate domain
            depending on the warp type."""
        # TODO
        return value

    def initializeGUI(self):
        visualizationWidget = WarpVisualizationWidget()
        # visualizationWidget.performLayout()
        # visualizationWidget.drawAll()
        visualizationWidget.setVisible(True)

        print(self)
        print(self is WarpVisualizationWidget)
        print(self is Widget)
        print(self is Screen)
        print(self is Window)

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
        testButton.setBackgroundColor(Color(0, 255, 0, 25))

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
        self.visualizationWidget = visualizationWidget
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

        # TODO
        #self.background().setZero()

        self.refresh()
        self.drawAll()
        self.setVisible(True)
        # TODO
        # self.framebufferSizeChanged()

    def refresh(self):
        self.visualizationWidget.hello()

        # TODO: check conversion from int to enum type
        # TODO: get value from a combobox
        samplingType = SamplingType.Independent # SamplingType(self.samplingTypeBox.selectedIndex())
        warpType = WarpType.NoWarp # WarpType(self.warpTypeBox.selectedIndex())
        parameterValue = self.mapParameter(warpType, self.parameterSlider.value())
        angle = 180 * self.angleSlider.value() - 90

        # Point count slider input is not linear
        pointCount = int(math.pow(2.0, 15.0 * self.pointCountSlider.value() + 5))
        self.pointCountSlider.setValue(
            (math.log(pointCount) / math.log(2.0) - 5) / 15.0);

        # TODO: special handling for microfacet BRDF


        # Generate the points' positions
        # TODO

        # Generate a color gradient
        # TODO

        # Upload the grid's lines to the GPU
        # TODO

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

        return True

    def draw(self, ctx):
        super(WarpVisualizer, self).draw(ctx)

    def drawHistogram(self, pos, size, tex):
        # TODO: should directly call super
        pass

    def mouseMotionEvent(self, p, rel, button, modifiers):
        if not super(WarpVisualizer, self).mouseMotionEvent(p, rel, button, modifiers):
            # TODO: arcball motion
            pass
        return True

    def mouseButtonEvent(self, p, button, down, modifiers):
        if down and (not self.window.visible()):
            self.drawHistogram = False
            self.window.setVisible(True)
            return True

        if not super(WarpVisualizer, self).mouseButtonEvent(p, button, down, modifiers):
            if button is glfw.MOUSE_BUTTON_1:
                # TODO: arcball button
                pass
        return True

    def keyboardEvent(self, key, scancode, action, modifiers):
        if super(WarpVisualizer, self).keyboardEvent(key, scancode,
                                                     action, modifiers):
            return True
        if (key is glfw.KEY_ESCAPE) and (action is glfw.PRESS):
            self.setVisible(False)
            return True
        return False


if __name__ == "__main__":
    nanogui.init()
    _ = WarpVisualizer()
    nanogui.mainloop()
    gc.collect()
    nanogui.shutdown()
