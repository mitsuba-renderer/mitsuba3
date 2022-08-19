.. _sec-spectra:

Spectra
=======

This section describes the plugins behind spectral reflectance or emission used
in 3. On an implementation level, these behave very similarly to the
:ref:`texture plugins <sec-textures>` described earlier (but lacking their
spatially varying property) and can thus be used similarly as either BSDF or
emitter parameters:

.. tabs::
    .. code-tab:: xml

        <scene version=3.0.0>
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
``<spectrum>`` and ``<rgb>`` tags See the corresponding section about the
:ref:`scene file format <sec-file-format>` for details.

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
combinations of inputs for the emitter radiance (here using a :ref:`constant <emitter-constant>` emitter)
and the BSDF reflectance (here using a :ref:`diffuse <bsdf-diffuse>` BSDF).

.. image:: ../../resources/data/docs/images/misc/spectrum_rgb_table.png
    :width: 60%
    :align: center

.. warning::

    While it is possible to define unbounded RGB properties (such as the ``eta``
    value for a :ref:`conductor BSDF <bsdf-conductor>`) using ``<rgb name=".." value=".."/>``
    tag, it is highly recommended to directly define a spectrum curve (or use a
    material from :num:`conductor-ior-list>`) as the spectral uplifting algorithm
    implemented in Mitsuba won't be able to guarantee that the produced spectrum
    will behave consistently in both RGB and spectral modes.