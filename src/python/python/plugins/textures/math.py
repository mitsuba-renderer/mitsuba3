from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

def smooth_min(a, b, c):
    h = dr.maximum(c - dr.abs(a - b), 0.0) / c
    out = dr.minimum(a, b) - h * h * h * c * (1.0 / 6.0)
    out[c == 0.0] = dr.minimum(a, b)
    return out

def fract(x):
    return x - dr.floor(x)

def safe_divide(a, b):
    return dr.select(b != 0.0, a / b, 0.0)

def wrap(a, b, c):
    r = b - c
    s = dr.select(a != b, dr.floor((a - c) / r), 1.0)
    return dr.select(r != 0.0, a - r * s, c)

# Supported math operators
# See blender implementation: https://github.com/blender/blender/blob/594f47ecd2d5367ca936cf6fc6ec8168c2b360d0/source/blender/gpu/shaders/material/gpu_shader_material_math.glsl#L203
OPERATORS = {
    'ADD':            (lambda a, b, c: a + b),
    'SUBTRACT':       (lambda a, b, c: a - b),
    'MULTIPLY':       (lambda a, b, c: a * b),
    'DIVIDE':         (lambda a, b, c: a / b),
    'MULTIPLY_ADD':   (lambda a, b, c: a * b + c),
    'POWER':          (lambda a, b, c: dr.power(a, b)),
    'LOGARITHM':      (lambda a, b, c: dr.log(a)/ dr.log(b)),
    'SQRT':           (lambda a, b, c: dr.sqrt(a)),
    'INVERSE_SQRT':   (lambda a, b, c: dr.rcp(dr.sqrt(a))),
    'ABSOLUTE':       (lambda a, b, c: dr.abs(a)),
    'EXPONENT':       (lambda a, b, c: dr.exp(a)),
    'MINIMUM':        (lambda a, b, c: dr.minimum(a, b)),
    'MAXIMUM':        (lambda a, b, c: dr.maximum(a, b)),
    'LESS_THAN':      (lambda a, b, c: dr.select(a < b, 1.0, 0.0)),
    'GREATER_THAN':   (lambda a, b, c: dr.select(a > b, 1.0, 0.0)),
    'SIGN':           (lambda a, b, c: dr.select(a >= 0.0, 1.0, 0.0)),
    'COMPARE':        (lambda a, b, c: dr.select(dr.abs(a - b) <= dr.maximum(c, 1e-5), 1.0, 0.0)),
    'SMOOTH_MIN':     (lambda a, b, c: smooth_min(a, b, c)),
    'SMOOTH_MAX':     (lambda a, b, c: -smooth_min(-a, -b, c)),
    'ROUND':          (lambda a, b, c: dr.round(a)),
    'FLOOR':          (lambda a, b, c: dr.floor(a)),
    'CEIL':           (lambda a, b, c: dr.ceil(a)),
    'TRUNC':          (lambda a, b, c: dr.trunc(a)),
    'FRACTION':       (lambda a, b, c: fract(a)),
    'MODULO':         (lambda a, b, c: dr.select(b != 0.0, a - dr.trunc(a / b) * b, 0.0)),
    'FLOORED_MODULO': (lambda a, b, c: dr.select(b != 0.0, a - dr.floor(a / b) * b, 0.0)),
    'WRAP':           (lambda a, b, c: wrap(a, b, c)),
    'SNAP':           (lambda a, b, c: dr.floor(safe_divide(a, b)) * b),
    'PINGPONG':       (lambda a, b, c: dr.select(b != 0.0, dr.abs(fract((a - b) / (b * 2.0)) * b * 2.0 - b), 0.0)),
    'SINE':           (lambda a, b, c: dr.sin(a)),
    'COSINE':         (lambda a, b, c: dr.cos(a)),
    'TANGENT':        (lambda a, b, c: dr.tan(a)),
    'ARCSINE':        (lambda a, b, c: dr.select(a <= 1.0 & a >= -1.0, dr.asin(a), 0.0)),
    'ARCCOSINE':      (lambda a, b, c: dr.select(a <= 1.0 & a >= -1.0, dr.acos(a), 0.0)),
    'ARCTANGENT':     (lambda a, b, c: dr.atan(a)),
    'ARCTAN2':        (lambda a, b, c: dr.atan2(a, b)),
    'SINH':           (lambda a, b, c: dr.sinh(a)),
    'COSH':           (lambda a, b, c: dr.cosh(a)),
    'TANH':           (lambda a, b, c: dr.tanh(a)),
    'RADIANS':        (lambda a, b, c: dr.deg2rad(a)),
    'DEGREES':        (lambda a, b, c: dr.rad2deg(a))
}

class Math(mi.Texture):
    '''
    Math Blender shader node texture.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.input_0 = props.get_texture('input_0')
        self.input_1 = props.get_texture('input_1', 0.0)
        self.input_2 = props.get_texture('input_2', 0.0)
        self.mode = str(props.get('mode'))
        self.clamp = props.get('clamp', False)

        if not self.mode in OPERATORS.keys():
            mi.Log(mi.LogLevel.Error, f'Unknown math operator: {self.mode}')

    def traverse(self, cb):
        cb.put('input_0', self.input_0, +mi.ParamFlags.Differentiable)
        cb.put('input_1', self.input_1, +mi.ParamFlags.Differentiable)
        cb.put('input_2', self.input_1, +mi.ParamFlags.Differentiable)

    def eval(self, si, active):
        return mi.UnpolarizedSpectrum(self.process(
            self.input_0.eval(si, active),
            self.input_1.eval(si, active),
            self.input_2.eval(si, active)
        ))

    def eval_1(self, si, active):
        return mi.Float(self.process(
            self.input_0.eval_1(si, active),
            self.input_1.eval_1(si, active),
            self.input_2.eval_1(si, active)
        ))

    def eval_3(self, si, active):
        return mi.Color3f(self.process(
            self.input_0.eval_3(si, active),
            self.input_1.eval_3(si, active),
            self.input_2.eval_3(si, active)
        ))

    def process(self, a, b, c):
        out = OPERATORS[self.mode](a, b, c)

        if self.clamp:
            out = dr.clip(out, 0.0, 1.0)

        return out

    def mean(self):
        return self.input_0.mean() # TODO best effort

    def resolution(self):
        return self.input_0.resolution()

    def is_spatially_varying(self):
        return any([t.is_spatially_varying() for t in [self.input_0, self.input_1, self.input_2]])

    def to_string(self):
        return f'Math[input_0={self.input_0}, input_1={self.input_1}, input_2={self.input_2}, mode={self.mode}, clamp={self.clamp}]'

mi.register_texture('math', lambda props: Math(props))
