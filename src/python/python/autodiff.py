from contextlib import contextmanager
import enoki as ek

def render(scene, spp=None, sensor_index=0):
    """
    Render the specified Mitsuba scene and return a floating point
    array containing RGB values and AOVs, if applicable
    """
    from mitsuba.core import (Float, UInt32, UInt64, Vector2f, Mask,
        is_monochromatic, is_rgb, is_polarized)
    from mitsuba.render import ImageBlock

    sensor = scene.sensors()[sensor_index]
    film = sensor.film()
    sampler = sensor.sampler()
    film_size = film.crop_size()
    if spp is None:
        spp = sampler.sample_count()

    pos = ek.arange(UInt64, ek.hprod(film_size) * spp)
    if not sampler.ready():
        sampler.seed(pos)

    pos //= spp
    scale = Vector2f(1.0 / film_size[0], 1.0 / film_size[1])
    pos = Vector2f(Float(pos  % int(film_size[0])),
                   Float(pos // int(film_size[0])))

    pos += sampler.next_2d()

    rays, weights = sensor.sample_ray_differential(
        time=0,
        sample1=sampler.next_1d(),
        sample2=pos * scale,
        sample3=0
    )

    spec, mask, aovs = scene.integrator().sample(scene, sampler, rays)
    spec *= weights
    del mask

    if is_polarized:
        from mitsuba.core import depolarize
        spec = depolarize(spec)

    if is_monochromatic:
        rgb = [spec[0]]
    elif is_rgb:
        rgb = spec
    else:
        from mitsuba.core import spectrum_to_xyz, xyz_to_srgb
        xyz = spectrum_to_xyz(spec_u, rays.wavelengths, active);
        rgb = xyz_to_srgb(xyz)
        del xyz

    aovs = [Float(1.0), *rgb] + aovs
    del rgb, spec, weights, rays

    rfilter = film.reconstruction_filter()
    block = ImageBlock(
        size=film.crop_size(),
        channel_count=len(aovs),
        filter=film.reconstruction_filter(),
        warn_negative=False,
        warn_invalid=True,
        border=False
    )

    block.clear()
    block.put(pos, aovs)

    del pos
    del aovs

    data = block.data()

    ch = block.channel_count()
    i = UInt32.arange(ek.hprod(block.size()) * (ch - 1))

    weight_idx = i // (ch - 1) * ch
    values_idx = (i * ch) // (ch - 1) + 1

    weight = ek.gather(data, weight_idx)
    values = ek.gather(data, values_idx)

    return values / (weight + 1e-8)


def write_bitmap(filename, data, size):
    import numpy as np
    from mitsuba.core import Bitmap, Struct

    bitmap = Bitmap(np.array(data).reshape(*size, -1))
    if filename.endswith('.png') or filename.endswith('.jpg'):
        bitmap = bitmap.convert(Bitmap.PixelFormat.RGB,
                                Struct.Type.UInt8, True)
    bitmap.write(filename)


class Optimizer:
    def __init__(self, params, lr):
        self.set_learning_rate(lr)
        self.params = params
        self.state = {}
        for k, p in self.params.items():
            ek.set_requires_gradient(p)
            self._reset(k)

    def set_learning_rate(self, lr):
        from mitsuba.core import Float
        # Ensure that the JIT compiler does merge 'lr' into the PTX code
        # (this would trigger a recompile every time it is changed)
        self.lr = lr
        self.lr_v = ek.detach(Float(lr, literal=False))

    @contextmanager
    def disable_gradients(self):
        """Temporarily disables gradients for the optimized parameters."""
        for _, p in self.params.items():
            ek.set_requires_gradient(p, False)
        try:
            yield
        finally:
            for _, p in self.params.items():
                ek.set_requires_gradient(p, True)


class SGD(Optimizer):
    def __init__(self, params, lr, momentum=0):
        """
        Implements basic stochastic gradient descent with a fixed learning rate
        and, optionally, momentum (0.9 is a typical parameter value).
        """
        assert momentum >= 0 and momentum < 1
        self.momentum = momentum
        super().__init__(params, lr)

    def step(self):
        """ Take a gradient step """
        for k, p in self.params.items():
            g_p = ek.gradient(p)
            size = ek.slices(g_p)
            if size == 0:
                continue

            if self.momentum != 0:
                if size != ek.slices(self.state[k]):
                    # Reset state if data size has changed
                    self._reset(k)

                self.state[k] = self.momentum * self.state[k] + \
                                self.lr_v * g_p
                value = ek.detach(p) - self.state[k]
            else:
                value = ek.detach(p) - self.lr_v * g_p

            value = type(p)(value)
            ek.set_requires_gradient(value)
            self.params[k] = value
        self.params.update()

    def _reset(self, key):
        """ Zero-initializes the internal state associated with a parameter """
        if self.momentum == 0:
            return
        p = self.params[key]
        size = ek.slices(p)
        self.state[key] = ek.detach(type(p).zero(size))

    def __repr__(self):
        return ('SGD[\n  lr = %.2g,\n  momentum = %.2g\n]') % \
                (self.lr, self.momentum)


class Adam(Optimizer):
    def __init__(self, params, lr, beta_1=0.9, beta_2=0.999, epsilon=1e-8):
        """
        Implements the optimization technique presented in

        "Adam: A Method for Stochastic Optimization"
        Diederik P. Kingma and Jimmy Lei Ba
        ICLR 2015

        The method has the following parameters:

        ``lr``: learning rate

        ``beta_1``: controls the exponential averaging of first
        order gradient moments

        ``beta_2``: controls the exponential averaging of second
        order gradient moments
        """
        super().__init__(params, lr)
        assert beta_1 >= 0 and beta_1 < 1
        assert beta_2 >= 0 and beta_2 < 1
        self.beta_1 = beta_1
        self.beta_2 = beta_2
        self.epsilon = epsilon

        from mitsuba.core import Float
        self.beta_1_t = ek.detach(Float(1, literal=False))
        self.beta_2_t = ek.detach(Float(1, literal=False))


    def step(self):
        self.beta_1_t *= self.beta_1
        self.beta_2_t *= self.beta_2
        lr_t = self.lr * ek.sqrt(1 - self.beta_2_t) / (1 - self.beta_1_t)

        for k, p in self.params.items():
            g_p = ek.gradient(p)
            size = ek.slices(g_p)
            if size == 0:
                continue
            elif size != ek.slices(self.state[k][0]):
                # Reset state if data size has changed
                self._reset(k)

            m_tp, v_tp = self.state[k]
            m_t = self.beta_1 * m_tp + (1 - self.beta_1) * g_p
            v_t = self.beta_2 * v_tp + (1 - self.beta_2) * ek.sqr(g_p)
            self.state[k] = (m_t, v_t)
            u = ek.detach(p) - lr_t * m_t / (ek.sqrt(v_t) + self.epsilon)
            u = type(p)(u)
            ek.set_requires_gradient(u)
            self.params[k] = u

    def _reset(self, key):
        """ Zero-initializes the internal state associated with a parameter """
        p = self.params[key]
        size = ek.slices(p)
        self.state[key] = (ek.detach(type(p).zero(size)), ek.detach(type(p).zero(size)))

    def __repr__(self):
        return ('Adam[\n  lr = {:.2g},\n  beta_1 = {:.2g},'
                '\n  beta_2 = {:.2g}\n]').format(self.lr, self.beta_1, self.beta_2)
