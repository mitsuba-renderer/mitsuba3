.. _sec-variants:

Variants
========

Mitsuba 2 is a *retargetable* rendering system: it provides a large set of
different *variants* that change elementary aspects of simulation---they can
for instance replace the representation of color to support monochromatic, RGB,
spectral, or even polarized illumination. Similarly, the basic arithmetic types
underlying the simulation can be exchanged to perform renderings using a higher
amount of precision, vectorization to process many light paths at once, or
automatic differentiation to to solve inverse problems using gradient-based
optimization. Variants are automatically created from a single generic codebase.


Choosing relevant variants
--------------------------

As many as 36 different variants of the renderer are presently available, shown
in the list below. Before building Mitsuba 2, you will therefore need to decide
which of these are relevant for your intended application.

.. container:: toggle

    .. container:: header

        **Show/hide available variants**

    .. code-block:: bash

        scalar_mono
        scalar_mono_double
        scalar_mono_polarized
        scalar_mono_polarized_double
        scalar_rgb
        scalar_rgb_double
        scalar_rgb_polarized
        scalar_rgb_polarized_double
        scalar_spectral
        scalar_spectral_double
        scalar_spectral_polarized
        scalar_spectral_polarized_double
        packet_mono
        packet_mono_double
        packet_mono_polarized
        packet_mono_polarized_double
        packet_rgb
        packet_rgb_double
        packet_rgb_polarized
        packet_rgb_polarized_double
        packet_spectral
        packet_spectral_double
        packet_spectral_polarized
        packet_spectral_polarized_double
        gpu_mono
        gpu_mono_polarized
        gpu_rgb
        gpu_rgb_polarized
        gpu_spectral
        gpu_spectral_polarized
        gpu_autodiff_mono
        gpu_autodiff_mono_polarized
        gpu_autodiff_rgb
        gpu_autodiff_rgb_polarized
        gpu_autodiff_spectral
        gpu_autodiff_spectral_polarized


Note that compilation time and compilation memory usage is roughly proportional
to the number of enabled variants, hence including many of them (more than
five) may not be advisable. Mitsuba 2 developers will typically want to
restrict themselves to 1-2 variants used by their current experiment to
minimize edit-recompile times. Each variant is associated with an identifying
name name that is composed of several elements:

.. image:: ../../images/variant.svg
    :width: 80%
    :align: center

We will now discuss component in turn.

Part 1: Computational backend
-----------------------------

The computational backend controls how basic arithmetic operations like
additions or multiplications are realized by the system. The following choices
are available:

- The ``scalar`` backend performs computation on the CPU using normal floating
  point arithmetic similar to older versions of Mitsuba. This is the default
  choice for generating renderings using the :monosp:`mitsuba` command line
  executable, or using the graphical user interface.

- The ``packet`` backend efficiently performs calculations on groups of 4, 8,
  or 16 floating point numbers, exploiting instruction set extensions such as
  SSE4.2, AVX, AVX2, and AVX512. In packet mode, every single operation in a
  rendering algorithm (ray tracing, BSDF sampling, etc.) will therefore operate on
  multiple inputs at once. The following visualizations of tracing light
  paths in scalar and packet mode gives an idea of this difference:

  .. image:: ../../../resources/data/docs/images/variants/vectorization.jpg
      :width: 100%
      :align: center

  Note, however, that packet mode is not a magic bullet: standard algorithms
  won't automatically be 8 or 16x faster. Packet mode requires special
  algorithms and is intended to be used by developers, whose software can
  exploit this type of parallelism. 

- The ``gpu`` backend offloads computation to the GPU using `Enoki's
  <https://github.com/mitsuba-renderer/enoki>`_ just-in-time (JIT) compiler
  that transforms computation into CUDA kernels. Using this backend, each
  operation typically operates on millions of inputs at the same time. Mitsuba
  then becomes what is known as a *wavefront path tracer* and delegates ray
  tracing on the GPU to NVIDIA's OptiX library. Note that this requires a
  relatively recent NVIDIA GPU: ideally *Turing* or newer. The older *Pascal*
  architecture is also supported but tends to be slower because it lacks ray
  tracing hardware acceleration.

- Building on the ``gpu`` backend, ``gpu_autodiff`` furthermore propagates
  derivative information through the simulation, which is a crucial ingredient
  for solving *inverse problems* using rendering algorithms.

  The following shows an example from :cite:`NimierDavidVicini2019Mitsuba2`.
  Here, Mitsuba 2 is used to compute the height profile of a transparent glass
  panel that refracts red, green, and blue light in such a way as to reproduce
  a specified color image.

  .. image:: ../../../resources/data/docs/images/autodiff/caustic.jpg
      :width: 100%
      :align: center

  The main use case of the ``gpu_autodiff`` backend is *differentiable
  rendering*, which interprets the rendering algorithm as a function
  :math:`f(\mathbf{x})` that converts an input :math:`\mathbf{x}` (the scene
  description) into an output :math:`\mathbf{y}` (the rendering). This function
  :math:`f` is then mathematically differentiated to obtain
  :math:`\frac{\mathrm{d}\mathbf{y}}{\mathrm{d}\mathbf{x}}`, providing a
  first-order approximation of how a desired change in the output
  :math:`\mathbf{y}` (the rendering) can be achieved by changing the inputs
  :math:`\mathbf{x}` (the scene description). Together with a differentiable
  *objective function* :math:`g(\mathbf{y})` that quantifies the suitability of
  tentative scene parameters and a gradient-based optimization algorithm, a
  differentiable renderer can be used to solve complex inverse problems
  involving light.

  .. image:: ../../../resources/data/docs/images/autodiff/autodiff.jpg
      :width: 100%
      :align: center

  The documentations provides several applied examples on :ref:`differentiable
  and inverse rendering <sec-inverse-rendering>`.

An appealing aspect of ``packet``, ``gpu``, and ``gpu_autodiff`` modes, is that
they expose *vectorized* Python interfaces that operate on arbitrarily large
set of inputs (even in the case of ``packet`` mode that works with smaller
arrays. The C++ implementation sweeps over larger inputs in this case). This
means that millions of ray tracing operations or BSDF evaluations can be
performed with a single Python function call, enabling efficient prototyping
within Python or Jupyter notebooks without costly iteration over many elements.

How to choose?
^^^^^^^^^^^^^^

We generally recommend compiling ``scalar`` variants for command line
rendering, and ``packet`` or ``gpu_autodiff`` variants for Python
development---the latter only if differentiable rendering is desired.

Part 2: Color representation
----------------------------

The next part determines how Mitsuba represents color information. The
following choices are available:

- ``mono`` completely disables the concept of color, which is useful when
  simulating scenes that are inherently monochromatic (e.g. illumination due to
  a laser). This mode is great for writing testcases where color is simply not
  relevant. When an input scene provides color information, :monosp:`mono` mode
  automatically converts it to grayscale.

- ``rgb`` mode selects an RGB-based color representation. This is a reasonable
  default choice and matches the typical behavior of the previous generation of
  Mitsuba. On the flipside, RGB mode can be a poor approximation of how color
  works in the real world. Please click on the following for a longer
  explanation.

    .. container:: toggle

        .. container:: header

            **Issues involving RGB-based rendering (click to expand)**

        In particular, multiplication of RGB colors is a nonsensical operation,
        which yields very different answers depending on the underlying RGB
        color space.

        .. image:: ../../../resources/data/docs/images/variants/rgb-mode-issue.svg
            :width: 100%
            :align: center

- Finally, ``spectral`` results in a spectral color representation spanning the
  visible spectrum (360..830 nm). 


Any RGB data provided in the input scene has to be
  up-sampled into plausible equivalent spectra in this case.


Compared to usual RGB rendering modes, Mitsuba 2 can also perform full spectral
rendering by performing additional Monte Carlo integration over the (visible)
wavelengths. This can considerably improve accuracy especially in scenarios where
measured spectral data is available. Consider for example the two Cornell box
renderings below: on the left side, the spectral reflectance data of all materials
is first converted to RGB and rendering using the RGB rendering mode (``scalar_rgb``).
In contrast, running the simulation in full spectral mode (``scalar_spectral``) results
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

Part 3: Polarization
--------------------

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


Part 4: Precision
-----------------

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
