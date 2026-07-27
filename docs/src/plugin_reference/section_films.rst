.. _sec-films:

Films
=====

A film defines how conducted measurements are stored and converted into the
final output that is written to disk at the end of the rendering process.
misuka's acoustic film is :ref:`tape <film-tape>`, which records an
**Energy Time Curve (ETC)**: energy accumulated per frequency band, against
propagation time. Unlike an image film, its two axes are frequency bands and
time bins rather than pixel width and height.

In the XML scene description language, a film configuration might look as
follows:

.. tabs::
    .. code-tab:: xml

        <scene version="3.0.0">
            <!-- .. scene contents -->

            <sensor type="microphone">
                <!-- .. sensor parameters .. -->

                <!-- Record an ETC over three frequency bands -->
                <film type="tape">
                    <string name="frequencies" value="100, 500, 20000"/>
                    <integer name="time_bins" value="1000"/>
                </film>
            </sensor>
        </scene>

    .. code-tab:: python

        'type': 'scene',

        # .. scene contents ..

        'sensor_id': {
            'type': 'microphone',

            # Record an ETC over three frequency bands
            'film_id': {
                'type': 'tape',
                'frequencies': '100, 500, 20000',
                'time_bins': 1000,
            }
        }

The ``<film>`` plugin should be instantiated inside a ``<sensor>``
declaration. As with other films, the output filename is inferred from the
scene filename and can be manually overridden by passing the configuration
parameter ``-o`` to the ``mitsuba`` executable when rendering from the
command line.
