from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

class BrightnessContrast(mi.Texture):
    '''
    Brightness Contrast Blender shader node texture.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.color      = props.get_texture('color')
        self.brightness = props.get_texture('brightness', 0.0)
        self.contrast   = props.get_texture('contrast', 0.0)

    def traverse(self, cb):
        cb.put('color',      self.color,      +mi.ParamFlags.Differentiable)
        cb.put('brightness', self.brightness, +mi.ParamFlags.Differentiable)
        cb.put('contrast',   self.contrast,   +mi.ParamFlags.Differentiable)

    def eval(self, si, active):
        return mi.UnpolarizedSpectrum(self.process(self.color.eval(si, active), si, active))

    def eval_1(self, si, active):
        return mi.Float(self.process(self.color.eval_1(si, active), si, active))

    def eval_3(self, si, active):
        return mi.Color3f(self.process(self.color.eval_3(si, active), si, active))

    def process(self, color, si, active):
        brightness = self.brightness.eval_1(si, active)
        contrast   = self.contrast.eval_1(si, active)
        return dr.maximum((1.0 + contrast) * color + (brightness - contrast * 0.5), 0.0)

    def mean(self):
        return self.color.mean() # TODO best effort

    def resolution(self):
        return mi.ScalarVector2i(1, 1)

    def is_spatially_varying(self):
        return any([t.is_spatially_varying() for t in [self.color, self.brightness, self.contrast]])

    def to_string(self):
        return f'BrightnessContrast[color={self.color}, brightness={self.brightness}, contrast={self.contrast}]'

mi.register_texture('brightness_contrast', lambda props: BrightnessContrast(props))
