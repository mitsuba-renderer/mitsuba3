from __future__ import unicode_literals, print_function

import gc
import nanogui
import numpy as np

from nanogui import (Color, Screen, Window, Widget, GroupLayout, BoxLayout,
                     Label, Button, TextBox, CheckBox, MessageDialog,
                     ComboBox, Slider, Alignment, Orientation, GLShader,
                     Arcball, lookAt, translate, frustum)

from nanogui import glfw, entypo, gl
from mitsuba.core import warp, pcg32, float_dtype
from mitsuba.core.chi2 import SphericalDomain

DEFAULT_PARAMETERS = {
    'sample_dim': 2
}

DISTRIBUTIONS = [
    # ('Uniform square', SphericalDomain(),
    # lambda x: x,
    # lambda x: np.ones(x.shape[1])),

    ('Uniform sphere', SphericalDomain(),
     warp.squareToUniformSphere,
     warp.squareToUniformSpherePdf,
     DEFAULT_PARAMETERS),

    ('Uniform hemisphere', SphericalDomain(),
     warp.squareToUniformHemisphere,
     warp.squareToUniformHemispherePdf,
     DEFAULT_PARAMETERS),
]


class WarpVisualizer(Screen):
    '''
    Graphical user interface that visualizes and tests commonly used
    warping functions used to implement Monte Carlo integration techniques
    in the context of rendering.
    '''

    def __init__(self):
        super(WarpVisualizer, self).__init__(
            [800, 600], u'Warp visualizer & χ² hypothesis test')

        window = Window(self, 'Warp tester')
        window.setPosition([15, 15])
        window.setLayout(GroupLayout())

        Label(window, 'Input point set', 'sans-bold')

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
        sample_type_box = ComboBox(
            window, ['Independent', 'Grid', 'Stratified', 'Halton'])
        sample_type_box.setCallback(lambda _: self.refresh())

        Label(window, 'Warping method', 'sans-bold')
        warp_type_box = ComboBox(window, [item[0] for item in DISTRIBUTIONS])
        warp_type_box.setCallback(lambda _: self.refresh())

        # Option to visualize the warped grid
        warped_grid_box = CheckBox(window, 'Visualize warped grid')
        warped_grid_box.setCallback(lambda _: self.refresh())

        # ---------- Second panel
        Label(window, 'Method parameters', 'sans-bold')

        # Chi-2 test button
        Label(window, u'χ² hypothesis test', 'sans-bold')
        testButton = Button(window, 'Run', entypo.ICON_CHECK)
        testButton.setBackgroundColor(Color(0, 1., 0., 0.10))

        self.point_count_slider = point_count_slider
        self.point_count_box = point_count_box
        self.sample_type_box = sample_type_box
        self.warp_type_box = warp_type_box

        if False:

            warpParametersPanel = Widget(window)
            warpParametersPanel.setLayout(
                BoxLayout(Orientation.Vertical, Alignment.Middle, 0, 20))

            # ---------- Third panel
            Label(window, 'BSDF parameters', 'sans-bold')
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
            # angleBox.setUnits(u'\u00B0')
            # # Option to visualize the BRDF values
            # brdfValuesCheckBox = CheckBox(window, 'Visualize BRDF values')
            # brdfValuesCheckBox.setCallback(lambda _: self.refresh())

        self.point_shader = GLShader()
        self.point_shader.init('Sample shader (points)',
            # Vertex shader
            '''
            #version 330
            uniform mat4 mvp;
            in vec3 position;
            in vec3 color;
            out vec3 frag_color;
            void main() {
                gl_Position = mvp * vec4(position, 1.0);
                if (isnan(position.r)) /* nan (missing value) */
                    frag_color = vec3(0.0);
                else
                    frag_color = color;
            }
            ''',
            # Fragment Shader
            '''
            #version 330
            in vec3 frag_color;
            out vec4 out_color;
            void main() {
                if (frag_color == vec3(0.0))
                    discard;
                out_color = vec4(frag_color, 1.0);
            }
            ''')

        self.arcball = Arcball()
        self.arcball.setSize(self.size())

        self.refresh()

    def drawContents(self):
        view = lookAt([0, 0, 2], [0, 0, 0], [0, 1, 0])
        viewAngle, near, far = 30, 0.01, 100
        fH = np.tan(viewAngle / 360 * np.pi) * near
        fW = fH * self.size()[0] / self.size()[1]
        proj = frustum(-fW, fW, -fH, fH, near, far)

        model = translate([-0.5, -0.5, 0.0])
        model = np.dot(self.arcball.matrix(), model)

        mvp = np.dot(np.dot(proj, view), model)

        gl.PointSize(2)
        gl.Enable(gl.DEPTH_TEST)
        self.point_shader.bind()
        self.point_shader.setUniform('mvp', mvp)
        self.point_shader.drawArray(gl.POINTS, 0, self.point_count)

    def resizeEvent(self, size):
        if not hasattr(self, 'arcball'):
            return False
        self.arcball.setSize(size)
        return True

    def keyboardEvent(self, key, scancode, action, modifiers):
        if super(WarpVisualizer, self).keyboardEvent(key, scancode, action,
                                                     modifiers):
            return True
        if key == glfw.KEY_ESCAPE and action == glfw.PRESS:
            self.setVisible(False)
            return True
        return False

    def mouseMotionEvent(self, p, rel, button, modifiers):
        if super(Screen, self).mouseMotionEvent(p, rel, button, modifiers):
            return True
        self.arcball.motion(p)
        return True

    def mouseButtonEvent(self, p, button, down, modifiers):
        if button == glfw.MOUSE_BUTTON_1 and not down:
            self.arcball.button(p, down)
        if super(Screen, self).mouseButtonEvent(p, button, down, modifiers):
            return True
        if button == glfw.MOUSE_BUTTON_1:
            self.arcball.button(p, down)
            return True
        return False

    def refresh(self):
        # Look up configuration
        distr = DISTRIBUTIONS[self.warp_type_box.selectedIndex()]
        sample_type = self.sample_type_box.selectedIndex()
        domain = distr[1]
        sample_func = distr[2]
        params = distr[4]
        sample_dim = params['sample_dim']

        # Point count slider input is not linear
        point_count = np.power(2, self.point_count_slider.value()).astype(int)

        if sample_type == 1 or sample_type == 2:  # Point/Grid
            sqrt_val = np.int32(np.sqrt(point_count) + 0.5)
            point_count = sqrt_val * sqrt_val

        self.point_count_slider.setValue(np.log(point_count) / np.log(2.0))

        # Update the companion box
        def formattedPointCount(n):
            if (n >= 1e6):
                self.point_count_box.setUnits('M')
                return '{:.2f}'.format(n / (1024 * 1024))
            if (n >= 1e3):
                self.point_count_box.setUnits('K')
                return '{:.2f}'.format(n / (1024))

            self.point_count_box.setUnits(' ')
            return str(n)

        self.point_count_box.setValue(formattedPointCount(point_count))

        if sample_type == 0:  # Uniform
            samples_in = pcg32().nextFloat(sample_dim, point_count)
        elif sample_type == 1:  # Grid
            x, y = np.mgrid[0:sqrt_val, 0:sqrt_val]
            x = (x.ravel() + 0.5) / sqrt_val
            y = (y.ravel() + 0.5) / sqrt_val
            samples_in = np.stack((y, x)).astype(float_dtype)
        elif sample_type == 2:  # Stratified
            samples_in = pcg32().nextFloat(sample_dim, point_count)
            x, y = np.mgrid[0:sqrt_val, 0:sqrt_val]
            x = (x.ravel() + samples_in[0, :]) / sqrt_val
            y = (y.ravel() + samples_in[1, :]) / sqrt_val
            samples_in = np.stack((y, x)).astype(float_dtype)

        # Perform the warp
        samples = sample_func(samples_in)

        samples *= 0.5
        samples += np.array([[0.5, 0.5, 0]]).T

        self.point_shader.bind()
        self.point_shader.uploadAttrib('position', samples)
        self.point_count = samples.shape[1]

        tmp = np.linspace(0, 1, self.point_count)
        colors = np.stack((tmp, 1 - tmp, np.zeros(self.point_count))) \
            .astype(samples.dtype)
        self.point_shader.uploadAttrib('color', colors)


if __name__ == '__main__':
    nanogui.init()
    wv = WarpVisualizer()
    wv.setVisible(True)
    wv.performLayout()
    nanogui.mainloop()
    del wv
    gc.collect()
    nanogui.shutdown()
