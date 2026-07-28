.. _sec-acoustics:

Acoustic rendering
==================

misuka turns Mitsuba 3 into a room-acoustic renderer. The geometry, samplers,
scene format, and the `Dr.Jit <https://drjit.readthedocs.io/en/v1.4.0/>`_
JIT / autodiff engine all carry over unchanged. What changes is *what* is
transported and *what* the renderer records. Instead of spectral radiance
integrated into a 2D image, misuka transports **sound energy** and records an
**energy-time curve (ETC)**. The ETC is the energy arriving at a receiver as a
function of propagation time, resolved per frequency band.

This page explains the concepts a Mitsuba user needs in order to work
acoustically. The plugins themselves are documented in the
:ref:`plugin reference <sec-integrators>`, and worked examples live in the
:doc:`rendering <../rendering_tutorials>` and
:doc:`inverse rendering <../inverse_rendering_tutorials>` tutorials.

The ``_acoustic`` variant family
--------------------------------

Mitsuba compiles the same C++ sources into several *variants*, each a
``(backend × spectrum)`` combination. misuka adds an ``_acoustic`` spectrum in
which the spectral type is ``Spectrum<Float, 1>``, a **single-channel energy
value** rather than a colour. Acoustic scenes must be rendered with one of these
variants:

.. code-block:: python

    import misuka as mi

    mi.set_variant("llvm_ad_acoustic")   # then mi.Scene, mi.Float, … resolve acoustically

The usual backend prefixes apply (``scalar_``, ``llvm_``, ``cuda_`` and their
``_ad_`` autodiff forms), so ``llvm_ad_acoustic`` is a vectorized CPU variant
with automatic differentiation, ``cuda_ad_acoustic`` its GPU counterpart, and
``scalar_acoustic`` a single-ray variant that is easiest to debug. An optical
variant such as ``llvm_ad_rgb`` **cannot** render acoustic scenes. The acoustic
plugins and integrators only register under ``_acoustic`` variants. See the
`Mitsuba variants guide
<https://mitsuba.readthedocs.io/en/v3.9.0/src/key_topics/variants.html>`_ for the
underlying variant system, and the :ref:`developer guide <sec-compiling>` for
enabling ``_acoustic`` variants in :monosp:`misuka.conf`.

Because a single frequency band replaces the wavelength axis, the spectral
plugins take a **frequency (Hz)** domain rather than wavelengths. See
:ref:`Frequency-domain spectra <sec-spectra-acoustic>`.

Energy transport, not radiance
------------------------------

Acoustic simulation in misuka is a geometric (ray-based) energy simulation. A
path from a sound source to the receiver carries energy that is attenuated at
each surface interaction by the frequency-dependent absorption and scattering of
the :ref:`acoustic material <bsdf-acousticbsdf>`. Two differences from light
transport are fundamental:

- **Energy, not radiance.** Each frequency band carries a single scalar energy.
  There is no colour and no polarization. Surfaces attenuate energy per band
  according to their absorption/scattering spectra.
- **Time is explicit.** Light transport is treated as instantaneous, but sound
  travels at a finite **speed of sound** (default 343 m/s). The renderer tracks
  the total geometric path length and converts it into a propagation *time*,
  which is what makes an impulse response, the ETC, meaningful.

Ray-geometry intersection, importance sampling, the Dr.Jit computation graph,
and gradient propagation are all inherited from Mitsuba and Dr.Jit and behave as
usual.

The energy-time curve (ETC)
---------------------------

The ETC is the acoustic analogue of an image and the primary output of a
forward render. It is produced by the :ref:`tape <film-tape>` film paired with a
:ref:`microphone <sensor-microphone>` sensor, and records how much energy
reaches the receiver in each **time bin**, separately for each **frequency
band**.

- The **frequency bands** are listed explicitly on the ``tape`` film via its
  ``frequencies`` parameter (e.g. octave-band centre frequencies). Each entry is
  one band of the output.
- The **time bins** (``time_bins``) discretize propagation time. Together with
  the integrator's ``max_time`` they set the temporal resolution: each bin
  covers ``max_time / time_bins`` seconds, and a path contributes to the bin
  matching its total propagation time. Paths whose travel distance exceeds
  ``max_time × speed_of_sound`` are discarded.

A render therefore returns a tensor of shape ``(time_bins, frequencies, 1)``,
with energy against time along the first axis and one column per frequency band.
This replaces the ``(height, width, channels)`` image a conventional film would
produce. The microphone has no image resolution, only a single receiver point.

.. code-block:: python

    max_time      = 0.1     # seconds
    sampling_rate = 10000   # time bins per second

    microphone = mi.load_dict({
        "type": "microphone",
        "origin": [2.0, 1.0, 1.2],
        "direction": [3.0, 6.0, 1.2],
        "film": {
            "type": "tape",
            "frequencies": "125, 250, 500, 1000, 2000, 4000",
            "time_bins": int(max_time * sampling_rate),
        },
    })

    integrator = mi.load_dict({
        "type": "acoustic_path",
        "max_time": max_time,
        "speed_of_sound": 343,
        "max_depth": -1,        # unlimited reflections
    })

    etc = mi.render(scene, sensor=microphone, integrator=integrator, spp=2**16)

What carries over, and what is replaced
---------------------------------------

If you already know Mitsuba 3, the mental model is: **keep the scene, swap the
acoustic components.**

.. list-table::
    :header-rows: 1
    :widths: 30 35 35

    * - Role
      - Mitsuba (optical)
      - misuka (acoustic)
    * - Spectrum
      - RGB / spectral radiance
      - single-channel energy per frequency band
    * - Material
      - optical BSDFs (``diffuse``, ``conductor``, …)
      - :ref:`acousticbsdf <bsdf-acousticbsdf>` (absorption + scattering)
    * - Sensor
      - camera (``perspective``, …)
      - :ref:`microphone <sensor-microphone>` (receiver point)
    * - Film
      - image film (``hdrfilm``, …)
      - :ref:`tape <film-tape>` (ETC storage)
    * - Integrator
      - path / PRB light-transport integrators
      - :ref:`acoustic_path <integrator-acoustic_path>` (forward) and the
        differentiable :ref:`acoustic_ad <integrator-acoustic_ad>` /
        :ref:`acoustic_prb <integrator-acoustic_prb>` integrators

Geometry, transforms, samplers, reconstruction filters, the XML/dict scene
format, and the whole Dr.Jit / autodiff stack are unchanged and remain
documented upstream.

Differentiable acoustics
------------------------

Under an ``_ad_`` variant, misuka differentiates the ETC with respect to scene
parameters: material absorption/scattering, source and receiver positions, and
geometry. The :ref:`acoustic_prb <integrator-acoustic_prb>` integrator
implements **Time-Resolved Path Replay Backpropagation**, which propagates
gradients efficiently across many reflections without storing the full path
history. This is the basis for the inverse-rendering tutorials, where scene
parameters are optimized to match a target ETC.
