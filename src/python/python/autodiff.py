from contextlib import contextmanager
from typing import Union, Tuple
import enoki as ek


def _render_helper(scene, spp=None, sensor_index=0):
    """
    Internally used function: render the specified Mitsuba scene and return a
    floating point array containing RGB values and AOVs, if applicable
    """
    from mitsuba.core import (Float, UInt32, UInt64, Vector2f,
                              is_monochromatic, is_rgb, is_polarized, DEBUG)
    from mitsuba.render import ImageBlock

    sensor = scene.sensors()[sensor_index]
    film = sensor.film()
    sampler = sensor.sampler()
    film_size = film.crop_size()
    if spp is None:
        spp = sampler.sample_count()

    total_sample_count = ek.hprod(film_size) * spp

    if sampler.wavefront_size() != total_sample_count:
        sampler.seed(0, total_sample_count)

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
        warn_invalid=DEBUG,
        border=False
    )

    block.clear()
    block.put(pos, aovs)

    del pos
    del aovs

    data = block.data()

    ch = block.channel_count()
    i = ek.arange(UInt32, ek.hprod(block.size()) * (ch - 1))

    weight_idx = i // (ch - 1) * ch
    values_idx = (i * ch) // (ch - 1) + 1

    weight = ek.gather(Float, data, weight_idx)
    values = ek.gather(Float, data, values_idx)

    return values / (weight + 1e-8)


def write_bitmap(filename, data, resolution, write_async=True):
    """
    Write the linearized RGB image in `data` to a PNG/EXR/.. file with
    resolution `resolution`.
    """
    import numpy as np
    from mitsuba.core import Bitmap, Struct

    if type(data).__name__ == 'Tensor':
        data = data.detach().cpu()

    data = np.array(data.numpy())
    data = data.reshape(resolution[1], resolution[0], -1)
    bitmap = Bitmap(data)
    if filename.endswith('.png') or \
       filename.endswith('.jpg') or \
       filename.endswith('.jpeg'):
        bitmap = bitmap.convert(Bitmap.PixelFormat.RGB,
                                Struct.Type.UInt8, True)
    quality = 0 if filename.endswith('png') else -1

    if write_async:
        bitmap.write_async(filename, quality=quality)
    else:
        bitmap.write(filename, quality=quality)


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
    if unbiased: # TODO refactoring
        if optimizer is None:
            raise Exception('render(): unbiased=True requires that an '
                            'optimizer is specified!')
        if not type(spp) is tuple:
            spp = (spp, spp)

        with optimizer.suspend_gradients():
            image = _render_helper(scene, spp=spp[0],
                                   sensor_index=sensor_index)
        image_diff = _render_helper(scene, spp=spp[1],
                                    sensor_index=sensor_index)
        ek.replace_grad(image, image_diff)
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
    def __init__(self, lr, params):
        """
        Parameter ``lr``:
            learning rate

        Parameter ``params`` (:py:class:`mitsuba.python.util.SceneParameters`):
            Scene parameters dictionary
        """
        self.set_learning_rate(lr)
        self.variables = {}
        self.params = params
        self.state = {}

    def __contains__(self, key: str):
        return self.variables.__contains__(key)

    def __getitem__(self, key: str):
        return self.variables[key]

    def __setitem__(self, key: str, value):
        if not (ek.is_diff_array_v(value) and ek.is_floating_point_v(value)):
            raise Exception('Optimizer.__setitem__(): value should be differentiable!')
        self.variables[key] = type(value)(ek.detach(value))
        ek.enable_grad(self.variables[key])
        self._reset(key)

    def __delitem__(self, key: str) -> None:
        del self.variables[key]

    def __len__(self) -> int:
        return len(self.variables)

    def keys(self):
        return self.variables.keys()

    def items(self):
        class OptimizerItemIterator:
            def __init__(self, params):
                self.params = params
                self.it = params.keys().__iter__()

            def __iter__(self):
                return self

            def __next__(self):
                key = next(self.it)
                return (key, self.params[key])

        return OptimizerItemIterator(self.variables)

    def __match(regexps, params):
        """Return subset of keys matching any regex from a list of regex"""
        if params is None:
            return []

        if regexps is None:
            return params.keys()

        if regexps is not list:
            regexps = [regexps]

        import re
        regexps = [re.compile(k).match for k in regexps]
        return [k for k in params.keys() if any (r(k) for r in regexps)]

    def load(self, keys=None):
        """
        Load a set of scene parameters to optimize.

        Parameter ``keys`` (``None``, ``str``, ``[str]``):
            Specifies which parameters should be loaded. Regex are supported to define
            a subset of parameters at once. If set to ``None``, all differentiable
            scene parameters will be loaded.
        """

        if self.params is None:
            raise Exception('Optimizer.load(): scene parameters dictionary should be'
                            'passed to the constructor!')

        for k in Optimizer.__match(keys, self.params):
            value = self.params[k]
            if ek.is_diff_array_v(value) and ek.is_floating_point_v(value):
                self[k] = value

    def update(self, keys=None):
        """
        Update a set of scene parameters.

        Parameter ``params`` (:py:class:`mitsuba.python.util.SceneParameters`):
            Scene parameters dictionary

        Parameter ``keys`` (``None``, ``str``, ``[str]``):
            Specifies which parameters should be updated. Regex are supported to update
            a subset of parameters at once. If set to ``None``, all scene parameters will
            be update.
        """

        if self.params is None:
            raise Exception('Optimizer.update(): scene parameters dictionary should be'
                            'passed to the constructor!')

        for k in Optimizer.__match(keys, self.variables):
            if k in self.params:
                self.params[k] = self.variables[k]
                ek.schedule(self.params[k])

        ek.eval()

        self.params.update()

    def set_learning_rate(self, lr):
        """Set the learning rate."""
        from mitsuba.core import Float
        # Ensure that the JIT compiler does merge 'lr' into the PTX code
        # (this would trigger a recompile every time it is changed)
        self.lr = lr
        self.lr_v = ek.full(ek.detached_t(Float), lr, size=1, eval=True)

    @contextmanager
    def suspend_gradients(self):
        """Temporarily disable the generation of gradients."""
        self.params.set_grad_suspended(True)
        try:
            yield
        finally:
            self.params.set_grad_suspended(False)


class SGD(Optimizer):
    """
    Implements basic stochastic gradient descent with a fixed learning rate
    and, optionally, momentum :cite:`Sutskever2013Importance` (0.9 is a typical
    parameter value for the ``momentum`` parameter).

    The momentum-based SGD uses the update equation

    .. math::

        v_{i+1} = \\mu \\cdot v_i +  g_{i+1}

    .. math::
        p_{i+1} = p_i + \\varepsilon \\cdot v_{i+1},

    where :math:`v` is the velocity, :math:`p` are the positions,
    :math:`\\varepsilon` is the learning rate, and :math:`\\mu` is
    the momentum parameter.
    """
    def __init__(self, lr, momentum=0, params=None):
        """
        Parameter ``lr``:
            learning rate

        Parameter ``momentum``:
            momentum factor
        """
        assert momentum >= 0 and momentum < 1
        assert lr > 0
        self.momentum = momentum
        super().__init__(lr, params)

    def step(self):
        """ Take a gradient step """
        for k, p in self.variables.items():
            g_p = ek.grad(p)
            size = ek.width(g_p)
            if size == 0:
                continue

            if self.momentum != 0:
                if size != ek.width(self.state[k]):
                    # Reset state if data size has changed
                    self._reset(k)

                self.state[k] = self.momentum * self.state[k] + g_p
                value = ek.detach(p) - self.lr_v * self.state[k]
                ek.schedule(self.state[k])
            else:
                value = ek.detach(p) - self.lr_v * g_p


            value = type(p)(value)
            ek.enable_grad(value)
            self.variables[k] = value

        ek.eval()

    def _reset(self, key):
        """ Zero-initializes the internal state associated with a parameter """
        if self.momentum == 0:
            return
        p = self.variables[key]
        size = ek.width(p)
        self.state[key] = ek.zero(ek.detached_t(p), size)

    def __repr__(self):
        return ('SGD[\n  params = %s,\n  lr = %.2g,\n  momentum = %.2g\n]') % \
            (list(self.keys()), self.lr, self.momentum)


class Adam(Optimizer):
    """
    Implements the Adam optimizer presented in the paper *Adam: A Method for
    Stochastic Optimization* by Kingman and Ba, ICLR 2015.
    """
    def __init__(self, lr, beta_1=0.9, beta_2=0.999, epsilon=1e-8, params=None):
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
        super().__init__(lr, params)

        assert 0 <= beta_1 < 1 and 0 <= beta_2 < 1 \
            and lr > 0 and epsilon > 0

        self.beta_1 = beta_1
        self.beta_2 = beta_2
        self.epsilon = epsilon
        self.t = 0

    def step(self):
        """ Take a gradient step """
        self.t += 1

        from mitsuba.core import Float

        lr_t = self.lr * ek.sqrt(1 - self.beta_2**self.t) / (1 - self.beta_1**self.t)
        lr_t = ek.full(ek.detached_t(Float), lr_t, size=1, eval=True)

        for k, p in self.variables.items():
            g_p = ek.grad(p)
            size = ek.width(g_p)

            if size == 0:
                continue
            elif size != ek.width(self.state[k][0]):
                # Reset state if data size has changed
                self._reset(k)

            m_tp, v_tp = self.state[k]
            m_t = self.beta_1 * m_tp + (1 - self.beta_1) * g_p
            v_t = self.beta_2 * v_tp + (1 - self.beta_2) * ek.sqr(g_p)
            self.state[k] = (m_t, v_t)
            ek.schedule(self.state[k])

            u = ek.detach(p) - lr_t * m_t / (ek.sqrt(v_t) + self.epsilon)
            u = type(p)(u)
            ek.enable_grad(u)
            self.variables[k] = u

        ek.eval()

    def _reset(self, key):
        """ Zero-initializes the internal state associated with a parameter """
        p = self.variables[key]
        size = ek.width(p)
        self.state[key] = (ek.zero(ek.detached_t(p), size),
                           ek.zero(ek.detached_t(p), size))

    def __repr__(self):
        return ('Adam[\n'
                '  params = %s,\n'
                '  lr = %g,\n'
                '  betas = (%g, %g),\n'
                '  eps = %g\n'
                ']' % (list(self.keys()), self.lr, self.beta_1, self.beta_2, self.epsilon))


# TODO refactoring
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
                    malloc_trim = False

                    ctx.inputs = [None, None]
                    for k, v in args.items():
                        if k == 'spp':
                            spp = v
                        elif k == 'sensor_index':
                            sensor_index = v
                        elif k == 'unbiased':
                            unbiased = v
                        elif k == 'malloc_trim':
                            malloc_trim = v
                        elif params is not None:
                            params[k] = type(params[k])(v)
                            ctx.inputs.append(None)
                            ctx.inputs.append(params[k] if v.requires_grad
                                              else None)
                            continue

                        ctx.inputs.append(None)
                        ctx.inputs.append(None)

                    if type(spp) is not tuple:
                        spp = (spp, spp)

                    result = None
                    ctx.malloc_trim = malloc_trim

                    if ctx.malloc_trim:
                        torch.cuda.empty_cache()

                    if params is not None:
                        params.update()

                        if unbiased:
                            result = render(scene, spp=spp[0],
                                            sensor_index=sensor_index).torch()

                    for v in ctx.inputs:
                        if v is not None:
                            ek.enable_grad(v)

                    ctx.output = render(scene, spp=spp[1],
                                        sensor_index=sensor_index)

                    if result is None:
                        result = ctx.output.torch()

                    if ctx.malloc_trim:
                        ek.cuda_malloc_trim()
                    return result
                except Exception as e:
                    print("render_torch(): critical exception during "
                          "forward pass: %s" % str(e))
                    raise e

            @staticmethod
            def backward(ctx, grad_output):
                try:
                    ek.set_grad(ctx.output, ek.detach(Float(grad_output)))
                    ek.backward(ctx.output)
                    result = tuple(ek.grad(i).torch() if i is not None
                                   else None
                                   for i in ctx.inputs)
                    del ctx.output
                    del ctx.inputs
                    if ctx.malloc_trim:
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


del Union
del Tuple
