# TODO: header / description

import nanogui
import mitsuba
import gc

from nanogui import Color, Screen, Window, GroupLayout, BoxLayout, \
                    ToolButton, Vector2i, Label, Button, Widget, \
                    PopupButton, CheckBox, MessageDialog, VScrollPanel, \
                    ImagePanel, ImageView, ComboBox, ProgressBar, Slider, \
                    TextBox, ColorWheel, Graph, VectorXf, GridLayout, \
                    Alignment, Orientation
from nanogui import glfw, entypo


class TestApp(Screen):
    def __init__(self):
        super(TestApp, self).__init__(Vector2i(800, 600), "Distribution warping visualizer")

    def initializeGUI(self):
        window = Window(self, "Warp tester")
        window.setPosition(Vector2i(15, 15))
        window.setLayout(GroupLayout())

        Label(window, "Distribution")

        self.performLayout()

    def draw(self, ctx):
        # self.progress.setValue(math.fmod(time.time() / 10, 1))
        super(TestApp, self).draw(ctx)

    def keyboardEvent(self, key, scancode, action, modifiers):
        if super(TestApp, self).keyboardEvent(key, scancode,
                                              action, modifiers):
            return True
        if key == glfw.KEY_ESCAPE and action == glfw.PRESS:
            self.setVisible(False)
            return True
        return False

if __name__ == "__main__":
    nanogui.init()
    test = TestApp()
    test.drawAll()
    test.setVisible(True)
    nanogui.mainloop()
    gc.collect()
    nanogui.shutdown()
