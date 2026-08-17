.. _sec-sensors:

Sensors
=======

*Sensors*, along with a *film*, are responsible for recording measurements in
some usable format. misuka's acoustic sensor is :ref:`microphone
<sensor-microphone>`, which records incoming sound energy into a
:ref:`tape film <sec-films>` (see :ref:`Films <sec-films>` for the resulting
ETC output format).

A sensor declaration looks as follows:

.. tabs::
    .. code-tab:: python

        'type': 'scene',

        # .. scene contents ..

        'sensor_id': {
            'type': 'microphone',

            'film_id': {
                'type': 'tape',
                # ...
            },
            'sampler_id': {
                'type': '<sampler_type>',
                # ...
            }
        }

    .. code-tab:: xml

        <scene version="3.0.0">
            <!-- .. scene contents .. -->

            <sensor type="microphone">
                <!-- .. sensor parameters .. -->

                <sampler type=".. sampler type ..">
                    <!-- .. sampler parameters .. -->
                </sampler>

                <film type="tape">
                    <!-- .. film parameters .. -->
                </film>
            </sensor>
        </scene>

In other words, the ``sensor`` declaration is a child element of the ``<scene>``
(the particular position in the scene file does not play a role). Nested within
the sensor declaration is a sampler instance (see :ref:`Samplers <sec-samplers>`,
defaulting to ``independent`` if omitted) and a film instance (see
:ref:`Films <sec-films>`).
