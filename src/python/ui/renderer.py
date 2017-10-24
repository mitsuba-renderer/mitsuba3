from __future__ import unicode_literals, print_function

import gc
import mitsuba
import nanogui
import numpy as np
import os
import sys

from mitsuba.core import Bitmap, Struct
from mitsuba.core import Thread, FileResolver
from mitsuba.core import filesystem as fs
from mitsuba.ui import GLTexture

from nanogui import (Color, Screen, Window, Widget, GroupLayout, BoxLayout,
                     Label, Button, TextBox, CheckBox, ComboBox, Slider,
                     Alignment, Orientation, ImageView)

from nanogui import glfw, entypo

class MitsubaRenderer(Screen):
    '''
    Graphical user interface for the Mitsuba renderer.
    '''

    def __init__(self):
        super(MitsubaRenderer, self).__init__(
            [800, 600], "Mitsuba Renderer (v%s)" % mitsuba.MTS_VERSION)
        self.sidebar_width = 200

        self.render_vectorized = True
        self.scene_filename = None
        self.scene = None
        self.log_lines = []
        self.init_layout()

    def init_layout(self):
        def add_spacer(h, parent = self):
            # TODO: better way to create margins?
            spacer = Label(parent, '')
            spacer.set_fixed_height(h)

        self.set_background(Color(0.3, 0.3, 0.3, 0))

        self.set_layout(BoxLayout(Orientation.Horizontal, Alignment.Fill))

        # ----- Main toolbar
        sidebar = Window(self, 'Mitsuba Renderer')
        sidebar.set_layout(BoxLayout(Orientation.Vertical, Alignment.Middle))
        sidebar.set_fixed_width(self.sidebar_width)
        inner_width = self.sidebar_width - 20

        # File loading button
        # TODO: "recent files" feature
        add_spacer(20, sidebar)
        sl = Label(sidebar, 'Scene selection', 'sans-bold')
        sl.set_fixed_width(inner_width)
        add_spacer(10, sidebar)
        scene_button = Button(sidebar, 'Open scene', entypo.ICON_FOLDER)
        scene_button.set_callback(lambda: self.select_scene())
        scene_button.set_fixed_width(inner_width)

        add_spacer(20, sidebar)
        self.scene_name = Label(sidebar, 'No scene loaded.', 'sans')
        self.scene_name.set_fixed_width(inner_width)
        add_spacer(20, sidebar)

        # Vectorized render checkbox
        add_spacer(20, sidebar)
        def update_vector_checkbox(val):
            self.render_vectorized = val
            print(self.render_vectorized)
        self.vectorized_checkbox = CheckBox(sidebar, 'Use SIMD instructions',
                                            update_vector_checkbox)
        self.vectorized_checkbox.set_checked(self.render_vectorized)
        add_spacer(10, sidebar)

        # Render button
        render_button = Button(sidebar, 'Render', entypo.ICON_ROCKET)
        render_button.set_background_color(Color(0, 1., 0., 0.10))
        render_button.set_fixed_width(inner_width)
        render_button.set_callback(lambda: self.run_render())

        # ----- Result view
        self.result_window = Window(self, '')
        self.result_window.set_layout(
            BoxLayout(Orientation.Vertical, Alignment.Fill))
        self.result_window.set_fixed_width(self.width() - self.sidebar_width)
        self.update_image_viewer(None)
        self.result_window.center()
        self.result_window.set_visible(True)

        # ----- Log view
        self.log_window = Window(self, 'Logs')
        self.log_window.set_layout(GroupLayout())
        self.log_window.set_visible(False)  # True

        # TODO: change to a proper textview (auto scroll, etc)
        self.log_widget = Label(self.log_window, '', 'sans')
        self.log_widget.set_fixed_size([self.size()[0] - 60, 150])
        self.log_window.set_position([
            15, self.size()[1] - 210
        ])

        self.update_logs()

        self.perform_layout()
        self.resize_event(self.size())

    def keyboard_event(self, key, scancode, action, modifiers):
        if super(MitsubaRenderer, self).keyboard_event(key, scancode, action,
                                                      modifiers):
            return True
        if key == glfw.KEY_ESCAPE and action == glfw.PRESS:
            # TODO: ask for confirmation if something important is unsaved.
            self.set_visible(False)
            return True
        return False

    def resize_event(self, size):
        super(MitsubaRenderer, self).resize_event(size)
        self.result_window.set_fixed_width(size[0] - self.sidebar_width)
        self.result_view.set_fixed_height(self.result_window.height())
        self.perform_layout()

    def show_error(self, message):
        # TODO: better error UI
        self.log_lines.append("Error: " + str(message))
        print(self.log_lines[-1], file=sys.stderr)
        self.update_logs()

    def update_logs(self):
        self.log_widget.set_caption('\n'.join(self.log_lines))

    def update_image_viewer(self, bitmap):
        if bitmap is None:
            # Fill the result image placeholder with neutral gray
            b = Bitmap(Bitmap.ERGB, Struct.EUInt8, [250, 250])
            np.array(b, copy=False)[:] = 127

            self.result_image = GLTexture()
            self.result_image.init(b)
            self.result_view = ImageView(
                self.result_window, self.result_image.id())
        else:
            if bitmap.pixel_format() != Bitmap.ERGB:
                bitmap = bitmap.convert(Bitmap.ERGB, bitmap.component_format(),
                                        bitmap.srgb_gamma())

            # TODO: why is result_image.refresh(bitmap) not sufficient?
            # self.result_image.refresh(bitmap)
            self.result_image = GLTexture()
            self.result_image.init(bitmap)

            self.result_view.bind_image(self.result_image.id())

    def select_scene(self):
        filename = nanogui.file_dialog([("xml", "Mitsuba scene file")], False)
        if not filename:
            self.show_error("No scene file selected.")
            return True

        # Update the file resolver to include the scene's directory
        # TODO: double-check that the file exists and is valid?
        thread = Thread.thread()
        fres = thread.file_resolver()
        fres.append(os.path.dirname(filename))

        # Try and parse the scene
        scene = mitsuba.core.xml.load_file(filename)
        if scene is None:
            self.scene_name.set_caption("Error loading scene.")
            return True

        self.scene_filename = filename
        self.scene_name.set_caption("Scene loaded. " + str(filename))
        print("Scene loaded: " + str(filename))
        self.set_scene(scene)
        return True

    def set_scene(self, scene):
        """Takes a Mitsuba Scene object (after being parsed)."""
        self.scene = scene
        self.integrator = scene.integrator()
        if (self.integrator is None):
            self.show_error("At least one integrator must be specified.")
        self.film = scene.film()
        if (self.film is None):
            self.show_error("At least one film must be specified.")

    def run_render(self):
        if not self.scene:
            self.show_error("Please select a scene first.")
            return
        # TODO: auto-reload scene to allow for quick iterations

        if self.render_vectorized:
            print("Rendering now (vector)")
            res = self.integrator.render_vector(self.scene)
        else:
            print("Rendering now (scalar)")
            res = self.integrator.render_scalar(self.scene)
        print("Done rendering")
        if not res:
            self.show_error("Render failed (please see logs).")
            return

        self.develop(self.film)
        return True

    def develop(self, film):
        destination = fs.path(self.scene_filename).replace_extension(".exr")
        print("Developping to: " + str(destination))
        # Develop the film (i.e. save the result in an image file).
        film.set_destination_file(destination, 32)
        film.develop(self.scene, 1.0)

        # Display image
        self.update_image_viewer(film.bitmap())


if __name__ == '__main__':
    nanogui.init()
    gui = MitsubaRenderer()
    gui.set_visible(True)
    gui.perform_layout()
    nanogui.mainloop(refresh=0)
    del gui
    gc.collect()
    nanogui.shutdown()
