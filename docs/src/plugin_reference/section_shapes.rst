.. _sec-shapes:

Shapes
======

This section presents an overview of the shape plugins that are released along with the renderer.
They are inherited from Mitsuba 3 and are used for acoustic scenes unchanged.

In misuka, shapes define surfaces that mark transitions between different types of materials. For
instance, a shape could describe a boundary between air and a solid object, such as a wall or a
piece of furniture. A shape can also be used to create an object that emits on its own, which is how
sound sources are built (see :ref:`Emitters <sec-emitters>`).

.. note::

    Every shape type below is compatible with acoustic rendering. What differs is how much
    geometric detail is useful. Acoustic scenes should contain only *macro*-geometry, meaning
    geometry larger than the longest simulated wavelength. Detail at or below the wavelength
    scale is reflected geometrically instead of producing the diffraction and scattering it
    would cause in reality, which gives wrong results, so it must be modeled through the
    :ref:`material's BSDF <bsdf-acousticbsdf>` instead of the scene geometry. See
    :ref:`sec-acoustic-rendering` for the reasoning.

Shapes are usually declared along with a surface scattering model named *BSDF* (see the :ref:`respective section <sec-bsdfs>`). This BSDF characterizes what happens at the surface, and might look like the following:

.. tabs::
    .. code-tab:: python

        'type': 'scene',

        # .. scene contents ..

        'shape_id': {
            'type': '<shape_type>',
            'bsdf_id': {
                'type': '<bsdf_type>',
                # .. bsdf parameters ..
            }

            # Alternatively, reference a named BSDF that had been declared previously
            # 'bsdf_id' : {
            #     'type' : 'ref',
            #     'id' : 'some_bsdf_id'
            # }
        }

    .. code-tab:: xml

        <scene version="3.0.0">
            <!-- .. scene contents .. -->

            <shape type=".. shape type ..">
                .. shape parameters ..

                <bsdf type=".. BSDF type ..">
                    .. bsdf parameters ..
                </bsdf>

                <!-- Alternatively: reference a named BSDF that
                    has been declared previously

                    <ref id="my_bsdf"/>
                -->
            </shape>
        </scene>

The following subsections discuss the available shape types in greater detail.