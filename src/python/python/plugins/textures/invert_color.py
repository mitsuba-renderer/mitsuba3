from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

class InvertColor(mi.Texture):
    '''
    Invert Color Blender shader node texture.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.factor = props.get_texture('factor', 1.0)
        self.color  = props.get_texture('color')

    def traverse(self, cb):
        cb.put('factor', self.factor, +mi.ParamFlags.Differentiable)
        cb.put('color',  self.color,  +mi.ParamFlags.Differentiable)

    def eval(self, si, active):
        return mi.UnpolarizedSpectrum(self.process(self.color.eval(si, active), si, active))

    def eval_1(self, si, active):
        return mi.Float(self.process(self.color.eval_1(si, active), si, active))

    def eval_3(self, si, active):
        return mi.Color3f(self.process(self.color.eval_3(si, active), si, active))

    def process(self, color, si, active):
        factor = self.factor.eval_1(si, active)
        return color * (1.0 - factor) + factor * (1.0 - color)

    def mean(self):
        return self.color.mean() # TODO best effort

    def resolution(self):
        return self.color.resolution()

    def is_spatially_varying(self):
        return any([t.is_spatially_varying() for t in [self.color, self.factor]])

    def to_string(self):
        return f'InvertColor[factor={self.factor}, color={self.color}]'

mi.register_texture('invert_color', lambda props: InvertColor(props))
