from __future__ import annotations # Delayed parsing of type annotations

import mitsuba as mi

class MeshAttributeAdapter(mi.Texture):
    '''
    Ensure we always query the mesh attributes using eval instead of eval_1 and eval_3
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.attribute = props.get_texture('attribute')

    def traverse(self, cb):
        cb.put('attribute', self.attribute, +mi.ParamFlags.Differentiable)

    def parameters_changed(self, keys):
        pass

    def eval(self, si, active):
        return mi.UnpolarizedSpectrum(self.attribute.eval(si, active))

    def eval_1(self, si, active):
        return mi.Float(mi.luminance(self.attribute.eval(si, active)))

    def eval_3(self, si, active):
        return mi.Color3f(self.attribute.eval(si, active))

    def mean(self):
        return mi.luminance(self.attribute.mean())

    def to_string(self):
        return f'MeshAttributeAdapter[{self.attribute}]'

mi.register_texture('mesh_attribute_adapter', lambda props: MeshAttributeAdapter(props))
