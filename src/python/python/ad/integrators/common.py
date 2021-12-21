from __future__ import annotations # Delayed parsing of type annotations

import mitsuba
import enoki as ek
import gc

# -------------------------------------------------------------------
# Default implementation of Integrator.render_forward/backward
# -------------------------------------------------------------------

def render_forward_impl(self: mitsuba.render.Integrator,
                        scene: mitsuba.render.Scene,
                        params: mitsuba.python.util.SceneParameters,
                        sensor: mitsuba.render.Sensor,
                        seed: int = 0,
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
    like ``rb`` (Radiative Backpropagation) and ``prb`` (Path Replay
    Backpropagation) that are specifically designed for differentiation can be
    significantly more efficient.

    Parameter ``scene`` (``mitsuba.render.Scene``):
        The scene to be rendered differentially.

    Parameter ``params`` (``mitsuba.python.utils.SceneParameters``):
       Scene parameter data structure. Note that gradient tracking must be
       explicitly enabled for each desired parameter using
       ``enoki.enable_grad(params['parameter_name'])``. Furthermore,
       ``enoki.set_grad(...)`` must be used to associate specific gradient
       values with parameters in forward mode differentiation.

    Parameter ``sensor`` (``int``, ``mitsuba.render.Sensor``):
        Specify a sensor or a (sensor index) to render the scene from a
        different viewpoint. By default, the first sensor within the scene
        description (index 0) will take precedence.

    Parameter ``seed` (``int``)
        This parameter controls the initialization of the random number
        generator. It is crucial that you specify different seeds (e.g., an
        increasing sequence) if subsequent calls should produce statistically
        independent images (e.g. to de-correlate gradient-based optimization
        steps).

    Parameter ``spp`` (``int``):
        Optional parameter to override the number of samples per pixel for the
        differential rendering step. The value provided within the original
        scene specification takes precedence if ``spp=0``.
    """

    # Recorded loops cannot be differentiated, so let's disable them
    with ek.scoped_set_flag(ek.JitFlag.LoopRecord, False):
        image = self.render(
            scene=scene,
            sensor=sensor,
            seed=seed,
            spp=spp,
            develop=True,
            evaluate=False
        )

        # Process the computation graph using forward-mode AD
        ek.enqueue(ek.ADMode.Forward, params)
        ek.traverse(mitsuba.core.Float)

        return ek.grad(image)


def render_backward_impl(self: mitsuba.render.Integrator,
                         scene: mitsuba.render.Scene,
                         params: mitsuba.python.util.SceneParameters,
                         grad_in: mitsuba.core.TensorXf,
                         sensor: mitsuba.render.Sensor,
                         seed: int = 0,
                         spp: int = 0) -> None:
    """
    Evaluates the reverse-mode derivative of the rendering step.

    Reverse-mode differentiation transforms image-space gradients into scene
    parameter gradients, enabling simultaneous optimization of scenes with
    millions of free parameters. The function is invoked with an input
    *gradient image* (``grad_in``) and transforms and accumulates these into
    the ``params`` scene parameter data structure.

    Before calling this function, you must first enable gradients with respect
    to some of the elements of ``params`` (via ``enoki.enable_grad()``), or it
    will not do anything. Following the call to ``render_backward()``, use
    ``ek.grad()`` to query the gradients of elements of ``params``.

    The default implementation relies on naive automatic differentiation (AD),
    which records a computation graph of the rendering step that is
    subsequently traversed to propagate derivatives. This tends to be
    relatively inefficient due to the need to track intermediate program state;
    in particular, the calculation can easily run out of memory. Integrators
    like ``rb`` (Radiative Backpropagation) and ``prb`` (Path Replay
    Backpropagation) that are specifically designed for differentiation can be
    significantly more efficient.

    Parameter ``scene`` (``mitsuba.render.Scene``):
        The scene to be rendered differentially.

    Parameter ``params`` (``mitsuba.python.utils.SceneParameters``):
       Scene parameter data structure. Note that gradient tracking must be
       explicitly enabled for each desired parameter using
       ``enoki.enable_grad(params['parameter_name'])``.

    Parameter ``grad_in`` (``mitsuba.core.TensorXf``):
        Gradient image that should be back-propagated.

    Parameter ``sensor`` (``int``, ``mitsuba.render.Sensor``):
        Specify a sensor or a (sensor index) to render the scene from a
        different viewpoint. By default, the first sensor within the scene
        description (index 0) will take precedence.

    Parameter ``seed` (``int``)
        This parameter controls the initialization of the random number
        generator. It is crucial that you specify different seeds (e.g., an
        increasing sequence) if subsequent calls should produce statistically
        independent images (e.g. to de-correlate gradient-based optimization
        steps).

    Parameter ``spp`` (``int``):
        Optional parameter to override the number of samples per pixel for the
        differential rendering step. The value provided within the original
        scene specification takes precedence if ``spp=0``.
    """

    # Recorded loops cannot be differentiated, so let's disable them
    with ek.scoped_set_flag(ek.JitFlag.LoopRecord, False):
        image = self.render(
            scene=scene,
            sensor=sensor,
            seed=seed,
            spp=spp,
            develop=True,
            evaluate=False
        )

        # Process the computation graph using reverse-mode AD
        ek.set_grad(image, grad_in)
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
    """ Differentiable rendering CustomOp, used in render() below """

    def eval(self, scene, params, sensor, integrator, seed, spp):
        self.scene = scene
        self.integrator = integrator
        self.params = params
        self.sensor = sensor
        self.seed = seed
        self.spp = spp

        with ek.suspend_grad():
            return self.integrator.render(
                scene=self.scene,
                sensor=sensor,
                seed=seed[0],
                spp=spp[0],
                develop=True,
                evaluate=False
            )

    def forward(self):
        self.set_grad_out(
            self.integrator.render_forward(self.scene, self.params, self.sensor,
                                           self.seed[1], self.spp[1]))

    def backward(self):
        self.integrator.render_backward(self.scene, self.params, self.grad_out(),
                                        self.sensor, self.seed[1], self.spp[1])

    def name(self):
        return "RenderOp"


def render(scene: mitsuba.render.Scene,
           params: mitsuba.python.util.SceneParameters,
           sensor: Union[int, mitsuba.render.Sensor] = 0,
           integrator: mitsuba.render.Integrator = None,
           seed: int = 0,
           seed_diff: int = 0,
           spp: int = 0,
           spp_diff: int = 0) -> mitsuba.core.TensorXf:
    """
    This function provides a convenient high-level interface to differentiable
    rendering algorithms in Enoki. The function returns a rendered image that
    can be used in subsequent differentiable computation steps. At any later
    point, the entire computation graph can be differentiated end-to-end in
    either forward or reverse mode (i.e., using ``enoki.forward()`` and
    ``enoki.backward()``).

    Under the hood, the differentiation operation will be intercepted and
    routed to ``Integrator.render_forward()`` or
    ``Integrator.render_backward()``, which evaluate the derivative using
    either naive AD or a more specialized differential simulation.

    Note the default implementation relies on naive automatic differentiation
    (AD), which records a computation graph of the primal rendering step that
    is subsequently traversed to propagate derivatives. This tends to be
    relatively inefficient due to the need to track intermediate program state;
    in particular, the calculation can easily run out of memory. Integrators
    like ``rb`` (Radiative Backpropagation) and ``prb`` (Path Replay
    Backpropagation) that are specifically designed for differentiation can be
    significantly more efficient.

    Parameter ``scene`` (``mitsuba.render.Scene``):
        The scene to be rendered in a differentiable manner.

    Parameter ``params`` (``mitsuba.python.utils.SceneParameters``):
       Scene parameter data structure. Note that gradient tracking must be
       explicitly enabled for each desired parameter using
       ``enoki.enable_grad(params['parameter_name'])``. Furthermore,
       ``enoki.set_grad(...)`` must be used to associate specific gradient
       values with parameters if forward mode derivatives are desired.

    Parameter ``sensor`` (``int``, ``mitsuba.render.Sensor``):
        Specify a sensor or a (sensor index) to render the scene from a
        different viewpoint. By default, the first sensor within the scene
        description (index 0) will take precedence.

    Parameter ``integrator`` (``mitsuba.render.Integrator``):
        Optional parameter to override the rendering technique to be used. By
        default, the integrator specified in the original scene description
        will be used.

    Parameter ``seed` (``int``)
        This parameter controls the initialization of the random number
        generator during the primal rendering step. It is crucial that you
        specify different seeds (e.g., an increasing sequence) if subsequent
        calls should produce statistically independent images (e.g. to
        de-correlate gradient-based optimization steps).

    Parameter ``seed_diff` (``int``)
        This parameter is analogous to the ``seed`` parameter but targets the
        differential simulation phase. If not specified, the implementation
        will automatically compute a suitable value from the primal ``seed``.

    Parameter ``spp`` (``int``):
        Optional parameter to override the number of samples per pixel for the
        primal rendering step. The value provided within the original scene
        specification takes precedence if ``spp=0``.

    Parameter ``spp_diff`` (``int``):
        This parameter is analogous to the ``seed`` parameter but targets the
        differential simulation phase. If not specified, the implementation
        will copy the value from ``spp``.
    """

    assert isinstance(scene, mitsuba.render.Scene)
    assert isinstance(params, mitsuba.python.util.SceneParameters)

    if integrator is None:
        integrator = scene.integrator()


    if isinstance(sensor, int):
        sensor = scene.sensors()[sensor]

    assert isinstance(integrator, mitsuba.render.Integrator)
    assert isinstance(sensor, mitsuba.render.Sensor)

    if spp_diff == 0:
        spp_diff = spp

    if seed_diff == 0:
        seed_diff = mitsuba.core.sample_tea_32(seed, 1)[0]

    return ek.custom(_RenderOp, scene, params, sensor,
                     integrator, (seed, seed_diff), (spp, spp_diff))


# -------------------------------------------------------------
#  Helper functions used by various differentiable integrators
# -------------------------------------------------------------

def prepare(sensor: mitsuba.render.Sensor,
            seed: int = 0,
            spp: int = 0,
            aovs: list = []):
    """
    Given a sensor and a desired number of samples per pixel, this function
    computes the necessary number of Monte Carlo samples and then suitably
    seeds the sampler underlying the sensor.

    Returns the final number of samples per pixel (which may differ from the
    requested amount)

    Parameter ``sensor`` (``mitsuba.render.Sensor``):
        Specify a sensor or a (sensor index) to render the scene from a
        different viewpoint. By default, the first sensor within the scene
        description (index 0) will take precedence.

    Parameter ``seed` (``int``)
        This parameter controls the initialization of the random number
        generator during the primal rendering step. It is crucial that you
        specify different seeds (e.g., an increasing sequence) if subsequent
        calls should produce statistically independent images (e.g. to
        de-correlate gradient-based optimization steps).

    Parameter ``spp`` (``int``):
        Optional parameter to override the number of samples per pixel for the
        primal rendering step. The value provided within the original scene
        specification takes precedence if ``spp=0``.
    """

    film = sensor.film()
    sampler = sensor.sampler().clone()

    if spp != 0:
        sampler.set_sample_count(spp)

    spp = sampler.sample_count()
    sampler.set_samples_per_wavefront(spp)

    film_size = film.crop_size()

    if film.sample_border():
        film_size += 2 * film.rfilter().border_size()

    wavefront_size = ek.hprod(film_size) * spp

    is_llvm = ek.is_llvm_array_v(mitsuba.core.Float)
    wavefront_size_limit = 0xffffffff if is_llvm else 0x40000000

    if wavefront_size >  wavefront_size_limit:
        raise Exception(
            "Tried to perform a %s-based rendering with a total sample "
            "count of %u, which exceeds 2^%u = %u (the upper limit "
            "for this backend). Please use fewer samples per pixel or "
            "render using multiple passes." %
            ("LLVM JIT" if is_llvm else "OptiX", wavefront_size,
             ek.log2i(wavefront_size_limit) + 1, wavefront_size_limit))

    sampler.seed(seed, wavefront_size)
    film.prepare(aovs)

    return sampler, spp


def sample_rays(sensor: mitsuba.render.Sensor,
                sampler: mitsuba.render.Sampler,
                reparameterize = False):
    """
    Sample a 2D grid of primary rays for a given sensor

    Returns a tuple containing

    1. a set of rays of type ``mitsuba.render.RayDifferential3f``.
    2. sampling weights of type ``mitsuba.core.Spectrum`.
    3. the continuous 2D sample positions of type ``Vector2f``
    4. the pixel index
    5. the aperture sample (if applicable)
    """
    from mitsuba.core import Float, UInt32, Vector2f, \
        ScalarVector2f, Vector2u, Ray3f

    film = sensor.film()
    film_size = film.crop_size()
    rfilter = film.rfilter()
    border_size = rfilter.border_size()

    if film.sample_border():
        film_size += 2 * border_size

    spp = sampler.sample_count()

    if reparameterize:
        if rfilter.is_box_filter():
            raise Exception("Please use a different reconstruction filter, the "
                            "box filter cannot be used with this integrator!")

        if not film.sample_border():
            raise Exception("This integrator requires that the film's "
                            "'sample_border' parameter is set to true.")

    # Compute discrete sample position
    idx = ek.arange(UInt32, ek.hprod(film_size) * spp)

    # Try to avoid a division by an unknown constant if we can help it
    log_spp = ek.log2i(spp)
    if 1 << log_spp == spp:
        idx >>= ek.opaque(UInt32, log_spp)
    else:
        idx /= ek.opaque(UInt32, spp)

    # Compute the position on the image plane
    pos = Vector2u()
    pos.y = idx // film_size[0]
    pos.x = ek.fnmadd(film_size[0], pos.y, idx)

    if film.sample_border():
        pos -= border_size

    pos += film.crop_offset()

    # Cast to floating point and add random offset
    pos_f = Vector2f(pos) + sampler.next_2d()

    # Re-scale the position to [0, 1]^2
    scale = ek.rcp(ScalarVector2f(film.crop_size()))
    offset = -ScalarVector2f(film.crop_offset()) * scale
    pos_adjusted = ek.fmadd(pos_f, scale, offset)

    aperture_sample = Vector2f(0.0)
    if sensor.needs_aperture_sample():
        aperture_sample = sampler.next_2d()

    time = sensor.shutter_open()
    if sensor.shutter_open_time() > 0:
        time += sampler.next_1d() * sensor.shutter_open_time()

    wavelength_sample = 0
    if mitsuba.core.is_spectral:
        wavelength_sample = sampler.next_1d()

    rays, weights = sensor.sample_ray(
        time=wavelength_sample,
        sample1=sampler.next_1d(),
        sample2=pos_adjusted,
        sample3=aperture_sample
    )

    return rays, weights, pos_f, aperture_sample


def mis_weight(pdf_a, pdf_b):
    """
    Compute the Multiple Importance Sampling (MIS) weight given the densities
    of two sampling strategies according to the power heuristic.
    """
    a2 = ek.sqr(pdf_a)
    return ek.detach(ek.select(pdf_a > 0, a2 / ek.fmadd(pdf_b, pdf_b, a2), 0), True)


def develop_block(block: mitsuba.render.ImageBlock):
    """
    Perform the weight division in an ImageBlock, returns a tensor with one
    channel less than before.
    """

    from mitsuba.core import TensorXf, Float, UInt32, Bool

    channels_in  = block.channel_count()
    channels_out = channels_in - 1
    tensor       = block.tensor()
    size         = tensor.shape[0:2]

    # Compute indices to access specific components of 'values_in'
    idx         = ek.arange(UInt32, ek.hprod(size) * channels_out)
    pixel_idx   = idx // channels_out
    channel_idx = ek.fnmadd(channels_out, pixel_idx, idx)
    values_idx  = ek.fmadd(pixel_idx, channels_in, channel_idx)
    weight_idx  = ek.fmadd(pixel_idx, channels_in, channels_out)

    # Gather, and perform the weight division
    weight_in = ek.gather(Float, tensor.array, weight_idx)
    values_out = ek.gather(Float, tensor.array, values_idx)
    values_out /= ek.select(ek.eq(weight_in, 0), 1, weight_in)

    return TensorXf(values_out, (size[1], size[0], channels_out))


class ADIntegrator(mitsuba.render.SamplingIntegrator):
    def __init__(self, props = mitsuba.core.Properties()):
        super().__init__(props)

        self.max_depth = props.get('max_depth', 4)
        self.split_adjoint = props.get('split_adjoint', False)
        assert self.max_depth > 0


    def aovs(self):
        return []

    def render(self: mitsuba.render.SamplingIntegrator,
               scene: mitsuba.render.Scene,
               sensor: mitsuba.render.Sensor,
               seed: int = 0,
               spp: int = 0,
               develop: bool = True,
               evaluate: bool = True) -> mitsuba.core.TensorXf:

        from mitsuba.core import Bool, UInt32, Float

        if not develop:
            raise Exception("develop=True must be specified when "
                            "invoking AD integrators")

        # Disable derivatives in all of the following
        with ek.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = prepare(
                sensor=sensor,
                seed=seed,
                spp=spp,
                aovs=self.aovs()
            )

            # Generate many rays from the camera
            ray, weight, pos, ap_sample = sample_rays(sensor, sampler)

            # Launch the Monte Carlo sampling process in primal mode
            spec, valid, state = self.sample_impl(
                mode=ek.ADMode.Primal,
                scene=scene,
                sampler=sampler,
                ray=ray,
                depth=UInt32(0),
                δL=None,
                state_in=None,
                active=Bool(True)
            )

            # Prepare an ImageBlock as specified by the film
            block = sensor.film().create_block()

            # Only use the coalescing feature when rendering enough samples
            block.set_coalesce(block.coalesce() and spp >= 4)

            # Acumulate the image block
            alpha = ek.select(valid, Float(1), Float(0))
            block.put(pos, ray.wavelengths, spec * weight, alpha)

            # Explicitly delete any remaining unused variables
            del sampler, ray, weight, pos, ap_sample, spec, valid, \
                state, alpha

            # Probably a little overkill, but why not.. If there are any
            # Enoki arrays to be collected by Python's cyclic GC, then
            # freeing them may enable loop simplifications in ek.eval().
            gc.collect()

            # Perform the weight division and return an image tensor
            return develop_block(block)


    def render_backward(self: mitsuba.render.SamplingIntegrator,
                        scene: mitsuba.render.Scene,
                        params: mitsuba.python.util.SceneParameters,
                        grad_in: mitsuba.core.TensorXf,
                        sensor: mitsuba.render.Sensor,
                        seed: int = 0,
                        spp: int = 0) -> None:
        """
        Evaluates the reverse-mode derivative of the rendering step.

        Reverse-mode differentiation transforms image-space gradients into scene
        parameter gradients, enabling simultaneous optimization of scenes with
        millions of free parameters. The function is invoked with an input
        *gradient image* (``grad_in``) and transforms and accumulates these into
        the ``params`` scene parameter data structure.

        Before calling this function, you must first enable gradients with respect
        to some of the elements of ``params`` (via ``enoki.enable_grad()``), or it
        will not do anything. Following the call to ``render_backward()``, use
        ``ek.grad()`` to query the gradients of elements of ``params``.

        Parameter ``scene`` (``mitsuba.render.Scene``):
            The scene to be rendered differentially.

        Parameter ``params`` (``mitsuba.python.utils.SceneParameters``):
           Scene parameter data structure. Note that gradient tracking must be
           explicitly enabled for each desired parameter using
           ``enoki.enable_grad(params['parameter_name'])``.

        Parameter ``grad_in`` (``mitsuba.core.TensorXf``):
            Gradient image that should be back-propagated.

        Parameter ``sensor`` (``int``, ``mitsuba.render.Sensor``):
            Specify a sensor or a (sensor index) to render the scene from a
            different viewpoint. By default, the first sensor within the scene
            description (index 0) will take precedence.

        Parameter ``seed` (``int``)
            This parameter controls the initialization of the random number
            generator. It is crucial that you specify different seeds (e.g., an
            increasing sequence) if subsequent calls should produce statistically
            independent images (e.g. to de-correlate gradient-based optimization
            steps).

        Parameter ``spp`` (``int``):
            Optional parameter to override the number of samples per pixel for the
            differential rendering step. The value provided within the original
            scene specification takes precedence if ``spp=0``.
        """

        from mitsuba.core import Bool, UInt32
        from mitsuba.render import ImageBlock

        film = sensor.film()

        # self.sample_impl() will re-enable gradients as needed, disable them here
        with ek.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = prepare(sensor, seed, spp)

            # Generate many rays from the camera
            ray, weight, pos, ap_sample = sample_rays(sensor, sampler)

            # Launch the Monte Carlo sampling process in primal mode (1)
            spec, valid, state_out = self.sample_impl(
                mode=ek.ADMode.Primal,
                scene=scene,
                sampler=sampler.clone(),
                ray=ray,
                depth=UInt32(0),
                δL=None,
                state_in=None,
                active=Bool(True)
            )

            # Garbage collect unused values to simplify kernel about to be run
            del spec, valid, ap_sample

            if self.split_adjoint:
                # Probably a little overkill, but why not.. If there are any
                # Enoki arrays to be collected by Python's cyclic GC, then
                # freeing them may enable loop simplifications in ek.eval().
                gc.collect()
                ek.eval(state_out)

            # Wrap the input gradient image into an ImageBlock
            block = ImageBlock(offset=film.crop_offset(),
                               tensor=grad_in,
                               rfilter=film.rfilter(),
                               normalize=True,
                               border=False) # May need to revisit..

            # Use the reconstruction filter to interpolate the values at 'pos'
            δL = block.read(pos) * (weight * ek.rcp(spp))

            # Launch Monte Carlo sampling in backward AD mode (2)
            spec_2, valid_2, state_out_2 = self.sample_impl(
                mode=ek.ADMode.Backward,
                scene=scene,
                sampler=sampler,
                ray=ray,
                depth=UInt32(0),
                δL=δL,
                state_in=state_out,
                active=Bool(True)
            )

            # We don't need any of the outputs here
            del spec_2, valid_2, state_out, state_out_2, δL, \
                ray, weight, pos, block, sampler

            # Probably a little overkill, but why not.. If there are any
            # Enoki arrays to be collected by Python's cyclic GC, then
            # freeing them may enable loop simplifications in ek.eval().
            gc.collect()

            # Run kernel representing side effects of the above
            ek.eval()
