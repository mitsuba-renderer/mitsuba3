from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi
import numpy as np

def perlin_noise(res, scale, octave, normalize, seed):
    '''
    Generate a perlin noise image of size res x res
    '''
    def perlin_octave(x, y, seed):
        def lerp(a, b, x):
            "linear interpolation"
            return a + x * (b - a)

        def fade(t):
            "6t^5 - 15t^4 + 10t^3"
            return 6 * t**5 - 15 * t**4 + 10 * t**3

        def gradient(h, x, y):
            "grad converts h to the right gradient vector and return the dot product with (x,y)"
            vectors = np.array([[0, 1], [0, -1], [1, 0], [-1, 0]])
            g = vectors[h % 4]
            return g[:, :, 0] * x + g[:, :, 1] * y

        # permutation table
        np.random.seed(seed)
        p = np.arange(res, dtype=int)
        np.random.shuffle(p)
        p = np.stack([p, p]).flatten()
    # coordinates of the top-left
        xi, yi = x.astype(int), y.astype(int)
        # internal coordinates
        xf, yf = x - xi, y - yi
        # fade factors
        u, v = fade(xf), fade(yf)

        xi, yi = xi % res, yi % res

        # noise components
        n00 = gradient(p[p[xi] + yi], xf, yf)
        n01 = gradient(p[p[xi] + yi + 1], xf, yf - 1)
        n11 = gradient(p[p[xi + 1] + yi + 1], xf - 1, yf - 1)
        n10 = gradient(p[p[xi + 1] + yi], xf - 1, yf)
        # combine noises
        x1 = lerp(n00, n10, u)
        x2 = lerp(n01, n11, u)  # FIX1: I was using n10 instead of n01
        return lerp(x1, x2, v)  # FIX2: I also had to reverse x1 and x2 here

    p = np.zeros((res, res))
    for i in range(octave):
        freq = 2**i
        lin = np.linspace(0, freq, res, endpoint=False)
        x, y = np.meshgrid(lin, lin)  # FIX3: I thought I had to invert x and y here but it was a mistake
        p += perlin_octave(x * scale, y * scale, seed) / freq

    min_value = np.min(p)
    max_value = np.max(p)

    # Normalize between [0, 1]
    p = (p - min_value) / (max_value - min_value)

    if not normalize:
        p = (p - 0.5) * octave

    return p[:, :, None]

class NoiseTexture(mi.Texture):
    '''
    Noise Texture generating a perlin-noise signal.
    '''
    def __init__(self, props):
        mi.Texture.__init__(self, props)
        self.noise_type = props.get('noise_type', 'FBM')
        assert self.noise_type == 'FBM', self.noise_type
        self.dimensions = props.get('dimensions', '3D')
        assert self.dimensions == '3D', self.dimensions

        self.normalize = props.get('normalize', True) # TODO use this!

        self.scale      = props.get('scale', 0.0)
        self.detail     = props.get('detail', 0.0)
        self.roughness  = props.get('roughness', 0.0)
        self.lacunarity = props.get('lacunarity', 0.0)
        self.distortion = props.get('distortion', 0.0)

        assert isinstance(self.scale, float), '\'scale\' parameter should be a scalar value!'
        assert isinstance(self.detail, float), '\'detail\' parameter should be a scalar value!'
        assert isinstance(self.roughness, float), '\'roughness\' parameter should be a scalar value!'
        assert isinstance(self.lacunarity, float), '\'lacunarity\' parameter should be a scalar value!'
        assert isinstance(self.distortion, float), '\'distortion\' parameter should be a scalar value!'

        self.resolution = props.get('resolution', 128)

        self.texture_r = mi.Texture2f(perlin_noise(self.resolution, scale=self.scale, octave=int(self.detail) + 1, seed=0, normalize=self.normalize))
        self.texture_g = mi.Texture2f(perlin_noise(self.resolution, scale=self.scale, octave=int(self.detail) + 1, seed=1, normalize=self.normalize))
        self.texture_b = mi.Texture2f(perlin_noise(self.resolution, scale=self.scale, octave=int(self.detail) + 1, seed=2, normalize=self.normalize))

    def traverse(self, cb):
        pass

    def parameters_changed(self, keys):
        pass

    def eval(self, si, active):
        return mi.UnpolarizedSpectrum(self.eval_3(si, active))

    def eval_1(self, si, active):
        return mi.Float(self.texture_r.eval(si.uv, active)[0])

    def eval_3(self, si, active):
        return mi.Color3f(
            self.texture_r.eval(si.uv, active)[0],
            self.texture_g.eval(si.uv, active)[0],
            self.texture_b.eval(si.uv, active)[0],
        )

    def mean(self):
        return mi.Float(0.5) # TODO best effort

    def resolution(self):
        return mi.ScalarVector2i(1, 1)

    def is_spatially_varying(self):
        return True

    def to_string(self):
        return f'NoiseTexture[]'

mi.register_texture('noise', lambda props: NoiseTexture(props))
