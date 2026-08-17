.. _sec-spectra:

Spectra
=======

This section describes the plugins behind spectral reflectance or emission used
in misuka. They can be used as either BSDF or emitter parameters:

.. tabs::
    .. code-tab:: python

        'type': 'scene',
        'bsdf_id': {
            'type': '<bsdf_type>',

            '<parameter name>': {
                'type': '<spectrum type>',
                # .. spectrum parameters ..
            }
        }

    .. code-tab:: xml

        <scene version="3.0.0">
            <bsdf type=".. BSDF type ..">
                <!-- Explicitly add a uniform spectrum plugin -->
                <spectrum type=".. spectrum type .." name=".. parameter name ..">
                    <!-- Spectrum parameters go here -->
                </spectrum>
            </bsdf>
        </scene>

In practice, it is however discouraged to instantiate plugins in this explicit way,
and both the Python dict and XML scene parsers directly accept a number of common
(shorter) forms for a spectrum entry. Which plugin such a shorthand instantiates depends on its contents:

A single value applies at every frequency and instantiates
:ref:`uniform <spectrum-uniform>`:

.. tabs::
    .. code-tab:: python

        'absorption': {
            'type': 'spectrum',
            'value': 0.3
        }

    .. code-tab:: xml

        <spectrum name="absorption" value="0.3"/>

Frequency-value pairs instantiate :ref:`regular <spectrum-regular>` when the frequency
nodes are evenly spaced:

.. tabs::
    .. code-tab:: python

        'absorption': {
            'type': 'spectrum',
            'value': [(100, 0.1), (200, 0.2), (300, 0.4)]
        }

    .. code-tab:: xml

        <spectrum name="absorption" value="100:0.1, 200:0.2, 300:0.4"/>

They instantiate :ref:`irregular <spectrum-irregular>` otherwise. Octave-band center
frequencies fall into this case, since they are spaced logarithmically:

.. tabs::
    .. code-tab:: python

        'absorption': {
            'type': 'spectrum',
            'value': [(125, 0.1), (250, 0.2), (500, 0.4)]
        }

    .. code-tab:: xml

        <spectrum name="absorption" value="125:0.1, 250:0.2, 500:0.4"/>

A ``filename`` reads the same pairs from a file, one frequency and value per line, and
chooses between the two plugins in the same way:

.. tabs::
    .. code-tab:: python

        'absorption': {
            'type': 'spectrum',
            'filename': 'absorption.spd'
        }

    .. code-tab:: xml

        <spectrum name="absorption" filename="absorption.spd"/>

.. note::

    These three are the only spectra tested for acoustic rendering, and they are the ones used
    throughout misuka's acoustic examples and tests. :ref:`regular <spectrum-regular>` and
    :ref:`irregular <spectrum-irregular>` are also the only ones that accept frequency
    parameters rather than wavelengths, as described below.

    misuka inherits Mitsuba's remaining spectra (``srgb``, ``d65``, ``blackbody`` and
    ``rawconstant``). They still load under an acoustic variant, but they are defined over
    wavelengths and exist to serve Mitsuba's color handling, so they are documented in the
    :external+mitsuba:ref:`Mitsuba plugin reference <sec-plugins>`
    rather than here.

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

Only one of the two domains may be given for a single plugin instance. Specifying both
wavelength and frequency parameters together raises an error at scene-load time.

:ref:`regular <spectrum-regular>`'s exposed ``range`` scene parameter (used for parameter
traversal, e.g. during optimization) holds this same two-value extent regardless of which
domain the plugin was constructed with. It is a frequency range in Hz for acoustic variants,
and a wavelength range in nanometers otherwise.