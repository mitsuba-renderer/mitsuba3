from contextlib import contextmanager
import numpy as np

from mitsuba.core import Bitmap, EDebug, Log
from mitsuba.render import (DifferentiableParameters, RadianceSample3fD, ImageBlock)
from mitsuba.render.autodiff_utils import (compute_size, indent)

from enoki import (UInt32D, BoolD, FloatC, FloatD, Vector2fD, Vector3fD,
                   Vector4fD, Matrix4fD, select, eq, rcp, set_requires_gradient,
                   gather, detach, gradient, slices, set_gradient, sqr, sqrt, cuda_malloc_trim)


# ------------------------------------------------------------ Optimizers
class Optimizer:

    def __init__(self, params, lr):
        # Ensure that the JIT does not consider the learning rate as a literal
        self.lr = FloatC(lr, literal=False)
        self.params = params
        self.gradients_acc = {}
        self.gradients_acc_count = 0
        self.state = {}
        for k, p in self.params.items():
            set_requires_gradient(p)
            self.reset(k)

    def reset(self, _):
        pass

    def accumulate_gradients(self, weight = 1):
        """
        Retrieve gradients from the parameters and accumulate their values
        values until the next time `step` is called.
        Useful for e.g. block-based rendering.
        """
        for k, p in self.params.items():
            g = gradient(p)
            if slices(g) <= 0:
                continue
            if k not in self.gradients_acc:
                self.gradients_acc[k] = FloatC.full(0., len(g))
            self.gradients_acc[k] += FloatC(weight) * g
        self.gradients_acc_count += weight

    def compute_gradients(self):
        """
        Returns either the average gradients collected with `accumulate_gradients`,
        or the current gradient values if not available.
        Any accumulated gradients are cleared!
        """
        if self.gradients_acc:
            gg = {k: g / self.gradients_acc_count for k, g in self.gradients_acc.items()}
            self.gradients_acc.clear()
            self.gradients_acc_count = 0
            return gg
        # There was no call to `accumulate_gradients`
        return {k: gradient(p) for k, p in self.params.items()}

    @contextmanager
    def no_gradients(self):
        """Temporarily disables gradients for the optimized parameters."""
        for _, p in self.params.items():
            set_requires_gradient(p, False)
        try:
            yield
        finally:
            for _, p in self.params.items():
                set_requires_gradient(p, True)


class DummyOptimizer(Optimizer):
    """An optimizer which applies a "no-op" step, simply detaching the parameter
    from this iteration's graph."""

    def __init__(self, params):
        super(DummyOptimizer, self).__init__(params, 0.)

    def step(self):
        for k, p in self.params.items():
            self.params[k] = detach(p)
            set_requires_gradient(p)

    def __repr__(self):
        return 'DummyOptimizer[]'


class SGD(Optimizer):

    def __init__(self, params, lr, momentum=0):
        """
        Implements basic stochastic gradient descent with a fixed learning rate
        and, optionally, momentum (0.9 is a typical parameter value).
        """
        super(SGD, self).__init__(params, lr)
        assert momentum >= 0 and momentum < 1
        self.momentum = momentum

    def reset(self, key):
        """Resets the state to a zero-filled array of the right dimension."""
        p = self.params[key]
        size = compute_size(p)
        self.state[key] = detach(type(p).zero(size))

    def step(self):
        gradients = self.compute_gradients()
        for k, p in self.params.items():
            if compute_size(p) != compute_size(self.state[k]):
                # Reset state if data size has changed
                self.reset(k)

            if k not in gradients:
                continue
            g_p = gradients[k]

            if self.momentum == 0:
                self.params[k] = detach(p) - self.lr * g_p
            else:
                self.state[k] = self.momentum * self.state[k] + self.lr * g_p
                self.params[k] = detach(p) - self.state[k]
            set_requires_gradient(p)

    def __repr__(self):
        return 'SGD[\n  lr = {:.2g},\n  momentum = {:.2g}\n]'.format(
            self.lr.numpy().squeeze(), self.momentum)


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
        super(Adam, self).__init__(params, lr)
        assert beta_1 >= 0 and beta_1 < 1
        assert beta_2 >= 0 and beta_2 < 1
        self.beta_1 = beta_1
        self.beta_2 = beta_2
        self.epsilon = epsilon

        self.beta_1_t = FloatC(1, literal=False)
        self.beta_2_t = FloatC(1, literal=False)

    def reset(self, key):
        """Resets the state to a zero-filled array of the right dimension."""
        p = self.params[key]
        size = compute_size(p)
        t = type(p)
        self.state[key] = (detach(t.zero(size)), detach(t.zero(size)))

    def step(self):
        self.beta_1_t *= self.beta_1
        self.beta_2_t *= self.beta_2
        lr_t = self.lr * sqrt(1 - self.beta_2_t) / (1 - self.beta_1_t)

        gradients = self.compute_gradients()
        for k, p in self.params.items():
            if compute_size(p) != compute_size(self.state[k][0]):
                # Reset state if data size has changed
                self.reset(k)

            if k not in gradients:
                continue
            g_t = gradients[k]

            m_tp, v_tp = self.state[k]
            m_t = self.beta_1 * m_tp + (1 - self.beta_1) * g_t
            v_t = self.beta_2 * v_tp + (1 - self.beta_2) * sqr(g_t)
            self.state[k] = (m_t, v_t)
            self.params[k] = detach(p) - lr_t * m_t / \
                (sqrt(v_t) + self.epsilon)
            set_requires_gradient(p)

    def __repr__(self):
        return ('Adam[\n  lr = {:.2g},\n  beta_1 = {:.2g},\n  beta_2 = {:.2g},\n'
                '  params = {}\n]').format(
                    self.lr.numpy().squeeze(), self.beta_1, self.beta_2, indent(str(self.params)))


def get_differentiable_parameters(scene):
    params = DifferentiableParameters()
    children = set()

    def traverse(path, n):
        if n in children:
            return

        path += type(n).__name__
        if hasattr(n, "id"):
            identifier = n.id()
            if len(identifier) > 0 and '[0x' not in identifier:
                path += '[id=\"%s\"]' % identifier
        path += '/'
        params.set_prefix(path)

        children.add(n)

        if hasattr(n, 'put_parameters'):
            n.put_parameters(params)

        if hasattr(n, 'children'):
            for c in n.children():
                traverse(path, c)

    traverse('/', scene)
    return params


# ------------------------------------------------------------ Differentiable rendering

def render(scene, spp=None, pixel_format=Bitmap.EY):
    supported_formats = [Bitmap.EY, Bitmap.EXYZ, Bitmap.ERGB]
    if pixel_format not in supported_formats:
        raise ValueError('Unknown pixel format {} -- must be one of {}'
                         .format(pixel_format, supported_formats))

    block = render_path(scene, spp)

    # The block contains values in its `bitmap_d` storage, ensure the CPU side
    # has the same values.
    im = np.array(block.bitmap(), copy=False)
    floats = block.bitmap_d()
    for k, values in enumerate(floats):
        if len(values) == 1:  # Allow broadcasting
            im[:, :, k] = values.numpy()
        else:
            im[:, :, k] = values.numpy().reshape(im.shape[0], im.shape[1])

    return block


def render_path(scene, spp=None):
    sensor = scene.sensor()
    film = sensor.film()
    sampler = sensor.sampler()
    film_size = film.crop_size()
    if spp is None:
        spp = sampler.sample_count()

    pos = UInt32D.arange(film_size[0] * film_size[1] * spp) / spp

    scale = Vector2fD(1.0 / film_size[0], 1.0 / film_size[1])
    pos = Vector2fD(FloatD(pos % int(film_size[0])),
                    FloatD(pos / int(film_size[0])))

    active = BoolD.full(True, len(pos.x))

    wavelength_sample = sampler.next_1d_d(active)
    position_sample = pos + sampler.next_2d_d(active)

    rays, weights = sensor.sample_ray_differential(
        time=0,
        sample1=wavelength_sample,
        sample2=position_sample * scale,
        sample3=0,
        active=active
    )
    del wavelength_sample

    rs = RadianceSample3fD(scene, sampler)
    result = scene.integrator().eval(rays, rs) * weights
    wavelengths = rays.wavelengths
    del weights, rs, rays

    rfilter = film.reconstruction_filter()
    block = ImageBlock(Bitmap.EXYZAW, film.crop_size(), warn=False,
                       filter=rfilter, border=(rfilter.border_size() > 0))
    block.clear_d()
    block.put(position_sample, wavelengths, result, 1)

    del result, wavelengths, position_sample
    return block


def render_torch(scene, params=None, **kwargs):
    # Delayed import of PyTorch dependency
    ns = globals()
    if 'render_torch_helper' in ns:
        render_torch = ns['render_torch_helper']
    else:
        import torch

        class Render(torch.autograd.Function):

            @staticmethod
            def forward(ctx, *args):
                assert len(args) % 2 == 0
                args = dict(zip(args[0::2], args[1::2]))

                pixel_format = Bitmap.EY
                spp = None

                ctx.inputs = []
                for k, v in args.items():
                    if k == 'spp':
                        spp = v
                    elif k == 'pixel_format':
                        pixel_format = v
                    else:
                        value = type(params[k])(v)
                        set_requires_gradient(value)
                        params[k] = value
                        ctx.inputs.append(None)
                        ctx.inputs.append(value)
                        continue
                    ctx.inputs.append(None)
                    ctx.inputs.append(None)

                ctx.output = render(scene,
                                    spp=spp,
                                    pixel_format=pixel_format)
                result = ctx.output.torch()
                cuda_malloc_trim()
                return result

            @staticmethod
            def backward(ctx, grad_output):
                if type(ctx.output) is FloatD:
                    set_gradient(ctx.output, FloatD(grad_output))
                else:
                    set_gradient(ctx.output, Vector3fD(grad_output))

                FloatD.backward()
                result = tuple(gradient(i).torch() if i is not None else None
                               for i in ctx.inputs)
                del ctx.output
                del ctx.inputs
                return result

        render_torch = Render.apply
        ns['render_torch_helper'] = render_torch

    result = render_torch(*[num for elem in kwargs.items() for num in elem])
    film_size = scene.film().size()

    return result.reshape(film_size[1], film_size[0], -1).squeeze()
