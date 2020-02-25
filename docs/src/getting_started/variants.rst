.. _sec-variants:

Rendering variants
==================

Mitsuba 2 is a *retargetable* rendering system: it can be run in different rendering
modes that either change the representation of color (enabling spectral or polarized
rendering) or even the basic arithmetic type used by the system (lifting the system
to a vectorized renderer or enabling automatic differentiation).
All of this is achieved with a single codebase.


The :monosp:`mitsuba.conf` file
--------------------------------

This file lists variants of Mitsuba 2 that should be included during compilation. Note that
enabling many features here can lead to slow build times and very large binaries.  Available
features (which can often be combined) include:

1. **Basic arithmetic type used for computation**

   - ``scalar``: Computation is done on the CPU using ordinary floating point arithmetic as in
     Mitsuba 0.5. This is the preferred mode for fixing compilation errors (there is relatively
     little templating) and debugging the renderer.
   - ``packet``: Computation is done using packets of floating point numbers, exploiting vectorized
     instruction set extensions such as SSE4.2, AVX, AVX2, AVX512, NEON, etc. Not that this is not a
     magic bullet, rendering won't automatically be 8 or 16x faster. You will likely want to enable
     Embree in this mode and run specialized vectorized integrators for maximum benefit in this mode.
   - ``gpu``: Computation is done on the GPU using Enoki's Just-in-time compiler to generate CUDA
     kernels on the fly. In this case, Mitsuba becomes a wavefront path tracer that performs ray
     tracing using NVIDIA's OptiX library.
   - ``gpu_autodiff``: building on the properties of the ``gpu`` variant, computation additionally
     keeps track of derivatives, enabling the use of Mitsuba 2 for optimization and the
     solution of challenging inverse problems.

2. **Representation of color**

   - ``mono``: Monochromatic simulation, i.e., simply don't simulate color at all. This is useful
     when simulating scenes that are inherently monochromatic (e.g. with single-frequency laser
     illumination.) All input scene data is converted to grayscale.
   - ``rgb``: Simulate light transport using an RGB-based color representation (as e.g. done in
     Mitsuba 0.5). This is not particular realistic and somewhat arbitrary as multiplying RGB colors
     can yield dramatically different answers depending on the underlying RGB color space.
   - ``spectral``: Integrate over continuous wavelengths spanning the visible spectrum
     (360..830 nm). Any RGB data provided in the input scene has to be up-sampled into plausible
     equivalent spectra in this case.
   - ``spectral_polarized``: Building on the properties of the ``spectral`` variant, additionally keep
     track of the polarization state of light. Builtin materials based on specular reflection and
     refraction will also switch to the polarized form of the Fresnel equations.

3. **Precision**

   - default: Mitsuba normally uses single precision for all computation.
   - ``double``: Sometimes, it can be useful to compile a higher-precision version of the renderer
     to determine if an issue arises due to insufficient floating point accuracy.

These 3 feature dimensions can then be concatenated into variant names like ``scalar_rgb_double``.



Spectral rendering
------------------

Compared to usual RGB rendering modes, Mitsuba 2 can also perform full spectral
rendering by performing additional Monte Carlo integration over the (visible)
wavelengths. This can considerably improve accuracy especially in scenarios where
measured spectral data is available. Consider for example the two Cornell box
renderings below: on the left side, the spectral reflectance data of all materials
is first converted to RGB and rendering using the RGB rendering mode (`scalar_rgb`).
In contrast, running the simulation in full spectral mode (`scalar_spectral`) results
in a surprisingly different image.

.. subfigstart::
.. subfigure:: ../../../resources/data/docs/images/render/variants_cbox_rgb.jpg
   :caption: RGB Mode
.. subfigure:: ../../../resources/data/docs/images/render/variants_cbox_spectral.jpg
   :caption: Spectral Mode
.. subfigend::
   :label: fig-cbox-spectral

This is achieved with an implementation of *Hero wavelength sampling* :cite:`Wilkie2014Hero`.
In case some data is only available in a RGB format (e.g. for image textures), Mitsuba 2
performs spectral upsampling :cite:`Jakob2019Spectral` and will transform the data to
continuous spectra that can be sampled for arbitrary wavelengths.


Polarized rendering
-------------------

Optionally, Mitsuba 2 supports polarized rendering modes (e.g. `scalar_spectral_polarized`)
which, in addition to normal radiance, also track the full polarization state of light.
Inside the light transport simulation, *Stokes vectors* are used to parameterize
the elliptical shape of the transverse oscillations, and *Mueller matrices* are used
to compute the effect of surface scattering on the polarization :cite:`Collett1993PolarizedLight`.

.. image:: ../../images/polarization_wave.svg
    :width: 60%
    :align: center

For more details regarding the implementation of the polarized rendering modes, please
refer to the :ref:`developer_guide-polarization` section in the developer guide.


Differentiable rendering
------------------------

*TODO*


Vectorized rendering
--------------------

*TODO*

