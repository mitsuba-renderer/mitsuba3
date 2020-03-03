.. _sec-spectra:

Spectra
=======

This section describes the plugins behind spectral reflectance or emission used
in Mitsuba 2. On an implementation level, these behave very similarly to the
:ref:`texture plugins <sec-textures>` described earlier (but lacking their
spatially varying property) and can thus be used similarly as either BSDF or
emitter parameters:

.. code-block:: xml

    <scene version=2.0.0>
        <bsdf type=".. BSDF type ..">
            <!-- Explicitly add a uniform spectrum plugin -->
            <spectrum type=".. spectrum type .." name=".. parameter name ..">
                <!-- Spectrum parameters go here -->
            </spectrum>
        </bsdf>
    </scene>

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
          - :ref:`uniform <spectrum-uniform>`
          - :ref:`d65 <spectrum-d65>`
        * - ``<spectrum name=".." value="400:0.1, 700:0.2"/>``
          - :ref:`uniform <spectrum-uniform>`
          - :ref:`srgb_d65 <spectrum-srgb_d65>`
          - :ref:`regular <spectrum-regular>`/:ref:`irregular <spectrum-irregular>`
        * - ``<spectrum name=".." filename=".."/>``
          - :ref:`uniform <spectrum-uniform>`
          - :ref:`srgb_d65 <spectrum-srgb_d65>`
          - :ref:`regular <spectrum-regular>`/:ref:`irregular <spectrum-irregular>`
        * - ``<rgb name=".." value="0.5, 0.2, 0.5"/>``
          - :ref:`srgb_d65 <spectrum-srgb_d65>`
          - :ref:`srgb_d65 <spectrum-srgb_d65>`
          - :ref:`srgb_d65 <spectrum-srgb_d65>`
