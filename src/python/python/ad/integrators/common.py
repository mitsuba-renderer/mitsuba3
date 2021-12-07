from __future__ import annotations # Delayed parsing of type annotations

import enoki as ek
from typing import Union

# -------------------------------------------------------------------
# Default implementatio of SamplingIntegrator.render_forward/backward
# -------------------------------------------------------------------

def render_forward_impl(self: mitsuba.render.SamplingIntegrator,
                        scene: mitsuba.render.Scene,
                        params: mitsuba.python.util.SceneParameters,
                        seed: int,
                        sensor: Union[int, mitsuba.render.Sensor] = 0,
                        spp: int = 0) -> mitsuba.core.TensorXf:
    """
    Evaluates the forward-mode derivative of the rendering step.

    Forward-mode differentiation propagates gradients from the ``params`` scene
    parameter data structure through the simulation, producing a *gradient
    image* (i.e., the corresponding derivative of the rendered image). The
    gradient image is very helpful for debugging, and to inspect the gradient
    variance or region of influence of a scene parameter. It is not
    particularly useful for simultaneous optimization of many parameters, see
    ``render_backward()`` for this.

    Before calling this function, you must first associate gradients with
    variables in ``params`` (via ``enoki.enable_grad()`` and
    ``enoki.set_grad()``), otherwise the function simply returns zero.

    The default implementation relies on naive automatic differentiation (AD),
    which records a computation graph of the rendering step that is
    subsequently traversed to propagate derivatives. This tends to be
    relatively inefficient due to the need to track intermediate program state;
    in particular, the calculation can easily run out of memory. Integrators
    like ``rb`` and ``prb`` that are specifically designed for differentiation
    can be significantly more efficient.

    Parameter ``scene`` (``mitsuba.render.Scene``):
        The scene to be rendered differentially.

    Parameter ``params`` (``mitsuba.python.utils.SceneParameters``):
       Scene parameter data structure specifying input gradients.

    Parameter ``seed` (``int``)
        Seed value for the sampler.

    Parameter ``sensor`` (``int``, ``mitsuba.render.Sensor``):
        Optional parameter to specify which sensor to use for rendering.

    Parameter ``spp`` (``int``):
        Optional parameter to override the number of samples per pixel.
        The value specified by the scene has precedence if ``spp=0``.
    """
    with ek.scoped_set_flag(ek.JitFlag.LoopRecord, False):
        image = self.render(scene, seed, sensor, spp=spp)
        ek.enqueue(ek.ADMode.Forward, params)
        ek.traverse(mitsuba.core.Float)
        return ek.grad(image)


def render_backward_impl(self: mitsuba.render.SamplingIntegrator,
                         scene: mitsuba.render.Scene,
                         params: mitsuba.python.util.SceneParameters,
                         image_adj: mitsuba.core.TensorXf,
                         seed: int,
                         sensor: Union[int, mitsuba.render.Sensor] = 0,
                         spp: int=0) -> None:
    """
    Evaluates the reverse-mode derivative of the rendering step.

    Reverse-mode differentiation transforms image-space gradients into scene
    parameter gradients, enabling simultaneous optimization of scenes with
    millions of free parameters. The function is invoked with an input
    *gradient image* (``image_adj``) and transforms and accumulates these into
    the ``params`` scene parameter data structure.

    Before calling this function, you must first enable gradients with respect
    to some of the elements of ``params`` (via ``enoki.enable_grad()``),
    otherwise it does not do anything. Subsequently, use ``ek.grad()`` to query
    the gradients of elements of ``params``.

    The default implementation relies on naive automatic differentiation (AD),
    which records a computation graph of the rendering step that is
    subsequently traversed to propagate derivatives. This tends to be
    relatively inefficient due to the need to track intermediate program state;
    in particular, the calculation can easily run out of memory. Integrators
    like ``rb`` and ``prb`` that are specifically designed for differentiation
    can be significantly more efficient.

    Parameter ``scene`` (``mitsuba.render.Scene``):
        The scene to be rendered differentially.

    Parameter ``params`` (``mitsuba.python.utils.SceneParameters``):
       Scene parameter data structure that will receive gradients.

    Parameter ``image_adj`` (``mitsuba.core.TensorXf``):
        Gradient image that should be back-propagated.

    Parameter ``seed` (``int``)
        Seed value for the sampler.

    Parameter ``sensor`` (``int``, ``mitsuba.render.Sensor``):
        Optional parameter to specify which sensor to use for rendering.

    Parameter ``spp`` (``int``):
        Optional parameter to override the number of samples per pixel.
        The value specified by the scene has precedence if ``spp=0``.
    """
    with ek.scoped_set_flag(ek.JitFlag.LoopRecord, False):
        image = self.render(scene, seed, sensor, spp=spp)
        ek.set_grad(image, image_adj)
        ek.enqueue(ek.ADMode.Backward, image)
        ek.traverse(mitsuba.core.Float)

# Monkey-patch render_forward/backward into the Integrator base class
mitsuba.render.Integrator.render_backward = render_backward_impl
mitsuba.render.Integrator.render_forward = render_forward_impl

del render_backward_impl
del render_forward_impl

# -------------------------------------------------------------
#                  Rendering Custom Operation
# -------------------------------------------------------------

class _RenderOp(ek.CustomOp):
    """
    Differentiable rendering operation, implementation detail of mitsuba.python.ad.render
    """
    def eval(self, scene, integrator, params, sensor, seed, spp):
        self.args = kwargs
        self.scene = scene
        self.integrator = integrator
        self.params = params
        self.sensor = sensor
        self.seed = (seed, seed_diff)
        self.spp = (spp, spp_diff)
        with ek.suspend_grad():
            image = self.integrator.render(self.scene, seed[0], sensor, spp=spp)
        return image

    def forward(self):
        g = self.integrator.render_forward(self.scene, self.params,
                                           self.seed[1], sensor, self.spp[1])
        self.set_grad_out(g)

    def backward(self):
        grad_out = self.grad_out()
        self.integrator.render_backward(self.scene, self.params, grad_out,
                                        self.seed[1], sensor, self.spp[1])

    def name(self):
        return "RenderOp"


def render(scene: mitsuba.render.Scene,
           integrator: mitsuba.render.Integrator,
           params: mitsuba.python.util.SceneParameters,
           seed: int,
           sensor: Union[int, mitsuba.render.Sensor] = 0,
           spp: int = 0,
           seed_diff: int = 0,
           spp_diff: int = 0) -> mitsuba.core.TensorXf:
    """
    This function provides a convenient high-level interface to differentiable
    rendering algorithms in Enoki. Invoking the function returns an ordinary
    rendering to be used in further computation that can subsequently be
    differentiated in either forward or reverse mode (i.e., using
    ``enoki.forward()`` and ``enoki.backward()``).

    Under the hood, the differentiation operation will be intercepted and
    routed to ``SamplingIntegrator.render_forward()`` or
    ``SamplingIntegrator.render_backward()`` which realize suitable
    differential simulations.

    Note the default implementation relies on naive automatic differentiation
    (AD), which records a computation graph of the rendering step that is
    subsequently traversed to propagate derivatives. This tends to be
    relatively inefficient due to the need to track intermediate program state;
    in particular, the calculation can easily run out of memory. Integrators
    like ``rb`` and ``prb`` that are specifically designed for differentiation
    can be significantly more efficient.

    Parameter ``scene`` (``mitsuba.render.Scene``):
        The scene to be rendered differentially.

    Parameter ``integrator`` (``mitsuba.render.Integrator``):
        The integrator object implementing the light transport algorithm

    Parameter ``params`` (``mitsuba.python.utils.SceneParameters``):
       Scene parameter data structure.

    Parameter ``seed` (``int``)
        Seed value for the sampler.

    Parameter ``sensor`` (``int``, ``mitsuba.render.Sensor``):
        Optional parameter to specify which sensor to use for rendering.

    Parameter ``spp`` (``int``):
        Optional parameter to override the number of samples per pixel for the
        primal phase. The value specified by the scene has precedence if
        ``spp=0``.

    Parameter ``seed_diff` (``int``)
        Optional seed value for the differential phase. By default, the seed
        will be derived from the primal `seed` value.

    Parameter ``spp_diff`` (``int``):
        Optional parameter to override the number of samples per pixel used
        during the differential phase. The default is to use the same number
        of samples as the primal phase.
    """

    assert isinstance(scene, mitsuba.render.Scene)
    assert isinstance(integrator, mitsuba.render.Integrator)
    assert isinstance(params, mitsuba.python.util.SceneParameters)

    if spp and spp_diff == 0:
        spp_diff = spp

    if seed_diff == 0:
        seed_diff = mitsuba.core.sample_tea_32(seed, 1)[0]

    return ek.custom(_RenderOp, scene, integrator, params,
                     sensor, seed, seed_diff, spp, spp_diff)

# -------------------------------------------------------------
#  Helper functions used by various differentiable integrators
# -------------------------------------------------------------

def prepare_sampler(sensor: mitsuba.render.Sensor,
                    seed: int=0,
                    spp: int=0):
    """
    Given a sensor and a desired number of samples per pixel, this function
    computes the necessary number of Monte Carlo samples and then suitably
    seeds the sampler underlying the sensor.

    Returns the final number of samples per pixel (which may differ from the
    requested amount)
    """
    film = sensor.film()
    sampler = sensor.sampler()
    if spp != 0:
        sampler.set_sample_count(spp)
    spp = sampler.sample_count()

    film_size = film.crop_size()
    if film.has_high_quality_edges():
        film_size += 2 * film.reconstruction_filter().border_size()
    sampler.seed(seed, ek.hprod(film_size) * spp)
    return spp


def sample_sensor_rays(sensor: mitsuba.render.Sensor):
    """
    Sample a 2D grid of primary rays for a given sensor

    Returns a tuple containing

    1. a set of rays of type ``mitsuba.render.RayDifferential3f``.
    2. sampling weights of type ``mitsuba.core.Spectrum`.
    3. the continuous 2D sample positions of type ``Vector2f``
    4. the pixel index
    5. the aperture sample (if applicable)
    """
    from mitsuba.core import Float, UInt32, Vector2f, ScalarVector2f, Vector2u

    film = sensor.film()
    sampler = sensor.sampler()
    film_size = film.crop_size()
    border_size = film.reconstruction_filter().border_size()

    if film.has_high_quality_edges():
        film_size += 2 * border_size

    spp = sampler.sample_count()
    total_sample_count = ek.hprod(film_size) * spp

    assert sampler.wavefront_size() == total_sample_count

    # Compute discrete sample position
    idx = ek.arange(UInt32, total_sample_count)

    if spp != 1:
        idx //= ek.opaque(UInt32, spp)

    pos = Vector2u()
    pos.y = idx // film_size[0]
    pos.x = idx - pos.y * film_size[0]

    if film.has_high_quality_edges():
        pos -= border_size

    pos += film.crop_offset()

    # Cast to floating point and add random offset
    pos_f = Vector2f(pos) + sampler.next_2d()

    # Re-scaled to [0, 1]^2
    scale = ek.rcp(ScalarVector2f(film.crop_size()))
    offset = -ScalarVector2f(film.crop_offset()) * scale
    pos_adjusted = ek.fmadd(pos_f, scale, offset)

    aperture_sample = Vector2f(0.0)
    if sensor.needs_aperture_sample():
        aperture_sample = sampler.next_2d()

    rays, weights = sensor.sample_ray_differential(
        time=0.0,
        sample1=sampler.next_1d(),
        sample2=pos_adjusted,
        sample3=aperture_sample
    )

    return rays, weights, pos_f, idx, aperture_sample


def mis_weight(pdf_a, pdf_b):
    """
    Compute the Multiple Importance Sampling (MIS) weight given the densities
    of two sampling strategies according to the power heuristic.
    """
    a2, b2 = ek.sqr(pdf_a), ek.sqr(pdf_b)
    return ek.detach(ek.select(pdf_a > 0, a2 / (a2 + b2), 0), True)
