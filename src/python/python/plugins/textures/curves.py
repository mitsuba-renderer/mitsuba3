from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi
import numpy as np

def eval_curve(points, value):
    p0 = mi.Vector2f(0)
    p1 = mi.Vector2f(1)

    for i in range(len(points)-1):
        found = (points[i][0] < value) & (points[i+1][0] >= value)
        p0[found] = mi.Vector2f(points[i][:2])
        p1[found] = mi.Vector2f(points[i+1][:2])

    t = (value - p0.x) / dr.maximum(p1.x - p0.x, 1e-6)

    res = dr.lerp(p0.y, p1.y, t)

    res[value < points[0][0]]  = 0.0
    res[value > points[-1][0]] = 1.0

    return res

class FloatCurve(mi.Texture):
    '''
    Float curve Blender shader node texture.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.factor = props.get_texture('factor', 1.0)
        self.value  = props.get_texture('value')
        self.points = np.array(props.get('points')).tolist()

        # TODO handle bezier handles (e.g. points[i][2] == 1)

    def traverse(self, cb):
        cb.put('factor', self.factor, +mi.ParamFlags.Differentiable)
        cb.put('value',  self.value,  +mi.ParamFlags.Differentiable)

    def eval(self, si, active):
        return mi.UnpolarizedSpectrum(self.process(si, active))

    def eval_1(self, si, active):
        return mi.Float(self.process(si, active))

    def eval_3(self, si, active):
        return mi.Color3f(self.process(si, active))

    def process(self, si, active):
        factor = self.factor.eval_1(si, active)
        value  = self.value.eval_1(si, active)
        return dr.lerp(value, eval_curve(self.points, value), factor)

    def mean(self):
        return self.value.mean() # TODO best effort

    def resolution(self):
        return self.value.resolution()

    def is_spatially_varying(self):
        return any([t.is_spatially_varying() for t in [self.factor, self.value]])

    def to_string(self):
        return f'FloatCurve[factor={self.factor}, value={self.value}]'

mi.register_texture('float_curve', lambda props: FloatCurve(props))


class RGBCurve(mi.Texture):
    '''
    RGB curve Blender shader node texture.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.factor = props.get_texture('factor', 1.0)
        self.color  = props.get_texture('color')
        self.points_c = np.array(props.get('points_c')).tolist()
        self.points_r = np.array(props.get('points_r')).tolist()
        self.points_g = np.array(props.get('points_g')).tolist()
        self.points_b = np.array(props.get('points_b')).tolist()

    def traverse(self, cb):
        cb.put('factor', self.factor, +mi.ParamFlags.Differentiable)
        cb.put('color',  self.color,  +mi.ParamFlags.Differentiable)

    def eval(self, si, active):
        return mi.UnpolarizedSpectrum(self.process(self.color.eval(si, active), si, active))

    def eval_1(self, si, active):
        return mi.luminance(self.process(mi.Color3f(self.color.eval_1(si, active)), si, active))

    def eval_3(self, si, active):
        return mi.Color3f(self.process(mi.Color3f(self.color.eval_3(si, active)), si, active))

    def process(self, color, si, active):
        factor = self.factor.eval_1(si, active)

        color2 = mi.Color3f(
            eval_curve(self.points_r, eval_curve(self.points_c, color.x)),
            eval_curve(self.points_g, eval_curve(self.points_c, color.y)),
            eval_curve(self.points_b, eval_curve(self.points_c, color.z)),
        )

        return dr.lerp(color, color2, factor)

    def mean(self):
        return self.color.mean() # TODO best effort

    def resolution(self):
        return self.color.resolution()

    def is_spatially_varying(self):
        return any([t.is_spatially_varying() for t in [self.factor, self.color]])

    def to_string(self):
        return f'RGBCurve[factor={self.factor}, color={self.color}]'

mi.register_texture('rgb_curve', lambda props: RGBCurve(props))
