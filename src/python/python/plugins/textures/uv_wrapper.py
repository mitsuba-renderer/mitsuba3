from __future__ import annotations # Delayed parsing of type annotations
from typing import Tuple

import drjit as dr
import mitsuba as mi

class UVWrapper(mi.Texture):
    '''
    Wrapper texture to evaluate input texture with a different set of UVs.

    This plugin in used in the Blender-Mitsuba add-on.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.uv = props.get('uv')
        self.input = props.get('input')

    def eval_si(self, si, active):
        si = mi.SurfaceInteraction3f(si)
        si.uv = self.uv.eval_3(si, active).xy
        si.uv = mi.Vector2f(si.uv.x, 1.0-si.uv.y) # Blender convention
        return si

    def eval(self, si, active):
        return self.input.eval(self.eval_si(si, active), active)

    def eval_1(self, si, active):
        return self.input.eval_1(self.eval_si(si, active), active)

    def eval_3(self, si, active):
        return self.input.eval_3(self.eval_si(si, active), active)

    def mean(self):
        return self.input.mean()

    def to_string(self):
        return f'UVWrapper[uv={self.uv}, input={self.input}]'

mi.register_texture('uv_wrapper', lambda props: UVWrapper(props))
