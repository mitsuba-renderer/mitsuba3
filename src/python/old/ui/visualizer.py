from __future__ import unicode_literals, print_function

import gc
import nanogui
import numpy as np
import os

from nanogui import (Color, Screen, Window, Widget, GroupLayout, BoxLayout,
                     Label, Button, TextBox, CheckBox, ComboBox, Slider,
                     Alignment, Orientation, Shader, Arcball, RenderPass,
                     look_at, frustum, ortho)

from nanogui import glfw, icons
from nanogui import nanovg as nvg
from mitsuba.scalar_rgb.core import PCG32, float_dtype, Bitmap, RadicalInverse
from mitsuba.core.chi2 import ChiSquareTest, SphericalDomain
from mitsuba.scalar_rgb.core.warp.distr import DISTRIBUTIONS
from mitsuba.ui import Texture


def load_shader(name):
    name = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        name + '_' + nanogui.api + '.shader')
    with open(name, 'r') as f:
        return f.read()


class WarpVisualizer(Screen):
    '''
    Graphical user interface that visualizes and tests commonly used
    warping functions used to implement Monte Carlo integration techniques
    in the context of rendering.
    '''

    def __init__(self):
        super(WarpVisualizer, self).__init__(
            [1024, 768], 'Warp visualizer & χ² hypothesis test')

        self.set_background(Color(0, 0))
        window = Window(self, 'Warp tester')
        window.set_position([15, 15])
        window.set_layout(GroupLayout())

        Label(window, 'Input point set', 'sans-bold')

        # ---------- First panel
        panel = Widget(window)
        panel.set_layout(
            BoxLayout(Orientation.Horizontal, Alignment.Middle, 0, 20))

        # Point count slider
        point_count_slider = Slider(panel)
        point_count_slider.set_fixed_width(55)
        point_count_slider.set_callback(lambda _: self.refresh())
        point_count_slider.set_range((10, 20))
        point_count_slider.set_value(10)

        # Companion text box
        point_count_box = TextBox(panel)
        point_count_box.set_fixed_size([80, 25])

        # Selection of sampling strategy
        sample_type_box = ComboBox(
            window, ['Independent', 'Grid', 'Stratified', 'Halton'])
        sample_type_box.set_callback(lambda _: self.refresh())

        Label(window, 'Warping method', 'sans-bold')
        warp_type_box = ComboBox(window, [item[0] for item in DISTRIBUTIONS])
        warp_type_box.set_callback(lambda _: self.refresh_distribution())

        # Option to visualize the warped grid
        warped_grid_box = CheckBox(window, 'Visualize warped grid')
        warped_grid_box.set_callback(lambda _: self.refresh())

        # Option to scale the grid according to the density function
        density_box = CheckBox(window, 'Visualize density')
        density_box.set_callback(lambda _: self.refresh())

        # Chi-2 test button
        Label(window, 'χ² hypothesis test', 'sans-bold')
        testButton = Button(window, 'Run', icons.FA_CHECK)
        testButton.set_background_color(Color(0, 1., 0., 0.10))
        testButton.set_callback(lambda: self.run_test())

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
        self.pending_refresh = 0

        self.render_pass = RenderPass([self])

        self.point_shader = Shader(
            render_pass=self.render_pass,
            name='Sample shader (points)',
            vertex_shader=load_shader('visualizer_points_vert'),
            fragment_shader=load_shader('visualizer_points_frag')
        )

        self.grid_shader = Shader(
            render_pass=self.render_pass,
            name='Sample shader (grids)',
            vertex_shader=load_shader('visualizer_grid_vert'),
            fragment_shader=load_shader('visualizer_grid_frag'),
            blend_mode=Shader.BlendMode.AlphaBlend
        )

        self.bsdf_shader = Shader(
            render_pass=self.render_pass,
            name='Sample shader (BSDFs)',
            vertex_shader=load_shader('visualizer_grid_vert'),
            fragment_shader=load_shader('visualizer_grid_frag')
        )

        self.histogram_shader = Shader(
            render_pass=self.render_pass,
            name='Histogram shader',
            vertex_shader=load_shader('visualizer_histogram_vert'),
            fragment_shader=load_shader('visualizer_histogram_frag')
        )

        self.histogram_shader.set_buffer(
            "position",
            np.array([[0, 0], [1, 0], [1, 1], [0, 1]], dtype=np.float32))
        self.histogram_shader.set_buffer(
            "indices",
            np.array([0, 2, 1, 3, 2, 0], dtype=np.uint32))
        self.arcball = Arcball()
        self.arcball.set_size(self.size())
        self.ri = RadicalInverse()

        self.refresh_distribution()

    def resize_event(self, size):
        if not hasattr(self, 'arcball'):
            return False
        self.arcball.set_size(size)
        return True

    def keyboard_event(self, key, scancode, action, modifiers):
        if super(WarpVisualizer, self).keyboard_event(key, scancode, action,
                                                      modifiers):
            return True
        if key == glfw.KEY_ESCAPE and action == glfw.PRESS:
            self.set_visible(False)
            return True
        return False

    def mouse_motion_event(self, p, rel, button, modifiers):
        if super(Screen, self).mouse_motion_event(p, rel, button, modifiers):
            return True
        return self.arcball.motion(p)

    def mouse_button_event(self, p, button, down, modifiers):
        if self.test is not None:
            self.test = None
            self.window.set_visible(True)
            return True
        if button == glfw.MOUSE_BUTTON_1 and not down:
            self.arcball.button(p, down)
        if super(Screen, self).mouse_button_event(p, button, down, modifiers):
            return True
        if button == glfw.MOUSE_BUTTON_1:
            self.arcball.button(p, down)
            return True
        return False

    def draw_contents(self):
        self.render_pass.resize(self.framebuffer_size())
        if self.test:
            self.draw_test_results()
        else:
            self.draw_point_cloud()

        missed_refreshes = self.pending_refresh > 1
        self.pending_refresh = 0
        if missed_refreshes:
            self.refresh()
            self.redraw()

    def draw_point_cloud(self):
        with self.render_pass:
            view = look_at([0, 0, 4], [0, 0, 0], [0, 1, 0])
            view_angle, near, far = 30, 0.01, 100
            fH = np.tan(view_angle / 360 * np.pi) * near
            fW = fH * self.size()[0] / self.size()[1]
            proj = frustum(-fW, fW, -fH, fH, near, far)

            model = self.arcball.matrix()

            mvp = np.dot(np.dot(proj, view), model).T

            self.point_shader.set_buffer('mvp', mvp)
            with self.point_shader:
                self.point_shader.draw_array(Shader.PrimitiveType.Point,
                    0, self.point_count, False)

            if self.warped_grid_box.checked():
                self.grid_shader.set_buffer('mvp', mvp)
                with self.grid_shader:
                    self.grid_shader.draw_array(Shader.PrimitiveType.Line,
                                                0, self.line_count, False)

            distr = DISTRIBUTIONS[self.warp_type_box.selected_index()]
            self.bsdf_shader.set_buffer('mvp', mvp)
            if 'InteractiveBSDF' in repr(distr[2][0]):
                with self.bsdf_shader:
                    self.bsdf_shader.draw_array(Shader.PrimitiveType.Line,
                                                0, 104, False)

    def draw_test_results(self):
        spacer = 20
        shape = self.test.pdf.shape
        hist_width = (self.width() - 3 * spacer) / 2
        hist_height = np.int32(shape[0] / shape[1] * hist_width)
        voffset = (self.height() - hist_height) / 2

        with self.render_pass:
            self.draw_histogram(
                np.array([spacer, voffset]),
                np.array([hist_width, hist_height]), self.histogram_texture)

            self.draw_histogram(
                np.array([2 * spacer + hist_width, voffset]),
                np.array([hist_width, hist_height]), self.pdf_texture)

        c = self.nvg_context()
        c.BeginFrame(self.size()[0], self.size()[1], self.pixel_ratio())
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
        mvp = ortho(s[0], e[0], e[1], s[1], -1, 1).T

        self.histogram_shader.set_buffer("mvp", mvp)
        self.histogram_shader.set_texture("tex", tex)

        with self.histogram_shader:
            self.histogram_shader.draw_array(Shader.PrimitiveType.Triangle,
                                             0, 6, True)

    def refresh_distribution(self):
        distr = DISTRIBUTIONS[self.warp_type_box.selected_index()]
        settings = distr[3]
        for widget in self.parameter_widgets:
            self.window.remove_child(widget)
        self.parameter_widgets = []
        self.parameters = []
        self.center = None
        self.scale = None
        index = 0
        for name, vrange in settings['parameters']:
            label = Label(None, name, 'sans-bold')
            panel = Widget(None)
            panel.set_layout(
                BoxLayout(Orientation.Horizontal, Alignment.Middle, 0, 10))
            slider = Slider(panel)
            slider.set_fixed_width(70)
            slider.set_range((vrange[0], vrange[1]))
            slider.set_value(vrange[2])
            tbox = TextBox(panel)
            tbox.set_fixed_size([80, 25])
            tbox.set_value("%.2f" % vrange[2])
            self.parameters.append(vrange[2])

            def slider_callback(value, index=index, tbox=tbox):
                tbox.set_value("%.2f" % value)
                self.parameters[index] = value
                self.refresh()

            slider.set_callback(slider_callback)
            self.parameter_widgets += [label, panel]
            index += 1

        for index, widget in enumerate(self.parameter_widgets):
            self.window.add_child(7 + index, widget)
        self.perform_layout()
        self.refresh()

    def refresh(self):
        self.pending_refresh += 1
        if self.pending_refresh > 1:
            return

        distr = DISTRIBUTIONS[self.warp_type_box.selected_index()]
        sample_type = self.sample_type_box.selected_index()
        sample, pdf = distr[2]

        sample_func = lambda *args: sample(*(list(args) + self.parameters))
        pdf_func = lambda *args: pdf(*(list(args) + self.parameters))

        domain = distr[1]
        settings = distr[3]
        sample_dim = settings['sample_dim']

        # Point count slider input is not linear
        point_count = np.power(2, self.point_count_slider.value()).astype(int)
        sqrt_val = np.int32(np.sqrt(point_count) + 0.5)

        if sample_type == 1 or sample_type == 2:  # Point/Grid
            point_count = sqrt_val * sqrt_val

        self.point_count_slider.set_value(np.log(point_count) / np.log(2.0))

        # Update the companion box
        def formatted_point_count(n):
            if n >= 1e6:
                self.point_count_box.set_units('M')
                return '{:.2f}'.format(n / (1024 * 1024))
            if n >= 1e3:
                self.point_count_box.set_units('K')
                return '{:.2f}'.format(n / (1024))

            self.point_count_box.set_units(' ')
            return str(n)

        self.point_count_box.set_value(formatted_point_count(point_count))

        self.warped_grid_box.set_enabled(sample_dim == 2)
        if not self.warped_grid_box.enabled():
            self.warped_grid_box.set_checked(False)

        if not self.density_box.enabled():
            self.density_box.set_checked(False)

        if sample_type == 0:  # Uniform
            samples_in = PCG32().next_float((point_count, sample_dim))
        elif sample_type == 1:  # Grid
            x, y = np.mgrid[0:sqrt_val, 0:sqrt_val]
            x = (x.ravel() + 0.5) / sqrt_val
            y = (y.ravel() + 0.5) / sqrt_val
            samples_in = np.column_stack((y, x)).astype(float_dtype)
        elif sample_type == 2:  # Stratified
            samples_in = PCG32().next_float((sample_dim, point_count))
            x, y = np.mgrid[0:sqrt_val, 0:sqrt_val]
            x = (x.ravel() + samples_in[0, :]) / sqrt_val
            y = (y.ravel() + samples_in[1, :]) / sqrt_val
            samples_in = np.column_stack((y, x)).astype(float_dtype)
        elif sample_type == 3:  # Halton
            indices = np.arange(1, point_count + 1, dtype = np.uint64)
            samples_in = np.column_stack(
                [self.ri.eval_scrambled(dim, indices) for dim in range(sample_dim)]
            ).astype(float_dtype)

        if sample_dim == 1:
            samples_in = samples_in.ravel()

        # Perform the warp
        samples = sample_func(samples_in)
        if type(samples) is tuple:
            samples = samples[0]

        if self.density_box.checked():
            pdfs = pdf_func(samples)
            pdfs /= np.amax(pdfs)
            samples *= pdfs[..., np.newaxis]

        if samples.ndim == 1:
            samples = np.column_stack(
                [samples, np.zeros(
                    samples.shape[0], dtype=samples.dtype)])

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

        self.point_shader.set_buffer('position', samples)
        self.point_count = samples.shape[0]

        tmp = np.linspace(0, 1, self.point_count)
        colors = np.stack((tmp, 1 - tmp, np.zeros(self.point_count))) \
            .astype(samples.dtype)
        self.point_shader.set_buffer('color', colors.T)

        if self.warped_grid_box.checked():
            grid = np.linspace(0, 1, sqrt_val + 1)
            fine_grid = np.linspace(0, 1, 16 * sqrt_val)
            fine_grid = np.insert(fine_grid,
                                  np.arange(len(fine_grid)), fine_grid)[1:-1]
            fine_grid, grid = np.array(
                np.meshgrid(fine_grid, grid), dtype=float_dtype)

            lines = sample_func(
                np.column_stack([
                    np.hstack([grid.ravel(), fine_grid.ravel()]),
                    np.hstack([fine_grid.ravel(), grid.ravel()])
                ]))

            if type(lines) is tuple:
                lines = lines[0]

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

            self.grid_shader.set_buffer('position', lines)
            self.line_count = lines.shape[0]

        if 'InteractiveBSDF' in repr(distr[2][0]):
            def sph(theta, phi):
                return [np.cos(phi) * np.sin(theta), np.sin(phi) * np.sin(theta), np.cos(theta)]
            res = 50
            angles = np.linspace(0, 2*np.pi, res)
            angles = np.array(list(zip(angles[:-1], angles[1:]))).ravel()
            lines = np.array((np.cos(angles), np.sin(angles), np.zeros(angles.shape)))
            theta, phi = np.array(self.parameters[:2]) * np.pi / 180
            arrows = np.array([[0, 0, 0], sph(theta, phi),
                               [0, 0, 0], sph(theta, phi + np.pi)])
            lines = np.column_stack((lines, arrows.T))
            self.bsdf_shader.set_buffer('position', np.float32(lines.T))


    def run_test(self):
        distr = DISTRIBUTIONS[self.warp_type_box.selected_index()]
        domain = distr[1]
        sample, pdf = distr[2]
        settings = distr[3]

        self.test = ChiSquareTest(
            domain=domain,
            sample_func=lambda *args: sample(
                *(list(args) + self.parameters)),
            pdf_func=lambda *args: pdf(
                *(list(args) + self.parameters)),
            res=settings['res'],
            ires=settings['ires'],
            sample_dim=settings['sample_dim'],
        )
        self.test_result = self.test.run(0.01)
        self.window.set_visible(False)

        # Convert the histogram & integrated PDF to normalized textures
        pdf = self.test.pdf
        histogram = self.test.histogram
        min_value = np.amin([np.amin(pdf), np.amin(histogram)]) / 2
        max_value = np.amax(pdf) * 1.1
        pdf = (pdf - min_value) / (max_value - min_value)
        histogram = (histogram - min_value) / (max_value - min_value)
        self.pdf_texture = Texture(Bitmap(np.float32(pdf)),
            Texture.InterpolationMode.Nearest,
            Texture.InterpolationMode.Nearest)
        self.histogram_texture = Texture(Bitmap(np.float32(histogram)),
            Texture.InterpolationMode.Nearest,
            Texture.InterpolationMode.Nearest)


if __name__ == '__main__':
    nanogui.init()
    wv = WarpVisualizer()
    wv.set_visible(True)
    wv.perform_layout()
    nanogui.mainloop(refresh=-1)
    del wv
    gc.collect()
    nanogui.shutdown()
