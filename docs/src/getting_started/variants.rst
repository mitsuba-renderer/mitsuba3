.. _sec-variants:

Variants
========

Mitsuba 2 is a *retargetable* rendering system: it provides a wide variety of
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


Note that build time and memory usage is roughly proportional to the number of
enabled variants, hence including many of them (more than five) may not be
advisable. Mitsuba 2 developers will typically want to restrict themselves to
1-2 variants used by their current experiment to minimize edit-recompile times.
Each variant is associated with an identifying name name that is composed of
several elements:

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
  rendering algorithm (ray tracing, BSDF sampling) will therefore operate on
  multiple inputs at once. Note, however, that is not a magic bullet: rendering
  won't automatically be 8 or 16x faster. Packet mode requires special
  algorithms and is mainly intended for software developers that can make use
  of this type of parallelism.

- The ``gpu`` backend off-loads computation to the GPU using `Enoki's
  <https://github.com/mitsuba-renderer/enoki>`_ just-in-time (JIT) compiler
  that transforms computation into CUDA kernels. Using this backend, each
  operation typically operates on millions of inputs at the same time. Mitsuba
  then becomes what is known as a *wavefront path tracer* and delegates ray
  tracing on the GPU to NVIDIA's OptiX library. Note that this requires a
  relatively recent NVIDIA GPU: ideally Turing or newer. The older Pascal
  architecture is also supported but tends to be slower because it lacks ray
  tracing hardware acceleration.

- The ``gpu_autodiff`` backend builds on the ``gpu`` backend and adds the
  possibility to differentiate the computation. This is required for
  *differentiable rendering*, which interprets the rendering algorithm as a
  function :math:`f(\mathbf{x})` that converts an input :math:`\mathbf{x}` (the
  scene description) into an output :math:`\mathbf{y}` (the rendering).
  Differentiable rendering then mathematically differentiates the function
  :math:`f` to obtain derivatives
  :math:`\frac{\mathrm{d}\mathbf{x}}{\mathrm{d}\mathbf{y}}` or
  :math:`\frac{\mathrm{d}\mathbf{y}}{\mathrm{d}\mathbf{x}}`.

  Differentiable rendering uses such derivatives to determine how a desired
  change in the output :math:`\mathbf{y}` (the rendering) can be achieved by
  changing the inputs :math:`\mathbf{x}` (the scene description). Because
  derivatives only provide extremely local information about the behavior of
  the function :math:`f`, this does not immediately result in the desired
  answer. Instead, the process must be split into smaller steps that produce a
  sequence of scene parameters :math:`\mathbf{x}_1`, :math:`\mathbf{x}_2`,
  :math:`\mathbf{x}_3`, that progressively improve the quality of the inversion, as
  measured by an *objective function* :math:`g(\mathbf{y})`. Here is an
  illustration of the involved steps:

  .. image:: ../../../resources/data/docs/images/autodiff/autodiff.jpg
      :width: 100%
      :align: center




  enabling the
  use of Mitsuba 2 for optimization and the solution of challenging inverse
  problems.

An appealing aspect of both ``packet_*`` and ``gpu_*`` modes, is that they
expose an efficient *vectorized* Python interface that operates on arbitrarily
large set of inputs. This means that millions of ray tracing operations or BSDF
evaluations can be performed with a single Python function call, enabling
efficient prototyping within Python or Jupyter notebooks.

We generally recommend compiling ``scalar`` variants for command line
rendering, and ``packet`` or ``gpu_autodiff`` variants for with Mitsuba Python
development---the latter, only if differentiable rendering is desired.

.. [#f1] Scalar mode can also be very useful for tracking down compilation errors or
    to debug incorrect code. It makes little use of templating, which reduces
    the length of compiler error messages and facilitates the use of debuggers
    like GDB or LLVM.

Part 2: Color representation
----------------------------

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


Part 3: Polarization
--------------------

- default: Mitsuba normally uses single precision for all computation.

- ``double``: Sometimes, it can be useful to compile a higher-precision version
  of the renderer to determine if an issue arises due to insufficient floating
  point accuracy.

Part 4: Precision
-----------------

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
