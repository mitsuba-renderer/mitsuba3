import pytest
import drjit as dr
import mitsuba as mi
import numpy as np

def test01_vcalls(variants_vec_backends_once_rgb):
    # Create a set of textures to test
    tex1 = mi.load_dict({
        'type' : 'uniform',
        'value' : 5.0
    })
    tex2 = mi.load_dict({
        'type' : 'uniform',
        'value' : 28
    })
    tex3 = mi.load_dict({
        'type' : 'uniform',
        'value' : 133
    })

    # Initialize pointer variable
    textures = dr.zeros(mi.TexturePtr, 6)
    dr.scatter(textures, tex1, mi.UInt32(0,2))
    dr.scatter(textures, tex2, mi.UInt32(1,3))
    dr.scatter(textures, tex3, mi.UInt32(4,5))

    # Evaluate the vcall
    si = dr.zeros(mi.SurfaceInteraction3f)
    result = textures.eval(si, True)

    assert dr.allclose(result, mi.Float(5.0, 28.0, 5.0, 28.0, 133.0, 133.0))

def test02_trampoline(variants_vec_backends_once_rgb):
    class DummyTexture(mi.Texture):
        def __init__(self, props):
            mi.Texture.__init__(self, props)
            self.value = props.get('value')

        def eval(self, si, active):
            return self.value

        def resolution(self):
            return 123

        def to_string(self):
            return f"DummyTexture"

    mi.register_texture('dummy_texture', DummyTexture)
    texture = mi.load_dict({
        'type': 'dummy_texture',
        'value' : 96.0
    })

    assert texture.resolution() == 123
    assert str(texture) == "DummyTexture"

    si = dr.zeros(mi.SurfaceInteraction3f)
    assert dr.allclose(texture.eval(si, True), 96.0)
    ptr = dr.zeros(mi.TexturePtr, 4)
    dr.scatter(ptr, texture, mi.UInt32(0,1,2,3))
    assert dr.allclose(ptr.eval(si, True), 96.0)
