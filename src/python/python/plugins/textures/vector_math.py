from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

def fract(x):
    return x - dr.floor(x)

def safe_divide(a, b):
    return dr.select(b != 0.0, a / b, 0.0)

def wrap(a, b, c):
    r = b - c
    s = dr.select(a != b, dr.floor((a - c) / r), 1.0)
    return dr.select(r != 0.0, a - r * s, c)

def refract(incident, normal, eta):
    normal = dr.normalize(normal)
    dot_ni = dr.dot(normal, incident)
    k = 1.0 - eta * eta * (1.0 - dot_ni * dot_ni)
    return dr.select(k < 0.0, 0.0, eta * incident - (eta * dot_ni + dr.sqrt(k)) * normal)

# Supported math operators
# See blender implementation: https://github.com/blender/blender/blob/594f47ecd2d5367ca936cf6fc6ec8168c2b360d0/source/blender/gpu/shaders/material/gpu_shader_material_math.glsl#L203
OPERATORS = {
    'ADD':            (lambda a, b, c: a + b),
    'SUBSTRACT':      (lambda a, b, c: a - b),
    'MULTIPLY':       (lambda a, b, c: a * b),
    'DIVIDE':         (lambda a, b, c: a / b),
    'MULTIPLY_ADD':   (lambda a, b, c: a * b + c),

    'CROSS_PRODUCT':  (lambda a, b, c: dr.cross(b, a)),
    'PROJECT':        (lambda a, b, c: b * (dr.dot(a, b) / dr.dot(b, b))),
    'REFLECT':        (lambda a, b, c: mi.reflect(a, dr.normalize(b))),
    'REFRACT':        (lambda a, b, c: refract(a, b, c.x)),
    'FACEFORWARD':    (lambda a, b, c: dr.select(dr.dot(b, c) < 0.0, a, -a)),
    'DOT_PRODUCT':    (lambda a, b, c: dr.dot(a, b)),
    'DISTANCE':       (lambda a, b, c: dr.norm(a - b)),
    'LENGTH':         (lambda a, b, c: dr.norm(a)),
    'SCALE':          (lambda a, b, c: a * b),
    'NORMALIZE':      (lambda a, b, c: dr.normalize(a)),

    'WRAP':           (lambda a, b, c: wrap(a, b, c)),
    'SNAP':           (lambda a, b, c: dr.floor(safe_divide(a, b)) * b),
    'FLOOR':          (lambda a, b, c: dr.floor(a)),
    'CEIL':           (lambda a, b, c: dr.ceil(a)),
    'ABSOLUTE':       (lambda a, b, c: dr.abs(a)),
    'FRACTION':       (lambda a, b, c: fract(a)),
    'MODULO':         (lambda a, b, c: dr.select(b != 0.0, a - dr.trunc(a / b) * b, 0.0)),
    'MINIMUM':        (lambda a, b, c: dr.minimum(a, b)),
    'MAXIMUM':        (lambda a, b, c: dr.maximum(a, b)),
    'SINE':           (lambda a, b, c: dr.sin(a)),
    'COSINE':         (lambda a, b, c: dr.cos(a)),
    'TANGENT':        (lambda a, b, c: dr.tan(a)),
}

class VectorMath(mi.Texture):
    '''
    Vector math Blender shader node texture.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.input_0 = props.get_texture('input_0')
        self.input_1 = props.get_texture('input_1', 0.0)
        self.input_2 = props.get_texture('input_2', 0.0)
        self.mode = str(props.get('mode'))

        if not self.mode in OPERATORS.keys():
            mi.Log(mi.LogLevel.Error, f'Unknown vector math operator: {self.mode}')

    def traverse(self, cb):
        cb.put('input_0', self.input_0, +mi.ParamFlags.Differentiable)
        cb.put('input_1', self.input_1, +mi.ParamFlags.Differentiable)
        cb.put('input_2', self.input_1, +mi.ParamFlags.Differentiable)

    def eval(self, si, active):
        return mi.UnpolarizedSpectrum(self.eval_3(si, active))

    def eval_1(self, si, active):
        return mi.Float(self.eval_3(si, active).x)

    def eval_3(self, si, active):
        return mi.Color3f(self.process(
            self.input_0.eval_3(si, active),
            self.input_1.eval_3(si, active),
            self.input_2.eval_3(si, active)
        ))

    def process(self, a, b, c):
        return OPERATORS[self.mode](a, b, c)

    def mean(self):
        return self.input_0.mean() # TODO best effort

    def resolution(self):
        return self.input_0.resolution()

    def is_spatially_varying(self):
        return any([t.is_spatially_varying() for t in [self.input_0, self.input_1, self.input_2]])

    def to_string(self):
        return f'VectorMath[input_0={self.input_0}, input_1={self.input_1}, input_2={self.input_2}, mode={self.mode}]'

mi.register_texture('vector_math', lambda props: VectorMath(props))
