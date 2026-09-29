from __future__ import annotations # Delayed parsing of type annotations
from typing import Tuple

import drjit as dr
import mitsuba as mi

class SeparateRGB(mi.Texture):
    '''
    Separate RGB Blender shader node texture.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.texture = props.get('input')
        self.channel = props.get('channel', 'r')
        if self.channel not in ['r', 'g', 'b']:
            raise ValueError(f"SeparateRGB: Invalid channel {self.channel}")

    def traverse(self, cb):
        cb.put('input', self.texture, mi.ParamFlags.Differentiable)

    def parameters_changed(self, keys):
        self.texture.parameters_changed(keys)

    def _eval_channel(self, si, active):
        color = self.texture.eval_3(si, active)
        return color[{ 'r': 0, 'g': 1, 'b': 2 }[self.channel]]

    def eval(self, si, active):
        return mi.UnpolarizedSpectrum(self._eval_channel(si, active))

    def eval_1(self, si, active):
        return mi.Float(self._eval_channel(si, active))

    def eval_3(self, si, active):
        return mi.Color3f(self._eval_channel(si, active))

    def mean(self):
        return self.texture.mean()

    def resolution(self):
        return self.texture.resolution()

    def is_spatially_varying(self):
        return self.texture.is_spatially_varying()

    def to_string(self):
        return f'Separate RGB[input={self.texture}, channel={self.channel}]'

mi.register_texture('separate_rgb', lambda props: SeparateRGB(props))
