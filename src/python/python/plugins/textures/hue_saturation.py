from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

def modulo(a, b):
    return (a - mi.Int32(a)) + mi.Int32(a) % b

def rgb2hsv(rgb):
    """
    Convert RGB colors to HSV.
    """
    # Calculate maximum and minimum RGB values
    max_rgb = dr.max(rgb)
    min_rgb = dr.min(rgb)

    # Calculate delta RGB
    delta = max_rgb - min_rgb

    # Initialize HSV array
    hsv = mi.Color3f(0, 0, max_rgb)

    R, G, B = rgb.x, rgb.y, rgb.z

    # Calculate Hue
    hsv.x[delta == 0] = 0.0
    hsv.x[(R >= G) & (G >= B)] = 60 * (G - B) / delta
    hsv.x[(G >= R) & (R >= B)] = 60 * (2.0 - (R - B) / delta)
    hsv.x[(G >= B) & (B >= R)] = 60 * (2.0 + (B - R) / delta)
    hsv.x[(B >= G) & (G >= R)] = 60 * (4.0 - (G - R) / delta)
    hsv.x[(B >= R) & (R >= G)] = 60 * (4.0 + (R - G) / delta)
    hsv.x[(R >= B) & (B >= G)] = 60 * (6.0 - (B - G) / delta)

    # Calculate Saturation
    hsv.y = dr.select(max_rgb == 0, 0, delta / max_rgb)

    return hsv

def hsv2rgb(hsv):
    """
    Convert HSV colors to RGB.
    """
    # Make sure we got a valid hue value
    hsv.x = modulo(dr.abs(hsv.x), 360)

    # Calculate chroma
    chroma = hsv.z * hsv.y

    # Calculate intermediate values
    x = chroma * (1.0 - dr.abs(modulo(hsv.x / 60.0, 2) - 1.0))

    # Calculate RGB components based on hue
    rgb = mi.Color3f(0.0)
    rgb[(0.0 <= hsv.x) & (hsv.x < 60.0)]    = mi.Color3f(chroma, x, 0)
    rgb[(60.0 <= hsv.x) & (hsv.x < 120.0)]  = mi.Color3f(x, chroma, 0)
    rgb[(120.0 <= hsv.x) & (hsv.x < 180.0)] = mi.Color3f(0, chroma, x)
    rgb[(180.0 <= hsv.x) & (hsv.x < 240.0)] = mi.Color3f(0, x, chroma)
    rgb[(240.0 <= hsv.x) & (hsv.x < 300.0)] = mi.Color3f(x, 0, chroma)
    rgb[(300.0 <= hsv.x) & (hsv.x < 360.0)] = mi.Color3f(chroma, 0, x)

    # Add lightness to RGB components
    rgb += (hsv.z - chroma)

    return rgb

class HueSaturationValue(mi.Texture):
    '''
    Hue-Saturation-Value Blender shader node texture.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.hue        = props.get_texture('hue', 0.5)
        self.saturation = props.get_texture('saturation', 1.0)
        self.value      = props.get_texture('value', 1.0)
        self.mix        = props.get_texture('mix', 1.0)
        self.input      = props.get_texture('input', 1.0)

    def traverse(self, cb):
        cb.put('hue',        self.hue,        +mi.ParamFlags.Differentiable)
        cb.put('saturation', self.saturation, +mi.ParamFlags.Differentiable)
        cb.put('value',      self.value,      +mi.ParamFlags.Differentiable)
        cb.put('mix',        self.mix,        +mi.ParamFlags.Differentiable)
        cb.put('input',      self.input,      +mi.ParamFlags.Differentiable)

    def parameters_changed(self, keys):
        pass

    def eval(self, si, active):
        return mi.UnpolarizedSpectrum(self.eval_3(si, active))

    def eval_1(self, si, active):
        return mi.Float(self.input.eval_1(si, active) * self.value.eval_1(si, active))

    def eval_3(self, si, active):
        hue        = self.hue.eval_1(si, active)
        saturation = self.saturation.eval_1(si, active)
        value      = self.value.eval_1(si, active)
        mix        = self.mix.eval_1(si, active)
        color      = self.input.eval_3(si, active)

        hsv = rgb2hsv(color)

        hsv.x = hsv.x + 360.0 * (hue - 0.5)
        hsv.y = dr.clip(hsv.y * saturation, 0.0, 1.0)
        hsv.z *= value

        return mi.Color3f(dr.lerp(color, hsv2rgb(hsv), mix))

    def mean(self):
        return self.input.mean() # TODO best effort

    def resolution(self):
        return self.input.resolution()

    def is_spatially_varying(self):
        return any([t.is_spatially_varying() for t in [
            self.hue, self.saturation, self.value, self.mix, self.input
        ]])

    def to_string(self):
        return f'HueSaturationValue[hue={self.hue}, saturation={self.saturation}, value={self.value}, mix={self.mix}, color={self.color}]'

mi.register_texture('hue_saturation_value', lambda props: HueSaturationValue(props))
