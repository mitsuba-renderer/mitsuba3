# -------------------------------------------------------------------
#        Default implementation for Integrator adjoint methods
# -------------------------------------------------------------------

import enoki as ek
import mitsuba
import mitsuba.python.util


def render_adjoint_impl(self: mitsuba.render.SamplingIntegrator,
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
    image = self.render(scene, sensor_index, spp=spp)
    ek.set_grad(image, image_adj)
    ek.enqueue(image)
    ek.traverse(mitsuba.core.Float, reverse=True, retain_graph=False)


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
mitsuba.render.SamplingIntegrator.render_adjoint = render_adjoint_impl
mitsuba.render.SamplingIntegrator.sample_adjoint = sample_adjoint_impl


# -------------------------------------------------------------------
#        Import all integrators implemented in this folder
# -------------------------------------------------------------------


from os.path import dirname, basename, isfile, join
import glob
modules = glob.glob(join(dirname(__file__), "*.py"))
__all__ = [ basename(f)[:-3] for f in modules if isfile(f) and not f.endswith('__init__.py')]
