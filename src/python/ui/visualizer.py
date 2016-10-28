from __future__ import unicode_literals, print_function

import gc
import nanogui
import numpy as np

from nanogui import (Color, Screen, Window, Widget, GroupLayout, BoxLayout,
                     Label, Button, TextBox, CheckBox, MessageDialog,
                     ComboBox, Slider, Alignment, Orientation)

from nanogui import glfw, entypo

from mitsuba.core import warp, pcg32
from mitsuba.core.chi2 import SphericalDomain

DEFAULT_PARAMETERS = {
    'sample_dim' : 2
}

DISTRIBUTIONS = [
    # ("Uniform square", SphericalDomain(),
     # lambda x: x,
     # lambda x: np.ones(x.shape[1])),

    ("Uniform sphere", SphericalDomain(),
     warp.squareToUniformSphere,
     warp.squareToUniformSpherePdf,
     DEFAULT_PARAMETERS),

    ("Uniform hemisphere", SphericalDomain(),
     warp.squareToUniformHemisphere,
     warp.squareToUniformHemispherePdf,
     DEFAULT_PARAMETERS),
]


class WarpVisualizer(Screen):
    """
    Graphical user interface that visualizes and tests commonly used
    warping functions used to implement Monte Carlo integration techniques
    in the context of rendering.
    """

    def __init__(self):
        super(WarpVisualizer, self).__init__(
            [800, 600], u"Warp visualizer & χ² hypothesis test")

        window = Window(self, "Warp tester")
        window.setPosition([15, 15])
        window.setLayout(GroupLayout())

        Label(window, "Input point set", "sans-bold")

        # ---------- First panel
        panel = Widget(window)
        panel.setLayout(
            BoxLayout(Orientation.Horizontal, Alignment.Middle, 0, 20))

        # Point count slider
        point_count_slider = Slider(panel)
        point_count_slider.setFixedWidth(55)
        point_count_slider.setCallback(lambda _: self.refresh())
        point_count_slider.setRange((10, 20))
        point_count_slider.setValue(10)

        # Companion text box
        point_count_box = TextBox(panel)
        point_count_box.setFixedSize([80, 25])

        # Selection of sampling strategy
        sampleTypeBox = ComboBox(
            window, ["Independent", "Grid", "Stratified", "Halton"])
        sampleTypeBox.setCallback(lambda _: self.refresh())

        Label(window, "Warping method", "sans-bold")
        warp_type_box = ComboBox(window, [item[0] for item in DISTRIBUTIONS])

        # Chi-2 test button
        Label(window, u"χ² hypothesis test", "sans-bold")
        testButton = Button(window, "Run", entypo.ICON_CHECK)
        testButton.setBackgroundColor(Color(0, 1., 0., 0.10))

        self.point_count_slider = point_count_slider
        self.point_count_box = point_count_box
        self.sampleTypeBox = sampleTypeBox
        self.warp_type_box = warp_type_box

        self.refresh()
        return

        Label(window, "Warping method", "sans-bold")
        warping_names = map(lambda w: self.warps[w].name, self.warps.keys())
        warp_type_box = ComboBox(window, warping_names)
        warp_type_box.setCallback(warp_typeCallback)

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
        # angleBox.setFixedSize([80, 25])
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
        point_count = np.power(2.0, self.point_count_slider.value()).astype(int)
        self.point_count_slider.setValue(np.log(point_count) / np.log(2.0))

        # Update the companion box
        def formattedPointCount(n):
            if (n >= 1e6):
                self.point_count_box.setUnits("M")
                return "{:.2f}".format(n / (1024 * 1024))
            if (n >= 1e3):
                self.point_count_box.setUnits("K")
                return "{:.2f}".format(n / (1024))

            self.point_count_box.setUnits(" ")
            return str(n)

        self.point_count_box.setValue(formattedPointCount(point_count))

        distr = DISTRIBUTIONS[self.warp_type_box.selectedIndex()]
        domain = distr[1]
        sample_func = distr[2]
        params = distr[4]
        print(params)

        # Generate a table of uniform variates
        print(point_count)
        print(type(point_count))
        samples_in = pcg32().nextFloat(params['sample_dim'], point_count)
        print(samples_in)




if __name__ == "__main__":
    nanogui.init()
    wv = WarpVisualizer()
    wv.setVisible(True)
    wv.performLayout()
    nanogui.mainloop()
    del wv
    gc.collect()
    nanogui.shutdown()
