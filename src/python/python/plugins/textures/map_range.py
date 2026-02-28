from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

class MapRange(mi.Texture):
    '''
    Map Range Blender shader node texture.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.clamp = props.get('clamp', True)
        self.vector = props.get('vector', True) # Whether to interpret the inputs as vectors or floats
        self.steps    = props.get_texture('steps', 4.0)
        self.input    = props.get_texture('input', 1.0)
        self.from_min = props.get_texture('from_min', 0.0)
        self.from_max = props.get_texture('from_max', 1.0)
        self.to_min   = props.get_texture('to_min', 0.0)
        self.to_max   = props.get_texture('to_max', 1.0)
        self.interpolation_type = props.get('interpolation_type', 'LINEAR')

    def traverse(self, cb):
        cb.put('input',    self.input,    +mi.ParamFlags.Differentiable)
        cb.put('from_min', self.from_min, +mi.ParamFlags.Differentiable)
        cb.put('from_max', self.from_max, +mi.ParamFlags.Differentiable)
        cb.put('to_min',   self.to_min,   +mi.ParamFlags.Differentiable)
        cb.put('to_max',   self.to_max,   +mi.ParamFlags.Differentiable)

    def parameters_changed(self, keys):
        pass

    def eval(self, si, active):
        return mi.UnpolarizedSpectrum(self.eval_3(si, active))

    def eval_1(self, si, active):
        return mi.Float(self.process(si, active))

    def eval_3(self, si, active):
        return mi.Color3f(self.process(si, active))

    def process(self, si, active):
        if self.vector:
            v        = self.input.eval_3(si, active)
            from_min = self.from_min.eval_3(si, active)
            from_max = self.from_max.eval_3(si, active)
            to_min   = self.to_min.eval_3(si, active)
            to_max   = self.to_max.eval_3(si, active)
            steps    = self.steps.eval_3(si, active)
        else:
            v        = self.input.eval_1(si, active)
            from_min = self.from_min.eval_1(si, active)
            from_max = self.from_max.eval_1(si, active)
            to_min   = self.to_min.eval_1(si, active)
            to_max   = self.to_max.eval_1(si, active)
            steps    = self.steps.eval_1(si, active)

        if self.interpolation_type == 'LINEAR':
            v = (dr.clip(v, from_min, from_max) - from_min) / (from_max - from_min) * (to_max - to_min) + to_min
        elif self.interpolation_type == 'STEPPED':
            steps = self.steps.eval_1(si, active)
            v = (dr.clip(v, from_min, from_max) - from_min) / (from_max - from_min)
            v = dr.floor(v * (steps + 1)) / (steps + 1)
            v = v * (to_max - to_min) + to_min
        else:
            v = (dr.clip(v, from_min, from_max) - from_min) / (from_max - from_min)
            if self.interpolation_type == 'SMOOTHSTEP':
                v = v * v * (3.0 - 2.0 * v)
            else:
                v = v * v * v * (v * (6.0 * v - 15.0) + 10.0)
            v = v * (to_max - to_min) + to_min

        if self.clamp:
            v = dr.clip(v, dr.minimum(to_min, to_max), dr.maximum(to_min, to_max))

        return v

    def mean(self):
        return self.input.mean() # TODO best effort

    def resolution(self):
        return self.input.resolution()

    def is_spatially_varying(self):
        return any([t.is_spatially_varying() for t in [
            self.from_min, self.from_max, self.to_min, self.to_max, self.input
        ]])

    def to_string(self):
        return f'MapRange[input={self.input}]'

mi.register_texture('map_range', lambda props: MapRange(props))
