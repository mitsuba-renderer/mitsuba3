from __future__ import annotations # Delayed parsing of type annotations
from typing import Tuple

import drjit as dr
import mitsuba as mi

class TextureCoordinate(mi.Texture):
    '''
    Texture coordinates Blender shader node texture.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.channel = props.get('channel', 'UV')
        if self.channel not in ['UV']:
            raise ValueError(f"TextureCoordinate: Invalid channel {self.channel}")

    def _eval_channel(self, si, active):
        return mi.Vector3f(si.uv.x, 1.0-si.uv.y, 0.0) # Follow Blender's convention

    def eval(self, si, active):
        raise ValueError(f"TextureCoordinate: eval not supported!")

    def eval_1(self, si, active):
        raise ValueError(f"TextureCoordinate: eval_1 not supported!")

    def eval_3(self, si, active):
        return mi.Color3f(self._eval_channel(si, active))

    def mean(self):
        return mi.Float(0.5)

    def to_string(self):
        return f'Texture Coordinate[{self.channel}]'

mi.register_texture('texture_coordinate', lambda props: TextureCoordinate(props))
