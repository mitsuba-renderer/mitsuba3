.. _sec-films:

Films
=====

A film defines how conducted measurements are stored and converted into the final
output file that is written to disk at the end of the rendering process.

In the XML scene description language, a normal film configuration might look
as follows:

.. tabs::
    .. code-tab:: xml

        <scene version=3.0.0>
            <!-- .. scene contents -->

            <sensor type=".. sensor type ..">
                <!-- .. sensor parameters .. -->

                <!-- Write to a high dynamic range EXR image -->
                <film type="hdrfilm">
                    <!-- Specify the desired resolution (e.g. full HD) -->
                    <integer name="width" value="1920"/>
                    <integer name="height" value="1080"/>

                    <!-- Use a Gaussian reconstruction filter. -->
                    <rfilter type="gaussian"/>
                </film>
            </sensor>
        </scene>

    .. code-tab:: python

        'type': 'scene',

        # .. scene contents ..

        'sensor_id': {
            'type': '<sensor_type>'

            # Write to a high dynamic range EXR image
            'film_id': {
                'type': 'hdrfilm',
                # Specify the desired resolution (e.g. full HD)
                'width': 1920,
                'height': 1080,
                # Use a Gaussian reconstruction filter
                'filter': { 'type': 'gaussian' }
            }
        }

The ``<film>`` plugin should be instantiated nested inside a ``<sensor>``
declaration. Note how the output filename is never specified---it is automatically
inferred from the scene filename and can be manually overridden by passing the
configuration parameter ``-o`` to the ``mitsuba`` executable when rendering
from the command line.
