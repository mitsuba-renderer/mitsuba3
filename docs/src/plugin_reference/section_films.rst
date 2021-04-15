.. _sec-films:

Films
=====

A film defines how conducted measurements are stored and converted into the final
output file that is written to disk at the end of the rendering process.

In the XML scene description language, a normal film configuration might look
as follows:

.. code-block:: xml

    <scene version=2.0.0>
        <!-- .. scene contents -->

        <sensor type=".. sensor type ..">
            <!-- .. sensor parameters .. -->

            <!-- Write to a high dynamic range EXR image -->
            <film type="hdrfilm">
                <!-- Specify the desired resolution (e.g. full HD) -->
                <integer name="width" value="1920"/>
                <integer name="height" value="1080"/>

                <!-- Use a Gaussian reconstruction filter. For details
                     on these, refor to the next subsection -->
                <rfilter type="gaussian"/>
            </film>
        </sensor>
    </scene>

The ``<film>`` plugin should be instantiated nested inside a ``<sensor>``
declaration. Note how the output filename is never specified---it is automatically
inferred from the scene filename and can be manually overridden by passing the
configuration parameter ``-o`` to the ``mitsuba`` executable when rendering
from the command line.
