from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

class Clamp(mi.Texture):
    '''
    Clamp Blender shader node texture.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.input = props.get_texture('input', 1.0)
        self.min = props.get_texture('min', 0.0)
        self.max = props.get_texture('max', 1.0)
        self.clamp_type = props.get('clamp_type', 'RANGE')

    def traverse(self, cb):
        cb.put('input', self.input, +mi.ParamFlags.Differentiable)
        cb.put('min', self.min, +mi.ParamFlags.Differentiable)
        cb.put('max', self.max, +mi.ParamFlags.Differentiable)

    def parameters_changed(self, keys):
        pass

    def eval(self, si, active):
        return mi.UnpolarizedSpectrum(self.process(self.input.eval(si, active), si, active))

    def eval_1(self, si, active):
        return mi.Float(self.process(self.input.eval_1(si, active), si, active))

    def eval_3(self, si, active):
        return mi.Color3f(self.process(self.input.eval_3(si, active), si, active))

    def process(self, value, si, active):
        min_value = self.min.eval_1(si, active)
        max_value = self.max.eval_1(si, active)

        if self.clamp_type == 'RANGE':
            min_value, max_value = dr.minimum(min_value, max_value), dr.maximum(min_value, max_value)
            return dr.clip(value, min_value, max_value)
        else:
            return dr.minimum(dr.maximum(value, min_value), max_value)

    def mean(self):
        return self.input.mean() # TODO best effort

    def resolution(self):
        return self.input.resolution()

    def is_spatially_varying(self):
        return any([t.is_spatially_varying() for t in [self.input, self.min, self.max]])

    def to_string(self):
        return f'Clamp[input={self.input}]'

mi.register_texture('clamp', lambda props: Clamp(props))
