.. _sec-variants:

Variants
========

Mitsuba 2 is a *retargetable* rendering system: it provides a wide variety of
different *variants* that change elementary aspects of simulation---they can
for instance change the representation of color to support monochromatic, RGB,
spectral, or even polarized illumination. Similarly, the basic arithmetic types
underlying the simulation can be exchanged to perform renderings using a higher
amount of precision, vectorization to process many light paths at once, or
automatic differentiation to to solve inverse problems using gradient-based
optimization. Variants are automatically created from a single generic codebase.


Choosing relevant variants
--------------------------

Over 20 different variants of the renderer are presently available, shown in
the list below. Before building Mitsuba 2, you will therefore need to decide
which of these are relevant for your intended application.

.. container:: toggle

    .. container:: header

        **Show/hide available variants**

    .. code-block:: bash

        scalar_mono
        scalar_mono_double
        scalar_rgb
        scalar_rgb_double
        scalar_spectral
        scalar_spectral_double
        scalar_spectral_polarized
        scalar_spectral_polarized_double
        packet_mono
        packet_mono_double
        packet_rgb
        packet_rgb_double
        packet_spectral
        packet_spectral_double
        packet_spectral_polarized
        packet_spectral_polarized_double
        gpu_mono
        gpu_rgb
        gpu_spectral
        gpu_spectral_polarized
        gpu_autodiff_mono
        gpu_autodiff_rgb
        gpu_autodiff_spectral
        gpu_autodiff_spectral_polarized

Note that building the system with many variants (e.g., > 5) will lead to a
proportional increase in compilation times. Mitsuba 2 developers will typically
want to restrict themselves to 1-2 variants used by their current experiment to
minimize edit-recompile times. Each variant is associated with an identifying
name name that is composed of several elements:

.. image:: ../../images/variant.svg
    :width: 80%
    :align: center

We now discuss component in turn.

Computational backend
---------------------

- ``scalar``: Computation is done on the CPU using ordinary floating point
  arithmetic similar to older versions of Mitsuba. [#f1]_

- ``packet``: Computation is done using packets of floating point numbers,
  exploiting vectorized instruction set extensions such as SSE4.2, AVX, AVX2,
  AVX512, NEON, etc. Packet variants and the following ``gpu_`` variants expose
  an efficient *vectorized* Python interface that can, e.g., be used to trace
  millions of rays and evaluate BSDFs at the intersections using only two
  function calls. [#f2]_ 

- ``gpu``: Computation is done on the GPU using Enoki's Just-in-time compiler
  to generate CUDA kernels on the fly. In this case, Mitsuba becomes a
  wavefront path tracer that performs ray tracing using NVIDIA's OptiX library.

- ``gpu_autodiff``: building on the properties of the ``gpu`` variant,
  computation additionally keeps track of derivatives, enabling the use of
  Mitsuba 2 for optimization and the solution of challenging inverse problems.

Color representation
--------------------

- ``mono``: Monochromatic simulation that does not keep track of color at all.
  This is useful when simulating scenes that are inherently monochromatic (e.g.
  with single-frequency laser illumination.) All input scene data is converted
  to grayscale.

- ``rgb``: Simulate light transport using an RGB-based color representation (as
  e.g. done in Mitsuba 0.5). This is not particular realistic and somewhat
  arbitrary as multiplying RGB colors can yield dramatically different answers
  depending on the underlying RGB color space.

- ``spectral``: Integrate over continuous wavelengths spanning the visible
  spectrum (360..830 nm). Any RGB data provided in the input scene has to be
  up-sampled into plausible equivalent spectra in this case.

- ``spectral_polarized``: Building on the properties of the ``spectral``
  variant, additionally keep track of the polarization state of light. Builtin
  materials based on specular reflection and refraction will also switch to the
  polarized form of the Fresnel equations.


Precision specifier
-------------------

- default: Mitsuba normally uses single precision for all computation.

- ``double``: Sometimes, it can be useful to compile a higher-precision version
  of the renderer to determine if an issue arises due to insufficient floating
  point accuracy.

The :monosp:`mitsuba.conf` file
--------------------------------

Mitsuba 2 variants are specified in the file :monosp:`mitsuba.conf`. To get
started, first copy the default template to the directory where you intend
to compile Mitsuba:

.. code-block:: bash

    mkdir build
    cd build
    cp <..mitsuba directory..>/resources/mitsuba.conf.template mitsuba.conf

The default :monosp:`mitsuba.conf` file contains the following lines that
select three variants of Mitsuba for compilation:

.. code-block:: json

    "enabled": [
        "scalar_rgb",
        "scalar_spectral",
        "packet_rgb"
    ],



These 3 feature dimensions can then be concatenated into variant names like ``scalar_rgb_double``.

.. rubric:: Footnotes

.. [#f1] Scalar mode can also be very useful for tracking down compilation errors or
    to debug incorrect code. It makes little use of templating, which reduces
    the length of compiler error messages and facilitates the use of debuggers
    like GDB or LLVM.

.. [#f2] Not that packet mode is not a magic bullet: rendering won't
    automatically be 8 or 16x faster---this also requires algorithms with
    sufficiently coherent execution behavior. You will likely want to enable Embree
    (CMake option `MTS_ENABLE_EMBREE`).

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


Footnotes
---------
