.. _sec-sensors:

Sensors
=======

In Mitsuba 2, *sensors*, along with a *film*, are responsible for recording
radiance measurements in some usable format.

In the XML scene description language, a sensor declaration looks as follows:

.. code-block:: xml

    <scene version=2.0.0>
        <!-- .. scene contents .. -->

        <sensor type=".. sensor type ..">
            <!-- .. sensor parameters .. -->

            <sampler type=".. sampler type ..">
                <!-- .. sampler parameters .. -->
            </samplers>

            <film type=".. film type ..">
                <!-- .. film parameters .. -->
            </film>
        </sensor>
    </scene>

In other words, the ``sensor`` declaration is a child element of the ``<scene>``
(the particular position in the scene file does not play a role). Nested within
the sensor declaration is a sampler instance (see :ref:`Samplers <sec-samplers>`)
and a film instance (see :ref:`Films <sec-films>`).

Sensors in Mitsuba 2 are *right-handed*. Any number of rotations and translations
can be applied to them without changing this property. By default, they are located
at the origin and oriented in such a way that in the rendered image, :math:`+X`
points left, :math:`+Y` points upwards, and :math:`+Z` points along the viewing
direction.
Left-handed sensors are also supported. To switch the handedness, flip any one
of the axes, e.g. by passing a scale transform like ``<scale x="-1"/>`` to the
sensor's :monosp:`to_world` parameter.