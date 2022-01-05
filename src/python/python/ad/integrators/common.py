from __future__ import annotations # Delayed parsing of type annotations

import mitsuba
import enoki as ek
import gc

# -------------------------------------------------------------------
# Default implementation of Integrator.render_forward/backward
# -------------------------------------------------------------------

def render_forward(self: mitsuba.render.Integrator,
                   scene: mitsuba.render.Scene,
                   sensor: Union[int, mitsuba.render.Sensor] = 0,
                   seed: int = 0,
                   spp: int = 0) -> mitsuba.core.TensorXf:
    """
    Evaluates the forward-mode derivative of the rendering step.

    Forward-mode differentiation propagates gradients from scene parameters
    through the simulation, producing a *gradient image* (i.e., the derivative
    of the rendered image with respect to those scene parameters). The gradient
    image is very helpful for debugging, for example to inspect the gradient
    variance or visualize the region of influence of a scene parameter. It is
    not particularly useful for simultaneous optimization of many parameters,
    since multiple differentiation passes are needed to obtain separate
    derivatives for each scene parameter. See ``Integrator.render_backward()``
    for an efficient way of obtaining all parameter derivatives at once, or
    simply use the ``mitsuba.python.ad.render()`` abstraction that hides both
    ``Integrator.render_forward()`` and ``Integrator.render_backward()`` behind
    a unified interface.

    Before calling this function, you must first enable gradient tracking and
    furthermore associate concrete input gradients with one or more scene
    parameters, or the function will just return a zero-valued gradient image.
    This is typically done by invoking ``ek.enable_grad()`` and
    ``ek.set_grad()`` on elements of the ``SceneParameters`` data structure
    that can be obtained obtained via a call to
    ``mitsuba.python.util.traverse()``.

    The default implementation of this function relies on naive automatic
    differentiation (AD), which records a computation graph of the rendering
    step that is subsequently traversed to propagate derivatives. This tends to
    be relatively inefficient due to the need to track intermediate program
    state; in particular, the calculation can easily run out of memory.
    Integrators like ``rb`` (Radiative Backpropagation) and ``prb`` (Path
    Replay Backpropagation) that are specifically designed for differentiation
    can be significantly more efficient.

    Parameter ``scene`` (``mitsuba.render.Scene``):
        The scene to be rendered differentially.

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

        # Perform an AD traversal of all registered AD variables that
        # influence 'image' in a differentiable manner
        ek.forward_to(image)

        return ek.grad(image)


def render_backward(self: mitsuba.render.Integrator,
                    scene: mitsuba.render.Scene,
                    grad_in: mitsuba.core.TensorXf,
                    sensor: Union[int, mitsuba.render.Sensor] = 0,
                    seed: int = 0,
                    spp: int = 0) -> None:
    """
    Evaluates the reverse-mode derivative of the rendering step.

    Reverse-mode differentiation transforms image-space gradients into scene
    parameter gradients, enabling simultaneous optimization of scenes with
    millions of free parameters. The function is invoked with an input
    *gradient image* (``grad_in``) and transforms and accumulates these into
    the gradient arrays of scene parameters that previously had gradient
    tracking enabled.

    Before calling this function, you must first enable gradient tracking for
    one or more scene parameters, or the function will not do anything. This is
    typically done by invoking ``ek.enable_grad()`` on elements of the
    ``SceneParameters`` data structure that can be obtained obtained via a call
    to ``mitsuba.python.util.traverse()``. Use ``ek.grad()`` to query the
    resulting gradients of these parameters once ``render_backward()`` returns.

    The default implementation of this function relies on naive automatic
    differentiation (AD), which records a computation graph of the rendering
    step that is subsequently traversed to propagate derivatives. This tends to
    be relatively inefficient due to the need to track intermediate program
    state; in particular, the calculation can easily run out of memory.
    Integrators like ``rb`` (Radiative Backpropagation) and ``prb`` (Path
    Replay Backpropagation) that are specifically designed for differentiation
    can be significantly more efficient.

    Parameter ``scene`` (``mitsuba.render.Scene``):
        The scene to be rendered differentially.

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
        ek.backward_from(image * grad_in)

# Monkey-patch render_forward/backward into the Integrator base class
mitsuba.render.Integrator.render_backward = render_backward
mitsuba.render.Integrator.render_forward = render_forward

del render_backward
del render_forward

# -------------------------------------------------------------
#                  Rendering Custom Operation
# -------------------------------------------------------------

class _RenderOp(ek.CustomOp):
    """ Differentiable rendering CustomOp, used in render() below """

    def eval(self, scene, sensor, integrator, seed, spp, params):
        self.scene = scene
        self.integrator = integrator
        self.sensor = sensor
        self.seed = seed
        self.spp = spp

        # 'params' is unused and just specified to correctly wire the
        # rendering step into the AD computation graph
        del params

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
            self.integrator.render_forward(self.scene, self.sensor,
                                           self.seed[1], self.spp[1]))

    def backward(self):
        self.integrator.render_backward(self.scene, self.grad_out(),
                                        self.sensor, self.seed[1], self.spp[1])

    def name(self):
        return "RenderOp"


def render(scene: mitsuba.render.Scene,
           params: Any,
           sensor: Union[int, mitsuba.render.Sensor] = 0,
           integrator: mitsuba.render.Integrator = None,
           seed: int = 0,
           seed_grad: int = 0,
           spp: int = 0,
           spp_grad: int = 0) -> mitsuba.core.TensorXf:
    """
    This function provides a convenient high-level interface to differentiable
    rendering algorithms in Enoki. The function returns a rendered image that
    can be used in subsequent differentiable computation steps. At any later
    point, the entire computation graph can be differentiated end-to-end in
    either forward or reverse mode (i.e., using ``ek.forward()`` and
    ``ek.backward()``).

    Under the hood, the differentiation operation will be intercepted and
    routed to ``Integrator.render_forward()`` or
    ``Integrator.render_backward()``, which evaluate the derivative using
    either naive AD or a more specialized differential simulation.

    Note the default implementation of this functionality relies on naive
    automatic differentiation (AD), which records a computation graph of the
    primal rendering step that is subsequently traversed to propagate
    derivatives. This tends to be relatively inefficient due to the need to
    track intermediate program state; in particular, the calculation can easily
    run out of memory. Integrators like ``rb`` (Radiative Backpropagation) and
    ``prb`` (Path Replay Backpropagation) that are specifically designed for
    differentiation can be significantly more efficient.

    Parameter ``scene`` (``mitsuba.render.Scene``):
        Reference to the scene being rendered in a differentiable manner.

    Parameter ``params`` (``mitsuba.python.utils.SceneParameters``):
       Scene parameter data structure. Note that gradient tracking must be
       explicitly enabled for each desired parameter using
       ``ek.enable_grad(params['parameter_name'])``. Furthermore,
       ``ek.set_grad(...)`` must be used to associate specific gradient
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

    Parameter ``seed_grad` (``int``)
        This parameter is analogous to the ``seed`` parameter but targets the
        differential simulation phase. If not specified, the implementation
        will automatically compute a suitable value from the primal ``seed``.

    Parameter ``spp`` (``int``):
        Optional parameter to override the number of samples per pixel for the
        primal rendering step. The value provided within the original scene
        specification takes precedence if ``spp=0``.

    Parameter ``spp_grad`` (``int``):
        This parameter is analogous to the ``seed`` parameter but targets the
        differential simulation phase. If not specified, the implementation
        will copy the value from ``spp``.
    """

    assert isinstance(scene, mitsuba.render.Scene)

    if integrator is None:
        integrator = scene.integrator()

    if isinstance(sensor, int):
        sensor = scene.sensors()[sensor]

    assert isinstance(integrator, mitsuba.render.Integrator)
    assert isinstance(sensor, mitsuba.render.Sensor)

    if spp_grad == 0:
        spp_grad = spp

    if seed_grad == 0:
        # Compute a seed that de-correlates the primal and differential phase
        seed_grad = mitsuba.core.sample_tea_32(seed, 1)[0]
    elif seed_grad == seed:
        raise Exception('The primal and differential seed should be different '
                        'to ensure unbiased gradient computation!')

    return ek.custom(_RenderOp, scene, sensor, integrator,
                     (seed, seed_grad), (spp, spp_grad), params)


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

    Parameter ``sensor`` (``int``, ``mitsuba.render.Sensor``):
        Specify a sensor to render the scene from a different viewpoint.

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
        idx //= ek.opaque(UInt32, spp)

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


class ADIntegrator(mitsuba.render.SamplingIntegrator):
    """
    Abstract base class of numerous differentiable integrators in Mitsuba

    .. pluginparameters::

     * - max_depth
       - |int|
       - Specifies the longest path depth in the generated output image (where -1
         corresponds to :math:`\infty`). A value of |1| will only render directly
         visible light sources. |2| will lead to single-bounce (direct-only)
         illumination, and so on. (Default: |-1|)
     * - rr_depth
       - |int|
       - Specifies the path depth, at which the implementation will begin to use
         the *russian roulette* path termination criterion. For example, if set to
         |1|, then path generation many randomly cease after encountering directly
         visible surfaces. (Default: |5|)
     * - split_derivatives
       - |bool|
       - The differential phase of path replay-style integrators is split into
         two sub-phases that must communicate with each other. When
         |split_derivatives| is set to |true|, those two phases are executed
         using separate kernel launches that exchange information through
         global memory. If set to |false|, they are merged into a single kernel
         that keeps this information within registers. There are various
         pros/cons to either approach, so it's best to try both and use
         whichever is faster.
    """

    def __init__(self, props = mitsuba.core.Properties()):
        super().__init__(props)

        max_depth = props.get('max_depth', 4)
        if max_depth < 0 and max_depth != -1:
            raise Exception("\"max_depth\" must be set to -1 (infinite) or a value >= 0")

        # Map -1 (infinity) to 2^32-1 bounces
        self.max_depth = max_depth if max_depth != -1 else 0xffffffff

        self.rr_depth = props.get('rr_depth', 5)
        if self.rr_depth <= 0:
            raise Exception("\"rr_depth\" must be set to a value greater than zero!")

        self.split_derivative = props.get('split_derivative', False)

    def aovs(self):
        return []

    def to_string(self):
        return f'{type(self).__name__}[max_depth = {self.max_depth},' \
               f' rr_depth = { self.rr_depth }]'

    def render(self: mitsuba.render.SamplingIntegrator,
               scene: mitsuba.render.Scene,
               sensor: Union[int, mitsuba.render.Sensor] = 0,
               seed: int = 0,
               spp: int = 0,
               develop: bool = True,
               evaluate: bool = True) -> mitsuba.core.TensorXf:

        from mitsuba.core import Bool, UInt32, Float

        if not develop:
            raise Exception("develop=True must be specified when "
                            "invoking AD integrators")

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

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
            spec, valid, state = self.sample(
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

            # Accumulate into the image block
            alpha = ek.select(valid, Float(1), Float(0))
            block.put(pos, ray.wavelengths, spec * weight, alpha)

            # Explicitly delete any remaining unused variables
            del sampler, ray, weight, pos, ap_sample, spec, \
                valid, state, alpha

            # Probably a little overkill, but why not.. If there are any
            # Enoki arrays to be collected by Python's cyclic GC, then
            # freeing them may enable loop simplifications in ek.eval().
            gc.collect()

            # Perform the weight division and return an image tensor
            sensor.film().put_block(block)
            return sensor.film().develop()


    def render_forward(self: mitsuba.render.SamplingIntegrator,
                       scene: mitsuba.render.Scene,
                       sensor: Union[int, mitsuba.render.Sensor] = 0,
                       seed: int = 0,
                       spp: int = 0) -> mitsuba.core.TensorXf:
        """
        Evaluates the forward-mode derivative of the rendering step.

        Forward-mode differentiation propagates gradients from scene parameters
        through the simulation, producing a *gradient image* (i.e., the derivative
        of the rendered image with respect to those scene parameters). The gradient
        image is very helpful for debugging, for example to inspect the gradient
        variance or visualize the region of influence of a scene parameter. It is
        not particularly useful for simultaneous optimization of many parameters,
        since multiple differentiation passes are needed to obtain separate
        derivatives for each scene parameter. See ``Integrator.render_backward()``
        for an efficient way of obtaining all parameter derivatives at once, or
        simply use the ``mitsuba.python.ad.render()`` abstraction that hides both
        ``Integrator.render_forward()`` and ``Integrator.render_backward()`` behind
        a unified interface.

        Before calling this function, you must first enable gradient tracking and
        furthermore associate concrete input gradients with one or more scene
        parameters, or the function will just return a zero-valued gradient image.
        This is typically done by invoking ``ek.enable_grad()`` and
        ``ek.set_grad()`` on elements of the ``SceneParameters`` data structure
        that can be obtained obtained via a call to
        ``mitsuba.python.util.traverse()``.

        Parameter ``scene`` (``mitsuba.render.Scene``):
            The scene to be rendered differentially.

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

        from mitsuba.core import Bool, UInt32, Float

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

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

            # Launch the Monte Carlo sampling process in primal mode (1)
            spec, valid, state_out = self.sample(
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

            if self.split_derivative:
                # Probably a little overkill, but why not.. If there are any
                # Enoki arrays to be collected by Python's cyclic GC, then
                # freeing them may enable loop simplifications in ek.eval().
                gc.collect()
                ek.eval(state_out)

            # Launch the Monte Carlo sampling process in forward mode
            spec_2, valid_2, state_out_2 = self.sample(
                mode=ek.ADMode.Forward,
                scene=scene,
                sampler=sampler,
                ray=ray,
                depth=UInt32(0),
                δL=None,
                state_in=state_out,
                active=Bool(True)
            )

            # Prepare an ImageBlock as specified by the film
            block = sensor.film().create_block()

            # Only use the coalescing feature when rendering enough samples
            block.set_coalesce(block.coalesce() and spp >= 4)

            # Accumulate into the image block
            alpha = ek.select(valid_2, Float(1), Float(0))
            block.put(pos, ray.wavelengths, spec_2 * weight, alpha)

            # Explicitly delete any remaining unused variables
            del sampler, ray, weight, pos, spec_2, valid_2, \
                state_out, state_out_2, alpha

            # Probably a little overkill, but why not.. If there are any
            # Enoki arrays to be collected by Python's cyclic GC, then
            # freeing them may enable loop simplifications in ek.eval().
            gc.collect()

            # Perform the weight division and return an image tensor
            sensor.film().put_block(block)
            return sensor.film().develop()


    def render_backward(self: mitsuba.render.SamplingIntegrator,
                        scene: mitsuba.render.Scene,
                        grad_in: mitsuba.core.TensorXf,
                        sensor: Union[int, mitsuba.render.Sensor] = 0,
                        seed: int = 0,
                        spp: int = 0) -> None:
        """
        Evaluates the reverse-mode derivative of the rendering step.

        Reverse-mode differentiation transforms image-space gradients into scene
        parameter gradients, enabling simultaneous optimization of scenes with
        millions of free parameters. The function is invoked with an input
        *gradient image* (``grad_in``) and transforms and accumulates these into
        the gradient arrays of scene parameters that previously had gradient
        tracking enabled.

        Before calling this function, you must first enable gradient tracking for
        one or more scene parameters, or the function will not do anything. This is
        typically done by invoking ``ek.enable_grad()`` on elements of the
        ``SceneParameters`` data structure that can be obtained obtained via a call
        to ``mitsuba.python.util.traverse()``. Use ``ek.grad()`` to query the
        resulting gradients of these parameters once ``render_backward()`` returns.

        Parameter ``scene`` (``mitsuba.render.Scene``):
            The scene to be rendered differentially.

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

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()

        # self.sample() will re-enable gradients as needed, disable them here
        with ek.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = prepare(sensor, seed, spp)

            # Generate many rays from the camera
            ray, weight, pos, ap_sample = sample_rays(sensor, sampler)

            # Launch the Monte Carlo sampling process in primal mode (1)
            spec, valid, state_out = self.sample(
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

            if self.split_derivative:
                # Probably a little overkill, but why not.. If there are any
                # Enoki arrays to be collected by Python's cyclic GC, then
                # freeing them may enable loop simplifications in ek.eval().
                gc.collect()
                ek.eval(state_out)

            # Wrap the input gradient image into an ImageBlock
            block = ImageBlock(tensor=grad_in,
                               offset=film.crop_offset(),
                               rfilter=film.rfilter(),
                               normalize=True,
                               border=False) # May need to revisit..

            # Use the reconstruction filter to interpolate the values at 'pos'
            δL = block.read(pos) * (weight * ek.rcp(spp))

            # Launch Monte Carlo sampling in backward AD mode (2)
            spec_2, valid_2, state_out_2 = self.sample(
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


    def sample(self,
               mode: enoki.ADMode,
               scene: mitsuba.render.Scene,
               sampler: mitsuba.render.Sampler,
               ray: mitsuba.core.Ray3f,
               depth: mitsuba.core.UInt32,
               δL: Optional[mitsuba.core.Spectrum],
               state_in: Any,
               active: mitsuba.core.Bool) -> Tuple[mitsuba.core.Spectrum,
                                                   mitsuba.core.Bool, Any]:
        """
        This function does the main work of differentiable rendering and
        remains unimplemented here. It is provided by subclasses of the
        ``ADIntegrator`` interface.

        In those concrete implementations, the function performs a Monte Carlo
        random walk, implementing a number of different behaviors depending on
        the ``mode`` argument. For example in primal mode (``mode ==
        enoki.ADMode.Primal``), it behaves like a normal rendering algorithm
        and estimates the radiance incident along ``ray``.

        In forward mode (``mode == enoki.ADMode.Forward``), it estimates the
        derivative of the incident radiance for a set of scene parameters being
        differentiated. (This requires that these parameters are attached to
        the AD graph and have gradients specified via ``ek.set_grad()``)

        In backward mode (``mode == enoki.ADMode.Backward``), it takes adjoint
        radiance ``δL`` and accumulates it into differentiable scene parameters.

        You are normally *not* expected to directly call this function. Instead,
        use ``mitsuba.python.ad.render()`` , which performs various necessary
        setup steps to correctly use the functionality provided here.

        The parameters of this function are as follows:

        Parameter ``mode`` (``enoki.ADMode``)
            Specifies whether the rendering algorithm should run in primal or
            forward/backward derivative propagation mode

        Parameter ``scene`` (``mitsuba.render.Scene``):
            Reference to the scene being rendered in a differentiable manner.

        Parameter ``sampler`` (``mitsuba.render.Sampler``):
            A pre-seeded sample generator

        Parameter ``depth`` (``mitsuba.core.UInt32``):
            Path depth of `ray` (typically set to zero). This is mainly useful
            for forward/backward differentiable rendering phases that need to
            obtain an incident radiance estimate. In this case, they may
            recursively invoke ``sample(mode=ek.ADMode.Primal)`` with a nonzero
            depth.

        Parameter ``δL`` (``mitsuba.core.Spectrum``):
            When back-propagating gradients (``mode == enoki.ADMode.Backward``)
            the ``δL`` parameter should specify the adjoint radiance associated
            with each ray. Otherwise, it must be set to ``None``.

        Parameter ``state_in`` (``Any``):
            The primal phase of ``sample()`` returns a state vector as part of
            its return value. The forward/backward differential phases expect
            that this state vector is provided to them via this argument. When
            invoked in primal mode, it should be set to ``None``.

        Parameter ``active`` (``mitsuba.core.Bool``):
            This mask array can optionally be used to indicate that some of
            the rays are disabled.

        The function returns a tuple ``(spec, valid, state_out)`` where

        Output ``spec`` (``mitsuba.core.Spectrum``):
            Specifies the estimated radiance and differential radiance in
            primal and forward mode, respectively.

        Output ``valid`` (``mitsuba.core.Bool``):
            Indicates whether the rays intersected a surface, which can be used
            to compute an alpha channel.

        Output ``state_out`` (``Any``):
            When invoked in primal mode, this return argument provides an
            unspecified state vector that is a required input of both
            forward/backward differential phases.
        """

        raise Exception('ADIntegrator does not provide the sample() method. '
                        'It should be implemented by subclasses that '
                        'specialize the abstract ADIntegrator interface.')
