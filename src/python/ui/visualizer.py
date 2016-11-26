from __future__ import unicode_literals, print_function

import gc
import nanogui
import numpy as np

from nanogui import (Color, Screen, Window, Widget, GroupLayout, BoxLayout,
                     Label, Button, TextBox, CheckBox, ComboBox, Slider,
                     Alignment, Orientation, GLShader, Arcball, lookAt,
                     frustum)

from nanogui import glfw, entypo, gl
from nanogui import nanovg as nvg
from mitsuba.core import pcg32, float_dtype, Bitmap
from mitsuba.core.chi2 import ChiSquareTest, SphericalDomain
from mitsuba.core.warp.distr import DISTRIBUTIONS
from . import GLTexture


class WarpVisualizer(Screen):
    '''
    Graphical user interface that visualizes and tests commonly used
    warping functions used to implement Monte Carlo integration techniques
    in the context of rendering.
    '''

    def __init__(self):
        super(WarpVisualizer, self).__init__(
            [800, 600], 'Warp visualizer & χ² hypothesis test')

        self.setBackground(Color(0, 0))
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
        warp_type_box.setCallback(lambda _: self.refresh_distribution())

        # Option to visualize the warped grid
        warped_grid_box = CheckBox(window, 'Visualize warped grid')
        warped_grid_box.setCallback(lambda _: self.refresh())

        # Option to scale the grid according to the density function
        density_box = CheckBox(window, 'Visualize density')
        density_box.setCallback(lambda _: self.refresh())

        # Chi-2 test button
        Label(window, 'χ² hypothesis test', 'sans-bold')
        testButton = Button(window, 'Run', entypo.ICON_CHECK)
        testButton.setBackgroundColor(Color(0, 1., 0., 0.10))
        testButton.setCallback(lambda: self.run_test())

        self.point_count_slider = point_count_slider
        self.point_count_box = point_count_box
        self.sample_type_box = sample_type_box
        self.warp_type_box = warp_type_box
        self.warped_grid_box = warped_grid_box
        self.density_box = density_box
        self.window = window
        self.test = None
        self.center = None
        self.scale = None
        self.parameter_widgets = []

        self.point_shader = GLShader()
        self.point_shader.init(
            'Sample shader (points)',
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
        self.grid_shader.init(
            'Sample shader (grids)',
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
        self.histogram_shader.init(
            'Histogram shader',
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
        self.histogram_shader.bind()
        self.histogram_shader.uploadAttrib(
            "position",
            np.array(
                [[0, 1, 1, 0], [0, 0, 1, 1]], dtype=np.float32))
        self.histogram_shader.uploadIndices(
            np.array(
                [[0, 2], [1, 3], [2, 0]], dtype=np.uint32))
        self.arcball = Arcball()
        self.arcball.setSize(self.size())
        self.pdf_texture = GLTexture()
        self.histogram_texture = GLTexture()

        self.refresh_distribution()

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
            gl.Enable(gl.BLEND)
            gl.BlendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA)
            self.grid_shader.bind()
            self.grid_shader.setUniform('mvp', mvp)
            self.grid_shader.drawArray(gl.LINES, 0, self.line_count)
            gl.Disable(gl.BLEND)

    def draw_test_results(self):
        spacer = 20
        shape = self.test.pdf.shape
        hist_width = (self.width() - 3 * spacer) / 2
        hist_height = np.int32(shape[0] / shape[1] * hist_width)
        voffset = (self.height() - hist_height) / 2

        self.draw_histogram(
            np.array([spacer, voffset]),
            np.array([hist_width, hist_height]), self.histogram_texture)

        self.draw_histogram(
            np.array([2 * spacer + hist_width, voffset]),
            np.array([hist_width, hist_height]), self.pdf_texture)

        c = self.nvgContext()
        c.BeginFrame(self.size()[0], self.size()[1], self.pixelRatio())
        c.BeginPath()
        c.Rect(spacer, voffset + hist_height + spacer,
               self.width() - 2 * spacer, 70)
        c.FillColor(
            Color(100, 255, 100, 100)
            if self.test_result else Color(255, 100, 100, 100))
        c.Fill()
        c.FontSize(24)
        c.FontFace("sans-bold")
        c.TextAlign(int(nvg.ALIGN_CENTER) | int(nvg.ALIGN_TOP))
        c.FillColor(Color(255, 255))
        c.Text(spacer + hist_width / 2, voffset - 3 * spacer,
               "Sample histogram")
        c.Text(2 * spacer + (hist_width * 3) / 2, voffset - 3 * spacer,
               "Integrated density")
        c.StrokeColor(Color(255, 255))
        c.StrokeWidth(2)

        c.BeginPath()
        c.Rect(spacer, voffset, hist_width, hist_height)
        c.Rect(2 * spacer + hist_width, voffset, hist_width, hist_height)
        c.Stroke()

        c.FontSize(20)
        c.TextAlign(int(nvg.ALIGN_CENTER) | int(nvg.ALIGN_TOP))
        bounds = c.TextBoxBounds(0, 0,
                                 self.width() - 2 * spacer, self.test.messages)
        c.TextBox(spacer,
                  voffset + hist_height + spacer + (70 - bounds[3]) / 2,
                  self.width() - 2 * spacer, self.test.messages)
        c.EndFrame()

    def draw_histogram(self, pos, size, tex):
        s = -(pos + 0.25) / size
        e = self.size() / size + s
        mvp = nanogui.ortho(s[0], e[0], e[1], s[1], -1, 1)

        gl.Disable(gl.DEPTH_TEST)
        tex.bind(0)
        self.histogram_shader.bind()
        self.histogram_shader.setUniform("mvp", mvp)
        self.histogram_shader.setUniform("tex", 0)
        self.histogram_shader.drawIndexed(gl.TRIANGLES, 0, 2)
        tex.release()

    def refresh_distribution(self):
        distr = DISTRIBUTIONS[self.warp_type_box.selectedIndex()]
        settings = distr[4]
        for widget in self.parameter_widgets:
            self.window.removeChild(widget)
        self.parameter_widgets = []
        self.parameters = []
        self.center = None
        self.scale = None
        index = 0
        for name, vrange in settings['parameters']:
            label = Label(None, name, 'sans-bold')
            panel = Widget(None)
            panel.setLayout(
                BoxLayout(Orientation.Horizontal, Alignment.Middle, 0, 10))
            slider = Slider(panel)
            slider.setFixedWidth(70)
            slider.setRange((vrange[0], vrange[1]))
            slider.setValue(vrange[2])
            tbox = TextBox(panel)
            tbox.setFixedSize([80, 25])
            tbox.setValue("%.2f" % vrange[2])
            self.parameters.append(vrange[2])

            def slider_callback(value, index=index, tbox=tbox):
                tbox.setValue("%.2f" % value)
                self.parameters[index] = value
                self.refresh()

            slider.setCallback(slider_callback)
            self.parameter_widgets += [label, panel]
            index += 1

        for index, widget in enumerate(self.parameter_widgets):
            self.window.addChild(6 + index, widget)
        self.performLayout()
        self.refresh()

    def refresh(self):
        # Look up configuration
        distr = DISTRIBUTIONS[self.warp_type_box.selectedIndex()]
        sample_type = self.sample_type_box.selectedIndex()

        sample_func = lambda *args: distr[2](*(list(args) + self.parameters))
        pdf_func = lambda *args: distr[3](*(list(args) + self.parameters))

        domain = distr[1]
        settings = distr[4]
        sample_dim = settings['sample_dim']

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

        self.warped_grid_box.setEnabled(sample_dim == 2)
        if not self.warped_grid_box.enabled():
            self.warped_grid_box.setChecked(False)

        self.density_box.setEnabled(self.warped_grid_box.checked())
        if not self.density_box.enabled():
            self.density_box.setChecked(False)

        if sample_type == 0:  # Uniform
            samples_in = pcg32().nextFloat(point_count, sample_dim)
        elif sample_type == 1:  # Grid
            x, y = np.mgrid[0:sqrt_val, 0:sqrt_val]
            x = (x.ravel() + 0.5) / sqrt_val
            y = (y.ravel() + 0.5) / sqrt_val
            samples_in = np.column_stack((y, x)).astype(float_dtype)
        elif sample_type == 2:  # Stratified
            samples_in = pcg32().nextFloat(sample_dim, point_count)
            x, y = np.mgrid[0:sqrt_val, 0:sqrt_val]
            x = (x.ravel() + samples_in[0, :]) / sqrt_val
            y = (y.ravel() + samples_in[1, :]) / sqrt_val
            samples_in = np.column_stack((y, x)).astype(float_dtype)
        elif sample_type == 3:  # Halton
            pass

        # Perform the warp
        samples = sample_func(samples_in)

        if self.density_box.checked():
            pdfs = pdf_func(samples)
            pdfs /= np.amax(pdfs)
            samples *= pdfs[..., np.newaxis]

        if samples.shape[1] == 2:
            samples = np.column_stack(
                [samples, np.zeros(
                    samples.shape[0], dtype=samples.dtype)])

        if self.center is None:
            if isinstance(domain, SphericalDomain):
                self.center = np.array([0, 0, 0], dtype=float_dtype)
                self.scale = 1
            else:
                self.center = samples.mean(axis=0)
                self.scale = 2 / (np.amax(samples) - np.amin(samples))

        samples -= self.center
        samples *= self.scale

        self.point_shader.bind()
        self.point_shader.uploadAttrib('position', samples.T)
        self.point_count = samples.shape[0]

        tmp = np.linspace(0, 1, self.point_count)
        colors = np.stack((tmp, 1 - tmp, np.zeros(self.point_count))) \
            .astype(samples.dtype)
        self.point_shader.uploadAttrib('color', colors)

        if self.warped_grid_box.checked():
            grid = np.linspace(0, 1, sqrt_val + 1)
            fine_grid = np.linspace(0, 1, 16 * sqrt_val)
            fine_grid = np.insert(fine_grid,
                                  np.arange(len(fine_grid)), fine_grid)[1:-1]
            fine_grid, grid = np.array(
                np.meshgrid(fine_grid, grid), dtype=float_dtype)

            lines = sample_func(
                np.column_stack([
                    np.hstack([grid.ravel(), fine_grid.ravel()]), np.hstack(
                        [fine_grid.ravel(), grid.ravel()])
                ]))

            if self.density_box.checked():
                pdfs = pdf_func(lines)
                pdfs /= np.amax(pdfs)
                lines *= pdfs[..., np.newaxis]

            if lines.shape[1] == 2:
                lines = np.column_stack(
                    [lines, np.zeros(
                        lines.shape[0], dtype=lines.dtype)])

            lines -= self.center
            lines *= self.scale

            self.grid_shader.bind()
            self.grid_shader.uploadAttrib('position', lines.T)
            self.line_count = lines.shape[0]

    def run_test(self):
        distr = DISTRIBUTIONS[self.warp_type_box.selectedIndex()]
        domain = distr[1]
        settings = distr[4]

        self.test = ChiSquareTest(
            domain=domain,
            sample_func=lambda *args: distr[2](
                *(list(args) + self.parameters)),
            pdf_func=lambda *args: distr[3](
                *(list(args) + self.parameters)),
            res=settings['res'],
            ires=settings['ires'],
            sample_dim=settings['sample_dim'],
        )
        self.test_result = self.test.run(0.01)
        self.window.setVisible(False)

        # Convert the histogram & integrated PDF to normalized textures
        pdf = self.test.pdf
        histogram = self.test.histogram
        min_value = np.amin([np.amin(pdf), np.amin(histogram)]) / 2
        max_value = np.amax(pdf) * 1.1
        pdf = (pdf - min_value) / (max_value - min_value)
        histogram = (histogram - min_value) / (max_value - min_value)
        self.pdf_texture.init(Bitmap(np.float32(pdf)))
        self.pdf_texture.setInterpolation(GLTexture.ENearest)
        self.histogram_texture.init(Bitmap(np.float32(histogram)))
        self.histogram_texture.setInterpolation(GLTexture.ENearest)


if __name__ == '__main__':
    nanogui.init()
    wv = WarpVisualizer()
    wv.setVisible(True)
    wv.performLayout()
    nanogui.mainloop()
    del wv
    gc.collect()
    nanogui.shutdown()
