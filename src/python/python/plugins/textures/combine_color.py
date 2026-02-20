from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

class CombineColor(mi.Texture):
    '''
    RGB to BW Blender shader node texture.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.mode = props.get('mode', 'RGB')
        assert self.mode == 'RGB', self.mode
        self.R = props.get_texture('red', 0.0)
        self.G = props.get_texture('green', 0.0)
        self.B = props.get_texture('blue', 0.0)

    def traverse(self, cb):
        cb.put('R', self.R, +mi.ParamFlags.Differentiable)
        cb.put('G', self.G, +mi.ParamFlags.Differentiable)
        cb.put('B', self.B, +mi.ParamFlags.Differentiable)

    def parameters_changed(self, keys):
        pass

    def eval(self, si, active):
        return mi.UnpolarizedSpectrum(self.eval_3(si, active))

    def eval_3(self, si, active):
        return mi.Color3f(
            self.R.eval_1(si, active),
            self.G.eval_1(si, active),
            self.B.eval_1(si, active)
        )

    def mean(self):
        return self.color.mean() # TODO best effort

    def resolution(self):
        return self.color.resolution()

    def is_spatially_varying(self):
        return any([t.is_spatially_varying() for t in [self.R, self.G, self.B]])

    def to_string(self):
        return f'CombineColor[mode={self.mode}, R={self.R}, G={self.G}, B={self.B}]'

mi.register_texture('combine_color', lambda props: CombineColor(props))
