from contextlib import contextmanager
from typing import Union, Tuple
import enoki as ek
import mitsuba


def _render_helper(scene, spp=None, sensor_index=0):
    """
    Internally used function: render the specified Mitsuba scene and return a
    floating point array containing RGB values and AOVs, if applicable
    """
    from mitsuba.core import (Float, UInt32, UInt64, Vector2f,
                              is_monochromatic, is_rgb, is_polarized)
    from mitsuba.render import ImageBlock

    sensor = scene.sensors()[sensor_index]
    film = sensor.film()
    sampler = sensor.sampler()
    film_size = film.crop_size()
    if spp is None:
        spp = sampler.sample_count()

    total_sample_count = ek.hprod(film_size) * spp

    if sampler.wavefront_size() != total_sample_count:
        sampler.seed(ek.arange(UInt64, total_sample_count))

    pos = ek.arange(UInt32, total_sample_count)
    pos //= spp
    scale = Vector2f(1.0 / film_size[0], 1.0 / film_size[1])
    pos = Vector2f(Float(pos % int(film_size[0])),
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
        xyz = spectrum_to_xyz(spec, rays.wavelengths)
        rgb = xyz_to_srgb(xyz)
        del xyz

    aovs.insert(0, Float(1.0))
    for i in range(len(rgb)):
        aovs.insert(i + 1, rgb[i])
    del rgb, spec, weights, rays

    block = ImageBlock(
        size=film.crop_size(),
        channel_count=len(aovs),
        filter=film.reconstruction_filter(),
        warn_negative=False,
        warn_invalid=False,
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


def write_bitmap(filename, data, resolution):
    """
    Write the linearized RGB image in `data` to a PNG/EXR/.. file with
    resolution `resolution`.
    """
    import numpy as np
    from mitsuba.core import Bitmap, Struct

    if type(data).__name__ == 'Tensor':
        data = data.detach().cpu()

    data = np.array(data.numpy())
    data = data.reshape(*resolution, -1)
    bitmap = Bitmap(data)
    if filename.endswith('.png') or \
       filename.endswith('.jpg') or \
       filename.endswith('.jpeg'):
        bitmap = bitmap.convert(Bitmap.PixelFormat.RGB,
                                Struct.Type.UInt8, True)
    bitmap.write_async(filename, quality=0 if filename.endswith('png') else -1)


def render(scene,
           spp: Union[None, int, Tuple[int, int]] = None,
           unbiased=False,
           optimizer: 'mitsuba.python.autodiff.Optimizer' = None,
           sensor_index=0):
    """
    Perform a differentiable of the scene `scene`, returning a floating point
    array containing RGB values and AOVs, if applicable.

    Parameter ``spp`` (``None``, ``int``, or a 2-tuple ``(int, int)``):

        Specifies the number of samples per pixel to be used for rendering,
        overriding the value that is specified in the scene. If ``spp=None``,
        the original value takes precedence. If ``spp`` is a 2-tuple
        ``(spp_primal: int, spp_deriv: int)``, the first element specifies the
        number of samples for the *primal* pass, and the second specifies the
        number of samples for the *derivative* pass. See the explanation of the
        ``unbiased`` parameter for further detail on what these mean.

        Memory usage is roughly proportional to the ``spp``, value, hence this
        parameter should be reduced if you encounter out-of-memory errors.

    Parameter ``unbiased`` (``bool``):

        One potential issue when naively differentiating a rendering algorithm
        is that the same set of Monte Carlo sample is used to generate both the
        primal output (i.e. the image) along with derivative output. When the
        rendering algorithm and objective are jointly differentiated, we end up
        with expectations of products that do *not* satisfy the equality
        :math:`\mathbb{E}[X Y]=\mathbb{E}[X]\, \mathbb{E}[Y]` due to
        correlations between :math:`X` and :math:`Y` that result from this
        sample re-use.

        When ``unbiased=True``, the ``render()`` function will generate an
        *unbiased* estimate that de-correlates primal and derivative
        components, which boils down to rendering the image twice and naturally
        comes at some cost in performance :math:`(\sim 1.6 \times\!)`. Often,
        biased gradients are good enough, in which case ``unbiased=False``
        should be specified instead.

        The number of samples per pixel per pass can be specified separately
        for both passes by passing a tuple to the ``spp`` parameter.

        Note that unbiased mode is only relevant for reverse-mode
        differentiation. It is not needed when visualizing parameter gradients
        in image space using forward-mode differentiation.

    Parameter ``optimizer`` (:py:class:`mitsuba.python.autodiff.Optimizer`):

        The optimizer referencing relevant scene parameters must be specified
        when ``unbiased=True``. Otherwise, there is no need to provide this
        parameter.

    Parameter ``sensor_index`` (``int``):

        When the scene contains more than one sensor/camera, this parameter
        can be specified to select the desired sensor.

    """
    if unbiased:
        if optimizer is None:
            raise Exception('render(): unbiased=True requires that an '
                            'optimizer is specified!')
        if not type(spp) is tuple:
            spp = (spp, spp)

        with optimizer.disable_gradients():
            image = _render_helper(scene, spp=spp[0],
                                   sensor_index=sensor_index)
        image_diff = _render_helper(scene, spp=spp[1],
                                    sensor_index=sensor_index)
        ek.reattach(image, image_diff)
    else:
        if type(spp) is tuple:
            raise Exception('render(): unbiased=False requires that spp '
                            'is either an integer or None!')
        image = _render_helper(scene, spp=spp, sensor_index=sensor_index)

    return image


class Optimizer:
    """
    Base class of all gradient-based optimizers (currently SGD and Adam)
    """
    def __init__(self, params, lr):
        """
        Parameter ``params``:
            dictionary ``(name: variable)`` of differentiable parameters to be
            optimized.

        Parameter ``lr``:
            learning rate
        """
        self.set_learning_rate(lr)
        self.params = params
        self.state = {}
        for k, p in self.params.items():
            ek.set_requires_gradient(p)
            self._reset(k)

    def set_learning_rate(self, lr):
        """Set the learning rate."""
        from mitsuba.core import Float
        # Ensure that the JIT compiler does merge 'lr' into the PTX code
        # (this would trigger a recompile every time it is changed)
        self.lr = lr
        self.lr_v = ek.detach(Float(lr, literal=False))

    @contextmanager
    def disable_gradients(self):
        """Temporarily disable the generation of gradients."""
        for _, p in self.params.items():
            ek.set_requires_gradient(p, False)
        try:
            yield
        finally:
            for _, p in self.params.items():
                ek.set_requires_gradient(p, True)


class SGD(Optimizer):
    """
    Implements basic stochastic gradient descent with a fixed learning rate
    and, optionally, momentum (0.9 is a typical parameter value).
    """
    def __init__(self, params, lr, momentum=0):
        """
        Parameter ``lr``:
            learning rate

        Parameter ``momentum``:
            momentum factor
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
    """
    Implements the optimization technique presented in

    "Adam: A Method for Stochastic Optimization"
    Diederik P. Kingma and Jimmy Lei Ba
    ICLR 2015
    """
    def __init__(self, params, lr, beta_1=0.9, beta_2=0.999, epsilon=1e-8):
        """
        Parameter ``lr``:
            learning rate

        Parameter ``beta_1``:
            controls the exponential averaging of first
            order gradient moments

        Parameter ``beta_2``:
            controls the exponential averaging of second
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
        self.state[key] = (ek.detach(type(p).zero(size)),
                           ek.detach(type(p).zero(size)))

    def __repr__(self):
        return ('Adam[\n  lr = {:.2g},\n  beta_1 = {:.2g},'
                '\n  beta_2 = {:.2g}\n]').format(self.lr, self.beta_1,
                                                 self.beta_2)


def render_torch(scene, params=None, **kwargs):
    from mitsuba.core import Float
    # Delayed import of PyTorch dependency
    ns = globals()
    if 'render_torch_helper' in ns:
        render_torch = ns['render_torch_helper']
    else:
        import torch

        class Render(torch.autograd.Function):
            @staticmethod
            def forward(ctx, scene, params, *args):
                try:
                    assert len(args) % 2 == 0
                    args = dict(zip(args[0::2], args[1::2]))

                    spp = None
                    sensor_index = 0
                    unbiased = True

                    params_todo = {}
                    ctx.inputs = [None, None]
                    for k, v in args.items():
                        if k == 'spp':
                            spp = v
                        elif k == 'sensor_index':
                            sensor_index = v
                        elif k == 'unbiased':
                            unbiased = v
                        else:
                            value = type(params[k])(v)
                            ek.set_requires_gradient(value, v.requires_grad)
                            params_todo[k] = value
                            ctx.inputs.append(None)
                            ctx.inputs.append(value)
                            continue

                        ctx.inputs.append(None)
                        ctx.inputs.append(None)

                    result = None
                    if params is not None:
                        if unbiased:
                            for k in params.keys():
                                ek.set_requires_gradient(params[k], False)
                            result = render(scene, spp=spp,
                                            sensor_index=sensor_index).torch()

                        for k, v in params_todo.items():
                            params[k] = v
                        params.update()

                    ctx.output = render(scene, spp=spp,
                                        sensor_index=sensor_index)

                    if result is None:
                        result = ctx.output.torch()

                    ek.cuda_malloc_trim()
                    return result
                except Exception as e:
                    print("render_torch(): critical exception during "
                          "forward pass: %s" % str(e))
                    raise e

            @staticmethod
            def backward(ctx, grad_output):
                try:
                    ek.set_gradient(ctx.output, ek.detach(Float(grad_output)))
                    Float.backward()
                    result = tuple(ek.gradient(i).torch() if i is not None
                                   else None
                                   for i in ctx.inputs)
                    del ctx.output
                    del ctx.inputs
                    ek.cuda_malloc_trim()
                    return result
                except Exception as e:
                    print("render_torch(): critical exception during "
                          "backward pass: %s" % str(e))
                    raise e

        render_torch = Render.apply
        ns['render_torch_helper'] = render_torch

    result = render_torch(scene, params,
                          *[num for elem in kwargs.items() for num in elem])

    sensor_index = 0 if 'sensor_index' not in kwargs \
        else kwargs['sensor_index']
    crop_size = scene.sensors()[sensor_index].film().crop_size()
    return result.reshape(crop_size[1], crop_size[0], -1)
