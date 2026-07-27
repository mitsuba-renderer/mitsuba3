.. _sec-spectra:

Spectra
=======

This section describes the plugins behind spectral reflectance or emission used
in Mitsuba 3. On an implementation level, these behave very similarly to the
:ref:`texture plugins <sec-textures>` described earlier (but lacking their
spatially varying property) and can thus be used similarly as either BSDF or
emitter parameters:

.. tabs::
    .. code-tab:: xml

        <scene version="3.0.0">
            <bsdf type=".. BSDF type ..">
                <!-- Explicitly add a uniform spectrum plugin -->
                <spectrum type=".. spectrum type .." name=".. parameter name ..">
                    <!-- Spectrum parameters go here -->
                </spectrum>
            </bsdf>
        </scene>

    .. code-tab:: python

        'type': 'scene',
        'bsdf_id': {
            'type': '<bsdf_type>',

            '<parameter name>': {
                'type': '<spectrum type>',
                # .. spectrum parameters ..
            }
        }

In practice, it is however discouraged to instantiate plugins in this explicit way
and the XML scene description parser directly parses a number of common (shorter)
``<spectrum>`` and ``<rgb>`` tags. See the corresponding section about the
`scene file format <https://mitsuba.readthedocs.io/en/v3.9.0/src/key_topics/scene_format.html>`_
for details.

The following two tables summarize which underlying plugins get instantiated
in each case, accounting for differences between reflectance and emission properties
and all different color modes. Each plugin is briefly summarized below.

.. figtable::
    :label: spectrum-reflectance-table-list
    :caption: Spectra used for reflectance (within BSDFs)
    :alt: Spectrum reflectance table

    .. list-table::
        :widths: 35 25 25 25
        :header-rows: 1

        * - XML description
          - Monochrome mode
          - RGB mode
          - Spectral mode
        * - ``<spectrum name=".." value="0.5"/>``
          - :ref:`uniform <spectrum-uniform>`
          - :ref:`uniform <spectrum-uniform>`
          - :ref:`uniform <spectrum-uniform>`
        * - ``<spectrum name=".." value="400:0.1, 700:0.2"/>``
          - :ref:`uniform <spectrum-uniform>`
          - :ref:`srgb <spectrum-srgb>`
          - :ref:`regular <spectrum-regular>`/:ref:`irregular <spectrum-irregular>`
        * - ``<spectrum name=".." filename=".."/>``
          - :ref:`uniform <spectrum-uniform>`
          - :ref:`srgb <spectrum-srgb>`
          - :ref:`regular <spectrum-regular>`/:ref:`irregular <spectrum-irregular>`
        * - ``<rgb name=".." value="0.5, 0.2, 0.5"/>``
          - :ref:`srgb <spectrum-srgb>`
          - :ref:`srgb <spectrum-srgb>`
          - :ref:`srgb <spectrum-srgb>`

.. figtable::
    :label: spectrum-emission-table-list
    :caption: Spectra used for emission (within emitters)
    :alt: Spectrum emission table

    .. list-table::
        :widths: 35 25 25 25
        :header-rows: 1

        * - XML description
          - Monochrome mode
          - RGB mode
          - Spectral mode
        * - ``<spectrum name=".." value="0.5"/>``
          - :ref:`uniform <spectrum-uniform>`
          - :ref:`srgb <spectrum-srgb>`
          - :ref:`uniform <spectrum-uniform>`
        * - ``<spectrum name=".." value="400:0.1, 700:0.2"/>``
          - :ref:`uniform <spectrum-uniform>`
          - :ref:`srgb <spectrum-srgb>`
          - :ref:`regular <spectrum-regular>`/:ref:`irregular <spectrum-irregular>`
        * - ``<spectrum name=".." filename=".."/>``
          - :ref:`uniform <spectrum-uniform>`
          - :ref:`srgb <spectrum-srgb>`
          - :ref:`regular <spectrum-regular>`/:ref:`irregular <spectrum-irregular>`
        * - ``<rgb name=".." value="0.5, 0.2, 0.5"/>``
          - :ref:`d65 <spectrum-d65>`
          - :ref:`srgb <spectrum-srgb>`
          - :ref:`d65 <spectrum-d65>`

A uniform spectrum does not produce a uniform RGB response in sRGB (which
has a D65 white point). Hence giving ``<spectrum name=".." value="1.0"/>``
as the radiance value of an emitter will result in a purple-ish color. On the
other hand, using such spectrum for a BSDF reflectance value will result in
an object appearing white. Both RGB and spectral modes of Mitsuba 3 will
exhibit this behavior consistently. The figure below illustrates this for
combinations of inputs for the emitter radiance (here using a constant emitter)
and the BSDF reflectance (here using a diffuse BSDF).

.. image:: ../../resources/data/docs/images/misc/spectrum_rgb_table.png
    :width: 60%
    :align: center

.. warning::

    While it is possible to define unbounded RGB properties (such as the ``eta``
    value for a conductor BSDF) using ``<rgb name=".." value=".."/>``
    tag, it is highly recommended to directly define a spectrum curve (or use a
    material from the conductor's built-in IOR list) as the spectral uplifting algorithm
    implemented in Mitsuba won't be able to guarantee that the produced spectrum
    will behave consistently in both RGB and spectral modes.

.. _sec-spectra-acoustic:

Frequency-domain spectra (acoustic variants)
--------------------------------------------

In the ``*_acoustic`` variants, the spectral domain is **frequency (Hz)** rather than
**wavelength (nm)**: :monosp:`Spectrum` represents a single acoustic frequency band, and
material/emission curves are defined over a frequency range instead of a visible-light range.

:ref:`regular <spectrum-regular>` and :ref:`irregular <spectrum-irregular>` both accept
frequency parameters as a drop-in alternative to their wavelength parameters:

- :ref:`regular <spectrum-regular>` accepts ``frequency_min``/``frequency_max`` in place of
  ``wavelength_min``/``wavelength_max``.
- :ref:`irregular <spectrum-irregular>` accepts ``frequencies`` in place of ``wavelengths``.

Only one of the two domains may be given for a single plugin instance — specifying both
wavelength and frequency parameters together raises an error at scene-load time.

:ref:`regular <spectrum-regular>`'s exposed ``range`` scene parameter (used for parameter
traversal, e.g. during optimization) holds this same two-value extent regardless of which
domain the plugin was constructed with — a frequency range in Hz for acoustic variants, a
wavelength range in nanometers otherwise.