import enoki as ek
import mitsuba
import mitsuba.python.util

# -------------------------------------------------------------------
#        Default implementation for Integrator adjoint methods
# -------------------------------------------------------------------

def render_forward_impl(self: mitsuba.render.SamplingIntegrator,
                        scene: mitsuba.render.Scene,
                        params: mitsuba.python.util.SceneParameters,
                        seed: int,
                        sensor_index: int=0,
                        spp: int=0) -> None:
    """
    Performs the adjoint phase of differentiable rendering by backpropagating
    image gradients from the scene parameters to the film.

    This method assumes params already contains some gradient.

    The default implementation provided by this function relies on automatic
    differentiation, which tends to be relatively inefficient due to the need
    to track intermediate state of the program execution. More efficient
    implementations are provided by special adjoint integrators like ``rb`` and
    ``prb``.

    Parameter ``scene``:
        The scene to render

    Parameter ``params`` (``mitsuba.python.utils.SceneParameters``):
       SceneParameters data structure that will receive parameter gradients.

    Parameter ``seed` (``int``)
        Seed value for the sampler.

    Parameter ``sensor_index`` (``int``):
        Optional parameter to specify which sensor to use for rendering.

    Parameter ``spp`` (``int``):
        Optional parameter to override the number of samples per pixel.
        This parameter will be ignored if set to 0.
    """
    ek.enable_grad(params)
    prev_flag = ek.flag(ek.JitFlag.LoopRecord)
    ek.set_flag(ek.JitFlag.LoopRecord, False)
    image = self.render(scene, seed, sensor_index, spp=spp)
    ek.enqueue(ek.ADMode.Forward, params)
    ek.traverse(mitsuba.core.Float)
    ek.set_flag(ek.JitFlag.LoopRecord, prev_flag)
    return ek.grad(image)


def render_backward_impl(self: mitsuba.render.SamplingIntegrator,
                         scene: mitsuba.render.Scene,
                         params: mitsuba.python.util.SceneParameters,
                         image_adj: mitsuba.core.TensorXf,
                         seed: int,
                         sensor_index: int=0,
                         spp: int=0) -> None:
    """
    Performs the adjoint phase of differentiable rendering by backpropagating
    image gradients back to the scene parameters.

    The default implementation provided by this function relies on automatic
    differentiation, which tends to be relatively inefficient due to the need
    to track intermediate state of the program execution. More efficient
    implementations are provided by special adjoint integrators like ``rb`` and
    ``prb``.

    Parameter ``scene``:
        The scene to render

    Parameter ``params`` (``mitsuba.python.utils.SceneParameters``):
       SceneParameters data structure that will receive parameter gradients.

    Parameter ``image_adj`` (``mitsuba.core.TensorXf``):
        Gradient image that should be backpropagated.

    Parameter ``seed` (``int``)
        Seed value for the sampler.

    Parameter ``sensor_index`` (``int``):
        Optional parameter to specify which sensor to use for rendering.

    Parameter ``spp`` (``int``):
        Optional parameter to override the number of samples per pixel.
        This parameter will be ignored if set to 0.
    """
    ek.enable_grad(params)
    prev_flag = ek.flag(ek.JitFlag.LoopRecord)
    ek.set_flag(ek.JitFlag.LoopRecord, False)
    image = self.render(scene, seed, sensor_index, spp=spp)
    ek.set_grad(image, image_adj)
    ek.enqueue(ek.ADMode.Backward, image)
    ek.traverse(mitsuba.core.Float)
    ek.set_flag(ek.JitFlag.LoopRecord, prev_flag)


# Bind adjoint methods to the integrator classes
mitsuba.render.SamplingIntegrator.render_backward = render_backward_impl
mitsuba.render.SamplingIntegrator.render_forward = render_forward_impl
mitsuba.render.ScatteringIntegrator.render_backward = render_backward_impl
mitsuba.render.ScatteringIntegrator.render_forward = render_forward_impl


# -------------------------------------------------------------
#        Helper functions for integrators implementation
# -------------------------------------------------------------

def prepare_sampler(sensor, seed, spp):
    film = sensor.film()
    rfilter = film.reconstruction_filter()
    sampler = sensor.sampler()
    if spp > 0:
        sampler.set_sample_count(spp)
    spp = sampler.sample_count()

    film_size = film.crop_size()
    if film.has_high_quality_edges():
        film_size += 2 * rfilter.border_size()
    sampler.seed(seed, ek.hprod(film_size) * spp)
    return spp

def sample_sensor_rays(sensor):
    """Sample a 2D grid of primary rays for a given sensor"""
    from mitsuba.core import Float, UInt32, Vector2f

    film = sensor.film()
    sampler = sensor.sampler()
    film_size = film.crop_size()
    border_size = film.reconstruction_filter().border_size()

    if film.has_high_quality_edges():
        film_size += 2 * border_size

    spp = sampler.sample_count()
    total_sample_count = ek.hprod(film_size) * spp

    assert sampler.wavefront_size() == total_sample_count

    pos_idx = ek.arange(UInt32, total_sample_count)
    pos_idx //= ek.opaque(UInt32, spp)
    pos = Vector2f(Float(pos_idx %  int(film_size[0])),
                   Float(pos_idx // int(film_size[0])))

    if film.has_high_quality_edges():
        pos -= border_size

    pos += film.crop_offset()
    pos += sampler.next_2d()

    adjusted_pos = (pos - film.crop_offset()) / Vector2f(film.crop_size())

    aperture_samples = Vector2f(0.0)
    if sensor.needs_aperture_sample():
        aperture_samples = sampler.next_2d()

    rays, weights = sensor.sample_ray_differential(
        time=0.0,
        sample1=sampler.next_1d(),
        sample2=adjusted_pos,
        sample3=aperture_samples
    )

    return rays, weights, pos, pos_idx, aperture_samples


def mis_weight(a, b):
    """Compute Multiple Importance Sampling weight"""
    return ek.detach(ek.select(a > 0.0, (a**2) / (a**2 + b**2), 0.0))
