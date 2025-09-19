from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

class UDIMTexture(mi.Texture):
    '''
    UDIM Blender shader node texture.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)

        self.textures = []
        for name in props.unqueried():
            obj = props.get(name)
            if issubclass(type(obj), mi.Texture):
                self.textures.append(obj)
            else:
                raise Exception('Invalid nested texture: ', name, obj)

    def traverse(self, cb):
        for i, texture in enumerate(self.textures):
            cb.put(f'texture_{i:02d}', texture, +mi.ParamFlags.Differentiable)

    def parameters_changed(self, keys):
        pass

    def eval(self, si, active):
        si = mi.SurfaceInteraction3f(si)
        idx = mi.UInt32(si.uv.x)
        si.uv.x -= idx

        value = mi.UnpolarizedSpectrum(0)
        for i, texture in enumerate(self.textures):
            value[idx == i] = mi.UnpolarizedSpectrum(texture.eval(si, active))

        return value

    def eval_1(self, si, active):
        si = mi.SurfaceInteraction3f(si)
        idx = mi.UInt32(si.uv.x)
        si.uv.x -= idx

        value = mi.Float(0)
        for i, texture in enumerate(self.textures):
            value[idx == i] = texture.eval_1(si, active)

        return value

    def eval_3(self, si, active):
        si = mi.SurfaceInteraction3f(si)
        idx = mi.UInt32(si.uv.x)
        si.uv.x -= idx

        value = mi.Color3f(0)
        for i, texture in enumerate(self.textures):
            value[idx == i] = texture.eval_3(si, active)

        return value

    def mean(self):
        return mi.Float(0.5) # TODO best effort

    def resolution(self):
        return mi.ScalarVector2i(1, 1)

    def is_spatially_varying(self):
        return True

    def to_string(self):
        return f'UDIMTexture[]'

mi.register_texture('udim_texture', lambda props: UDIMTexture(props))
