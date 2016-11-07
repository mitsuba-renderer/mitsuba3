from __future__ import unicode_literals, print_function

import gc
import nanogui
import numpy as np

from nanogui import (Color, Screen, Window, Widget, GroupLayout, BoxLayout,
                     Label, Button, TextBox, CheckBox, MessageDialog,
                     ComboBox, Slider, Alignment, Orientation, GLShader,
                     Arcball, lookAt, translate, frustum)

from nanogui import glfw, entypo, gl
from nanogui import nanovg as nvg
from mitsuba.core import warp, pcg32, float_dtype, Bitmap
from mitsuba.core.chi2 import ChiSquareTest, SphericalDomain, PlanarDomain
from . import GLTexture

DEFAULT_PARAMETERS = {
    'sample_dim': 2
}

DISTRIBUTIONS = [
    # ('Uniform square', SphericalDomain(),
    # lambda x: x,
    # lambda x: np.ones(x.shape[1])),

    ('Uniform disk', PlanarDomain(),
     warp.squareToUniformDisk,
     warp.squareToUniformDiskPdf,
     DEFAULT_PARAMETERS),

    ('Uniform disk (concentric)', PlanarDomain(),
     warp.squareToUniformDiskConcentric,
     warp.squareToUniformDiskConcentricPdf,
     DEFAULT_PARAMETERS),

    ('Uniform sphere', SphericalDomain(),
     warp.squareToUniformSphere,
     warp.squareToUniformSpherePdf,
     DEFAULT_PARAMETERS),

    ('Uniform hemisphere', SphericalDomain(),
     warp.squareToUniformHemisphere,
     warp.squareToUniformHemispherePdf,
     DEFAULT_PARAMETERS),

    ('Cosine hemisphere', SphericalDomain(),
     warp.squareToCosineHemisphere,
     warp.squareToCosineHemispherePdf,
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
            [1024, 768], u'Warp visualizer & χ² hypothesis test')

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
        testButton.setCallback(lambda: self.run_test())

        self.point_count_slider = point_count_slider
        self.point_count_box = point_count_box
        self.sample_type_box = sample_type_box
        self.warp_type_box = warp_type_box
        self.warped_grid_box = warped_grid_box
        self.window = window
        self.test = None

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
            # Fragment shader
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

        self.grid_shader = GLShader()
        self.grid_shader.init('Sample shader (grids)',
            # Vertex shader
            '''
            #version 330
            uniform mat4 mvp;
            in vec3 position;
            void main() {
                gl_Position = mvp * vec4(position, 1.0);
            }
            ''',
            # Fragment shader
            '''
            #version 330
            out vec4 out_color;
            void main() {
                out_color = vec4(vec3(1.0), 0.4);
            }
            ''')

        self.histogram_shader = GLShader()
        self.histogram_shader.init('Histogram shader',
            # Vertex shader
            '''
            #version 330
            uniform mat4 mvp;
            in vec2 position;
            out vec2 uv;
            void main() {
                gl_Position = mvp * vec4(position, 0.0, 1.0);
                uv = position;
            }
            ''',
            # Fragment shader
            '''
            #version 330
            out vec4 out_color;
            uniform sampler2D tex;
            in vec2 uv;
            /* http://paulbourke.net/texture_colour/colourspace/ */
            vec3 colormap(float v, float vmin, float vmax) {
                vec3 c = vec3(1.0);
                if (v < vmin)
                    v = vmin;
                if (v > vmax)
                    v = vmax;
                float dv = vmax - vmin;
                if (v < (vmin + 0.25 * dv)) {
                    c.r = 0.0;
                    c.g = 4.0 * (v - vmin) / dv;
                } else if (v < (vmin + 0.5 * dv)) {
                    c.r = 0.0;
                    c.b = 1.0 + 4.0 * (vmin + 0.25 * dv - v) / dv;
                } else if (v < (vmin + 0.75 * dv)) {
                    c.r = 4.0 * (v - vmin - 0.5 * dv) / dv;
                    c.b = 0.0;
                } else {
                    c.g = 1.0 + 4.0 * (vmin + 0.75 * dv - v) / dv;
                    c.b = 0.0;
                }
                return c;
            }
            void main() {
                float value = texture(tex, uv).r;
                out_color = vec4(colormap(value, 0.0, 1.0), 1.0);
            }
            ''')

        self.arcball = Arcball()
        self.arcball.setSize(self.size())

        self.refresh()

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
        if self.test is not None:
            self.test = None
            self.window.setVisible(True)
            return True
        if button == glfw.MOUSE_BUTTON_1 and not down:
            self.arcball.button(p, down)
        if super(Screen, self).mouseButtonEvent(p, button, down, modifiers):
            return True
        if button == glfw.MOUSE_BUTTON_1:
            self.arcball.button(p, down)
            return True
        return False

    def drawContents(self):
        if self.test:
            self.draw_test_results()
        else:
            self.draw_point_cloud()

    def draw_point_cloud(self):
        view = lookAt([0, 0, 4], [0, 0, 0], [0, 1, 0])
        viewAngle, near, far = 30, 0.01, 100
        fH = np.tan(viewAngle / 360 * np.pi) * near
        fW = fH * self.size()[0] / self.size()[1]
        proj = frustum(-fW, fW, -fH, fH, near, far)

        model = self.arcball.matrix()

        mvp = np.dot(np.dot(proj, view), model)

        gl.PointSize(2)
        gl.Enable(gl.DEPTH_TEST)
        self.point_shader.bind()
        self.point_shader.setUniform('mvp', mvp)
        self.point_shader.drawArray(gl.POINTS, 0, self.point_count)

        if self.warped_grid_box.checked():
            self.grid_shader.bind()
            self.grid_shader.setUniform('mvp', mvp)
            self.grid_shader.drawArray(gl.LINES, 0, self.line_count)

    def draw_test_results(self):
        spacer = 20
        hist_width = (self.width() - 3 * spacer) / 2
        hist_height = hist_width
        voffset = (self.height() - hist_height) / 2

        ctx = self.nvgContext()
        ctx.BeginFrame(self.size()[0], self.size()[1], self.pixelRatio())
        ctx.BeginPath()
        ctx.Rect(spacer, voffset + hist_height + spacer,
                 self.width() - 2 * spacer, 70)
        ctx.FillColor(Color(100, 255, 100, 100)
                      if self.test_result else Color(255, 100, 100, 100))
        ctx.Fill()
        ctx.FontSize(24)
        ctx.FontFace("sans-bold")
        ctx.TextAlign(int(nvg.ALIGN_CENTER) | int(nvg.ALIGN_TOP))
        ctx.FillColor(Color(255, 255))
        ctx.Text(spacer + hist_width / 2, voffset - 3 * spacer,
                "Sample histogram")
        ctx.Text(2 * spacer + (hist_width * 3) / 2, voffset - 3 * spacer,
                 "Integrated density")
        ctx.StrokeColor(Color(255, 255))
        ctx.StrokeWidth(2)

        # self.draw_histogram(Vector2i(spacer, voffset), Vector2i(hist_width, hist_height), m_textures[0]);
        # self.draw_histogram(Vector2i(2*spacer + hist_width, voffset), Vector2i(hist_width, hist_height), m_textures[1]);

        ctx.BeginPath()
        ctx.Rect(spacer, voffset, hist_width, hist_height)
        ctx.Rect(2 * spacer + hist_width, voffset, hist_width, hist_height)
        ctx.Stroke()

        ctx.FontSize(20)
        ctx.TextAlign(int(nvg.ALIGN_CENTER) | int(nvg.ALIGN_TOP))
        bounds = ctx.TextBoxBounds(0, 0, self.width() - 2 * spacer,
                                   self.test.messages)
        ctx.TextBox(
            spacer, voffset + hist_height + spacer + (70 - bounds[3]) / 2,
            self.width() - 2 * spacer, self.test.messages)
        ctx.EndFrame()

    def refresh(self):
        # Look up configuration
        distr = DISTRIBUTIONS[self.warp_type_box.selectedIndex()]
        sample_type = self.sample_type_box.selectedIndex()
        # domain = distr[1]
        sample_func = distr[2]
        params = distr[4]
        sample_dim = params['sample_dim']

        # Point count slider input is not linear
        point_count = np.power(2, self.point_count_slider.value()).astype(int)
        sqrt_val = np.int32(np.sqrt(point_count) + 0.5)

        if sample_type == 1 or sample_type == 2:  # Point/Grid
            point_count = sqrt_val * sqrt_val

        self.point_count_slider.setValue(np.log(point_count) / np.log(2.0))

        # Update the companion box
        def formattedPointCount(n):
            if n >= 1e6:
                self.point_count_box.setUnits('M')
                return '{:.2f}'.format(n / (1024 * 1024))
            if n >= 1e3:
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
        elif sample_type == 3:  # Halton
            pass

        # Perform the warp
        samples = sample_func(samples_in)
        if samples.shape[0] == 2:
            samples = np.vstack([samples, np.zeros(samples.shape[1])])

        self.point_shader.bind()
        self.point_shader.uploadAttrib('position', samples)
        self.point_count = samples.shape[1]

        tmp = np.linspace(0, 1, self.point_count)
        colors = np.stack((tmp, 1 - tmp, np.zeros(self.point_count))) \
            .astype(samples.dtype)
        self.point_shader.uploadAttrib('color', colors)

        if self.warped_grid_box.checked():
            grid            = np.linspace(0, 1, sqrt_val + 1)
            fine_grid       = np.linspace(0, 1, 16 * sqrt_val)
            fine_grid       = np.insert(fine_grid, np.arange(len(fine_grid)),
                                        fine_grid)[1:-1]
            fine_grid, grid = np.array(
                np.meshgrid(fine_grid, grid),
                dtype=float_dtype
            )

            lines = sample_func(
                np.hstack([
                    np.vstack([grid.ravel(), fine_grid.ravel()]),
                    np.vstack([fine_grid.ravel(), grid.ravel()])
                ]))

            if lines.shape[0] == 2:
                lines = np.vstack([lines, np.zeros(lines.shape[1])])

            self.grid_shader.bind()
            self.grid_shader.uploadAttrib('position', lines)
            self.line_count = lines.shape[1]

    def run_test(self):
        distr = DISTRIBUTIONS[self.warp_type_box.selectedIndex()]
        domain = distr[1]
        sample_func = distr[2]
        pdf = distr[3]
        self.test = ChiSquareTest(domain, sample_func, pdf)
        self.test_result = self.test.run(0.01)
        self.window.setVisible(False)
        self.pdf_texture       = GLTexture(
            Bitmap(np.float32(self.test.pdf)))
        self.histogram_texture = GLTexture(
            Bitmap(np.float32(self.test.histogram)))

if __name__ == '__main__':
    nanogui.init()
    wv = WarpVisualizer()
    wv.setVisible(True)
    wv.performLayout()
    nanogui.mainloop()
    del wv
    gc.collect()
    nanogui.shutdown()
