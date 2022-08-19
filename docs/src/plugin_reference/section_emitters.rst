.. _sec-emitters:

Emitters
========

    .. image:: ../../resources/data/docs/images/emitter/emitter_overview.jpg
        :width: 70%
        :align: center

    Schematic overview of the emitters in 3. The arrows indicate
    the directional distribution of light.

Mitsuba 3 supports a number of different emitters/light sources, which can be
classified into two main categories: emitters which are located somewhere within the scene, and emitters that surround the scene to simulate a distant environment.

Generally, light sources are specified as children of the ``<scene>`` element; for instance,
the following snippet instantiates a point light emitter that illuminates a sphere:

.. tabs::
    .. code-tab:: xml

        <scene version=3.0.0>
            <!-- .. scene contents .. -->

            <emitter type="point">
                <rgb name="intensity" value="1"/>
                <point name="position" x="0" y="0" z="-2"/>
            </emitter>

            <shape type="sphere"/>
        </scene>

    .. code-tab:: python

        'type': 'scene',

        # .. scene contents ..

        'emitter_id': {
            'type': 'point'
            'position': [0, 0, -2],
            'intensity': {
                'type': 'spectrum',
                'value': 1.0,
            }
        },

        'shape_id': {
            'type': 'sphere'
        }

An exception to this are area lights, which turn a geometric object into a light source.
These are specified as children of the corresponding ``<shape>`` element:

.. tabs::
    .. code-tab:: xml

        <scene version=3.0.0>
            <!-- .. scene contents .. -->

            <shape type="sphere">
                <emitter type="area">
                    <rgb name="radiance" value="1"/>
                </emitter>
            </shape>
        </scene>

    .. code-tab:: python

        'type': 'scene',

        # .. scene contents ..

        'type': 'sphere',
        'emitter': {
            'type': 'area',
            'radiance': {
                'type': 'rgb',
                'value': 1.0,
            }
        }

