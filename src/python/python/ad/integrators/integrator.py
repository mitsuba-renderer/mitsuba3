import enoki as ek
import mitsuba
import mitsuba.python.util

# -------------------------------------------------------------------
#        Default implementation for Integrator adjoint methods
# -------------------------------------------------------------------

def render_backward_impl(self: mitsuba.render.SamplingIntegrator,
                        scene: mitsuba.render.Scene,
                        params: mitsuba.python.util.SceneParameters,
                        image_adj: mitsuba.core.Spectrum,
                        sensor_index: int=0,
                        spp: int=0) -> None:
    """
    Performed the adjoint rendering integration, backpropagating the
    image gradients to the scene parameters.

    The default implementation relies on Automatic Differentiation to
    compute the scene parameters gradients w.r.t. the adjoint image.

    Parameter ``scene``:
        The scene to render

    Parameter ``params`` (``mitsuba.python.utils.SceneParameters``):
        Scene parameters expecting gradients.

    Parameter ``image_adj`` (``mitsuba.core.ImageBuffer``):
        Gradient image to backpropagate during the adjoint integration.

    Parameter ``sensor_index`` (``int``):
        Optional parameter to specify which sensor to use for rendering.

    Parameter ``spp`` (``int``):
        Optional parameter to override the number of samples per pixel.
        This parameter will be ignored if set to 0.
    """
    ek.enable_grad(params)
    prev_flag = ek.flag(ek.JitFlag.LoopRecord)
    ek.set_flag(ek.JitFlag.LoopRecord, False)
    image = self.render(scene, sensor_index, spp=spp)
    ek.set_grad(image, image_adj)
    ek.enqueue(image)
    ek.traverse(mitsuba.core.Float, reverse=True, retain_graph=False)
    ek.set_flag(ek.JitFlag.LoopRecord, prev_flag)


def sample_adjoint_impl(self: mitsuba.render.SamplingIntegrator,
                        scene: mitsuba.render.Scene,
                        sampler: mitsuba.render.Sampler,
                        ray: mitsuba.core.Ray3f,
                        params: mitsuba.python.util.SceneParameters,
                        grad: mitsuba.core.Spectrum,
                        medium: mitsuba.render.Medium=None,
                        active: mitsuba.core.Mask=True) -> None:
    """
    Propagate adjoint radiance along a ray.

    Parameter ``scene``:
        The underlying scene in which the adjoint radiance should be propagated

    Parameter ``sampler``:
        A source of (pseudo-/quasi-) random numbers

    Parameter ``ray`` (``mitsuba.core.Ray3f``):
        Rays along which the adjoint radiance should be propagated

    Parameter ``params`` (``mitsuba.python.utils.SceneParameters``):
        Scene parameters expecting gradients

    Parameter ``grad`` (``mitsuba.core.Spectrum``):
        Gradient value to be backpropagated

    Parameter ``medium`` (``mitsuba.render.MediumPtr``):
         If the ray starts inside a medium, this argument holds a pointer
         to that medium
    """

    raise NotImplementedError(
        "SamplingIntegrator.sample_adjoint is not implemented!")


# Bind adjoint methods to the SamplingIntegrator class
mitsuba.render.SamplingIntegrator.render_backward = render_backward_impl
mitsuba.render.SamplingIntegrator.sample_adjoint = sample_adjoint_impl


# -------------------------------------------------------------
#        Helper functions for integrators implementation
# -------------------------------------------------------------


def sample_sensor_rays(sensor):
    """Sample a 2D grid of primary rays for a given sensor"""
    from mitsuba.core import Float, UInt32, Vector2f

    film = sensor.film()
    sampler = sensor.sampler()
    film_size = film.crop_size()
    spp = sampler.sample_count()

    total_sample_count = ek.hprod(film_size) * spp

    assert sampler.wavefront_size() == total_sample_count

    pos_idx = ek.arange(UInt32, total_sample_count)
    pos_idx //= ek.opaque(UInt32, spp)
    scale = ek.rcp(Vector2f(film_size))
    pos = Vector2f(Float(pos_idx %  int(film_size[0])),
                   Float(pos_idx // int(film_size[0])))

    pos += sampler.next_2d()

    rays, weights = sensor.sample_ray_differential(
        time=0.0,
        sample1=sampler.next_1d(),
        sample2=pos * scale,
        sample3=0.0
    )

    return rays, weights, pos, pos_idx


def mis_weight(a, b):
    """Compute Multiple Importance Sampling weight"""
    return ek.detach(ek.select(a > 0.0, (a**2) / (a**2 + b**2), 0.0))
