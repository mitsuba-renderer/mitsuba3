.. _sec-emitters:

Emitters
========

    .. image:: ../../resources/data/docs/images/emitter/emitter_overview.jpg
        :width: 70%
        :align: center

    Schematic overview of the emitters in Mitsuba 2. The arrows indicate
    the directional distribution of light.

Mitsuba 2 supports a number of different emitters/light sources, which can be
classified into two main categories: emitters which are located somewhere within the scene, and emitters that surround the scene to simulate a distant environment.

Generally, light sources are specified as children of the ``<scene>`` element; for instance,
the following snippet instantiates a point light emitter that illuminates a sphere:

.. code-block:: xml

    <scene version="2.0.0">
        <emitter type="point">
            <spectrum name="intensity" value="1"/>
            <point name="position" x="0" y="0" z="-2"/>
        </emitter>

        <shape type="sphere"/>
    </scene>

An exception to this are area lights, which turn a geometric object into a light source.
These are specified as children of the corresponding ``<shape>`` element:

.. code-block:: xml

    <scene version="2.0.0">
        <shape type="sphere">
            <emitter type="area">
                <spectrum name="radiance" value="1"/>
            </emitter>
        </shape>
    </scene>
