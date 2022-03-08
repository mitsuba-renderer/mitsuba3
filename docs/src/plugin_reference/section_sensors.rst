.. _sec-sensors:

Sensors
=======

In Mitsuba 3, *sensors*, along with a *film*, are responsible for recording
radiance measurements in some usable format.

In the XML scene description language, a sensor declaration looks as follows:

.. tabs::
    .. code-tab:: xml

        <scene version=3.0.0>
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

    .. code-tab:: python

        'type': 'scene',

        # .. scene contents ..

        'sensor_id': {
            'type': '<sensor_type>',

            'film_id': {
                'type': '<film_type>',
                # ...
            },
            'sampler_id': {
                'type': '<sampler_type>',
                # ...
            }
        }

In other words, the ``sensor`` declaration is a child element of the ``<scene>``
(the particular position in the scene file does not play a role). Nested within
the sensor declaration is a sampler instance (see :ref:`Samplers <sec-samplers>`)
and a film instance (see :ref:`Films <sec-films>`).

Sensors in Mitsuba 3 are *right-handed*. Any number of rotations and translations
can be applied to them without changing this property. By default, they are located
at the origin and oriented in such a way that in the rendered image, :math:`+X`
points left, :math:`+Y` points upwards, and :math:`+Z` points along the viewing
direction.
Left-handed sensors are also supported. To switch the handedness, flip any one
of the axes, e.g. by passing a scale transform like ``<scale x="-1"/>`` to the
sensor's :monosp:`to_world` parameter.

.. _explanation_srf_sensor:

**Spectral sensitivity** . Furthermore, sensors can define a custom sensor
response function (:monosp:`SRF`) defined as a spectral response function
``<spectrum name="srf" filename="..."/>`` (it is possible to load a spectrum from a
file, see the :ref:`Spectrum definition <color-spectra>` section). In other words,
it can be used to focus computation into several wavelengths. Note that this
parameter only works for ``spectral`` variants.

When no spectral sensitivity is set, it fallbacks to the ``rgb`` sensitivity curves.
On the other hand, it modifies the sampling of wavelengths to one according to the
new spectral sensitivity multiplied by the ``rgb`` sensitivity curves. Notice that
while using the :ref:`High dynamic range film <film-hdrfilm>`, the output will
be a ``rgb`` image containing only information of the visible range of the spectrum.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/films/cbox_complete.png
   :caption: ``RGB`` spectral rendering
.. subfigure:: ../../resources/data/docs/images/films/srf_red.png
   :caption: ``RGB``: :monosp:`red band`
.. subfigure:: ../../resources/data/docs/images/films/srf_green.png
   :caption: ``RGB``: :monosp:`green band`
.. subfigure:: ../../resources/data/docs/images/films/srf_blue.png
   :caption: ``RGB``: :monosp:`blue band`
.. subfigend::
   :label: fig-srf_sensors

Another option is to use :ref:`Spectral Film <film-specfilm>`. By using it is
possible to define an arbitrary number of sensor sensitivities (:monosp:`SRF`)
which do not need to be constrained to the visible range of the spectrum
(``rgb`` sensitivity curves are not taken into account).
