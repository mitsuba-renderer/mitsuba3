from __future__ import annotations # Delayed parsing of type annotations
from typing import Tuple

import drjit as dr
import mitsuba as mi

class NormalMap(mi.Texture):
    '''
    Normal map Blender shader node texture.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.color    = props.get_texture('color')
        self.strength = props.get_texture('strength')
        self.space    = props.get('space')
        if not self.space in ['TANGENT', 'OBJECT', 'WORLD']:
            raise Exception(f'NormalMap: Invalid space {self.space}!')

    def eval(self, si, active):
        return self.eval_3(si, active)

    def eval_1(self, si, active):
        raise ValueError(f"NormalMap: eval_1 not supported!")

    def eval_3(self, si, active):
        normal = mi.Normal3f(self.color.eval_3(si, active))
        strength = self.strength.eval_1(si, active)

        if self.space == 'TANGENT':
            pass
        elif self.space == 'OBJECT':
            pass # TODO understand the difference between tangent space and object space
        elif self.space == 'WORLD':
            normal = si.to_local(normal)

        normal = normal * 2.0 - 1.0 # [0, 1] -> [-1, 1]
        normal = mi.Normal3f(normal.x, -normal.y, normal.z) # Match Blender convention
        normal = dr.normalize(dr.lerp(mi.Normal3f(0, 0, 1), normal, strength))
        normal = (normal + 1.0) * 0.5 # [-1, 1] -> [0, 1]
        return normal

    def to_string(self):
        return f'Texture NormalMap[{self.color}]'

mi.register_texture('normal_map', lambda props: NormalMap(props))
