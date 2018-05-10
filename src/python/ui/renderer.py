from __future__ import unicode_literals, print_function

import argparse
import gc
import mitsuba
import nanogui
import numpy as np
import os
import sys

from mitsuba.core import Bitmap, Struct
from mitsuba.core import filesystem as fs
from mitsuba.core import Thread, FileResolver
from mitsuba.ui import GLTexture

from nanogui import (Color, Screen, Window, Widget, GroupLayout, BoxLayout,
                     Label, Button, TextBox, CheckBox, ComboBox, Slider,
                     Alignment, Orientation, ImageView)
from nanogui import glfw, entypo

from renderer_thread import RendererThread

class CustomViewer(ImageView):
    """
    Simple variation on NanoGUI's ImageView that can refresh when requested.
    By default, a gray square image is shown.
    """
    def __init__(self, parent, bitmap_picker):
        self.picker = bitmap_picker
        def cb(idx):
            self.select_bitmap(self.picker.items()[idx])
        self.picker.set_callback(cb)

        self.texture = GLTexture()
        self.bitmaps = {}
        self.selected_bitmap = None
        self.needs_update = True
        self.needs_bind = True

        # Neutral gray bitmap
        b = Bitmap(Bitmap.ERGB, Struct.EFloat32, [250, 250])
        b.set_srgb_gamma(True)
        np.array(b, copy=False)[:] = 0.5
        self.add_bitmap('placeholder', b)

        super(CustomViewer, self).__init__(parent, self.texture.id())

    def add_bitmap(self, index, bitmap):
        index = str(index)
        self.bitmaps[index] = bitmap
        current_items = self.picker.items()
        if index not in current_items:
            self.picker.set_items(current_items + [str(index)])
            # TODO: refresh layout after each item is added?
            # if self.parent().parent().layout_done:
            #     self.parent().parent().perform_layout()

        if self.selected_bitmap is None:
            self.select_bitmap(index)

    def has_bitmap(self, index, size = None):
        index = str(index)
        if not index in self.bitmaps:
            return False
        if size is not None:
            return np.all(self.bitmaps[index].size() == size)
        return True

    def select_bitmap(self, index):
        index = str(index)
        assert index in self.bitmaps
        self.needs_bind = (self.selected_bitmap is None) or \
                          np.any(self.bitmaps[index].size()
                                 != self.bitmaps[self.selected_bitmap].size())
        self.selected_bitmap = index
        self.needs_update = True

    def is_selected(self, index):
        return self.selected_bitmap == index

    def request_update(self):
        self.needs_update = True

    def draw(self, *args, **kwargs):
        if self.needs_update:
            self.texture.refresh(self.bitmaps[self.selected_bitmap])
            vs = self.scaled_image_size()
            if self.needs_bind:
                self.bind_image(self.texture.id())
                self.needs_bind = False
            self.needs_update = False

        super(CustomViewer, self).draw(*args, **kwargs)



class MitsubaRenderer(Screen):
    """
    Graphical user interface for the Mitsuba renderer.
    """

    def __init__(self):
        super(MitsubaRenderer, self).__init__(
            [800, 600], "Mitsuba Renderer (v%s)" % mitsuba.MTS_VERSION)
        self.sidebar_width = 200

        self.render_vectorized = True
        self.scene_filename = None
        self.scene = None
        self.worker_thread = None
        self.log_lines = []
        self.layout_done = False
        self.init_layout()


    def init_layout(self):
        def add_spacer(h, parent = self):
            # TODO: better way to create margins?
            spacer = Label(parent, '')
            spacer.set_fixed_height(h)

        self.set_background(Color(0.3, 0.3, 0.3, 0))
        self.set_layout(BoxLayout(Orientation.Horizontal, Alignment.Fill))
        # This is needed to allow for popup buttons (e.g. ComboBox)
        window = Window(self, '')
        window.set_layout(BoxLayout(Orientation.Horizontal, Alignment.Fill))


        # ----- Main toolbar
        sidebar = Widget(window)
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
        scene_button.set_callback(self.select_scene)
        scene_button.set_fixed_width(inner_width)

        add_spacer(20, sidebar)
        self.scene_name = Label(sidebar, 'No scene loaded.', 'sans')
        self.scene_name.set_fixed_width(inner_width)
        add_spacer(20, sidebar)

        # Vectorized render checkbox
        add_spacer(20, sidebar)
        def update_vector_checkbox(val):
            self.render_vectorized = val
        self.vectorized_checkbox = CheckBox(sidebar, 'Use SIMD instructions',
                                            update_vector_checkbox)
        self.vectorized_checkbox.set_checked(self.render_vectorized)
        add_spacer(10, sidebar)

        # Render button
        render_button = Button(sidebar, 'Render', entypo.ICON_ROCKET)
        render_button.set_background_color(Color(0, 1., 0., 0.10))
        render_button.set_fixed_width(inner_width)
        render_button.set_callback(lambda: self.run_render())
        add_spacer(20, sidebar)

        # Combobox (in the sidebar) to pick which bitmap to preview
        self.preview_combobox = ComboBox(sidebar)

        # ----- Result view
        self.result_window = Widget(window)
        self.result_window.set_layout(
            BoxLayout(Orientation.Vertical, Alignment.Fill))
        self.result_window.set_fixed_width(self.width() - self.sidebar_width)

        self.result_view = CustomViewer(self.result_window, self.preview_combobox)
        # self.result_window.center()
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
        self.layout_done = True


    def keyboard_event(self, key, scancode, action, modifiers):
        if super(MitsubaRenderer, self).keyboard_event(key, scancode, action,
                                                      modifiers):
            return True
        if key == glfw.KEY_ESCAPE and action == glfw.PRESS:
            # TODO: ask for confirmation if something important is unsaved.
            # TODO: properly cancel any ongoing render.
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


    def select_scene(self, filename = None):
        if not filename:
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
        if self.worker_thread is not None:
            self.show_error("A render is already in progress.")

        # TODO: auto-reload scene to allow for quick iterations

        # Prepare the preview bitmap
        if not self.result_view.has_bitmap('final', size=self.film.size()):
            b = Bitmap(Bitmap.ERGB, Struct.EFloat32, size=self.film.size())
            b.set_srgb_gamma(True)
            b.clear()
            self.result_view.add_bitmap('final', b)
            self.result_view.select_bitmap('final')

        def cb(res):
            if not res:
                self.show_error("Render failed (please see logs).")
                return False

            # Display image (after conversion to RGB)
            self.film.bitmap().convert(self.result_view.bitmaps['final'])
            self.result_view.select_bitmap('final')
            # Also save image file
            self.develop(self.film)

            self.worker_thread = None
            self.redraw()

        def progress(thread_id, bitmap, extra):
            key = 'wrk {}'.format(thread_id)
            if not self.result_view.has_bitmap(key, size=bitmap.size()):
                b = Bitmap(Bitmap.ERGB, Struct.EFloat32, size=bitmap.size())
                b.set_srgb_gamma(True)
                self.result_view.add_bitmap(key, b)

            # Automatically switch to an in-progress view when render starts
            if (self.result_view.selected_bitmap == -1
                or self.result_view.selected_bitmap == 'final'):
                self.result_view.select_bitmap(key)

            if self.result_view.is_selected(key):
                bitmap.convert(self.result_view.bitmaps[key])
                self.result_view.request_update()
                self.redraw()

        def fail(error):
            self.redraw()
            self.show_error(str(error))
            self.worker_thread = None


        self.worker_thread = RendererThread(
            integrator=self.integrator, args=[self.scene],
            kwargs={'vectorize': self.render_vectorized},
            progress_callback=progress, done_callback=cb, error_callback=fail
        )
        self.worker_thread.start()

        print("Started render ({} mode).".format("vector" if self.render_vectorized else "scalar"))
        return True


    def develop(self, film):
        destination = fs.path(self.scene_filename).replace_extension(".exr")
        # Develop the film (i.e. save the result in an image file).
        film.set_destination_file(destination, 32)
        film.develop()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(prog='mtsgui')
    parser.add_argument('--scalar', action='store_true', default=False, help='Render in scalar mode (default: False).')
    parser.add_argument('scene', nargs='?', default=None, help='Path to a Mitsuba scene file (.xml).')
    args = parser.parse_args()

    nanogui.init()
    gui = MitsubaRenderer()
    gui.set_visible(True)
    gui.perform_layout()
    gui.render_vectorized = not args.scalar

    if args.scene:
        gui.select_scene(filename=args.scene)
        gui.run_render()

    gui.redraw()
    nanogui.mainloop(refresh=0)
    del gui
    gc.collect()
    nanogui.shutdown()
