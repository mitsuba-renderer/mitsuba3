from __future__ import unicode_literals
import gc
import nanogui
import numpy as np

from ..test.test_chi2 import CHI2_TESTS

from nanogui import (Color, Screen, Window, Widget, GroupLayout, BoxLayout,
                     Vector2i, Label, Button, TextBox, CheckBox, MessageDialog,
                     ComboBox, Slider, Alignment, Orientation)

from nanogui import glfw, entypo


class WarpVisualizer(Screen):
    """
    Graphical user interface that visualizes and tests commonly used
    warping functions used to implement Monte Carlo integration techniques
    in the context of rendering.
    """

    def __init__(self):
        super(WarpVisualizer, self).__init__(
            Vector2i(800, 600), u"Warp visualizer & χ² hypothesis test")

        window = Window(self, "Warp tester")
        window.setPosition(Vector2i(15, 15))
        window.setLayout(GroupLayout())

        Label(window, "Input point set", "sans-bold")

        # ---------- First panel
        panel = Widget(window)
        panel.setLayout(
            BoxLayout(Orientation.Horizontal, Alignment.Middle, 0, 20))

        # Point count slider
        pointCountSlider = Slider(panel)
        pointCountSlider.setFixedWidth(55)
        pointCountSlider.setCallback(lambda _: self.refresh())
        pointCountSlider.setRange((10, 20))
        pointCountSlider.setValue(10)

        # Companion text box
        pointCountBox = TextBox(panel)
        pointCountBox.setFixedSize(Vector2i(80, 25))

        # Selection of sampling strategy
        sampleTypeBox = ComboBox(
            window, ["Independent", "Grid", "Stratified", "Halton"])
        sampleTypeBox.setCallback(lambda _: self.refresh())

        Label(window, "Warping method", "sans-bold")
        warpTypeBox = ComboBox(window, [item[0] for item in CHI2_TESTS])

        # Chi-2 test button
        Label(window, u"χ² hypothesis test", "sans-bold")
        testButton = Button(window, "Run", entypo.ICON_CHECK)
        testButton.setBackgroundColor(Color(0, 1., 0., 0.25))

        self.pointCountSlider = pointCountSlider
        self.pointCountBox = pointCountBox
        self.sampleTypeBox = sampleTypeBox

        self.refresh()
        return

        Label(window, "Warping method", "sans-bold")
        warpingNames = map(lambda w: self.warps[w].name, self.warps.keys())
        warpTypeBox = ComboBox(window, warpingNames)
        warpTypeBox.setCallback(warpTypeCallback)

        # Option to visualize the warped grid
        warpedGridCheckBox = CheckBox(window, "Visualize warped grid")
        warpedGridCheckBox.setCallback(lambda _: self.refresh())

        # ---------- Second panel
        Label(window, "Method parmeters", "sans-bold")

        warpParametersPanel = Widget(window)
        warpParametersPanel.setLayout(
            BoxLayout(Orientation.Vertical, Alignment.Middle, 0, 20))

        # ---------- Third panel
        Label(window, "BSDF parameters", "sans-bold")
        panel = Widget(window)
        panel.setLayout(
            BoxLayout(Orientation.Horizontal, Alignment.Middle, 0, 20))

        # TODO: BRDF-specific, some of this should be built-in the described args
        # # Angle BSDF parameter
        # angleSlider = Slider(panel)
        # angleSlider.setFixedWidth(55)
        # angleSlider.setValue(WarpVisualizer.angleDefaultValue)
        # angleSlider.setCallback(lambda _: self.refresh())
        # # Companion text box
        # angleBox = TextBox(panel)
        # angleBox.setFixedSize(Vector2i(80, 25))
        # angleBox.setUnits(u"\u00B0")
        # # Option to visualize the BRDF values
        # brdfValuesCheckBox = CheckBox(window, "Visualize BRDF values")
        # brdfValuesCheckBox.setCallback(lambda _: self.refresh())

    def keyboardEvent(self, key, scancode, action, modifiers):
        if super(WarpVisualizer, self).keyboardEvent(key, scancode, action,
                                                     modifiers):
            return True
        if key == glfw.KEY_ESCAPE and action == glfw.PRESS:
            self.setVisible(False)
            return True
        return False

    def refresh(self):
        # Point count slider input is not linear
        pointCount = np.power(2.0, self.pointCountSlider.value()).astype(int)
        self.pointCountSlider.setValue(np.log(pointCount) / np.log(2.0))

        # Update the companion box
        def formattedPointCount(n):
            if (n >= 1e6):
                self.pointCountBox.setUnits("M")
                return "{:.2f}".format(n / (1024 * 1024))
            if (n >= 1e3):
                self.pointCountBox.setUnits("K")
                return "{:.2f}".format(n / (1024))

            self.pointCountBox.setUnits(" ")
            return str(n)

        self.pointCountBox.setValue(formattedPointCount(pointCount))


if __name__ == "__main__":
    nanogui.init()
    wv = WarpVisualizer()
    wv.setVisible(True)
    wv.performLayout()
    nanogui.mainloop()
    gc.collect()
    nanogui.shutdown()
