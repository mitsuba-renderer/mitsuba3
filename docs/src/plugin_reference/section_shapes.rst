.. _sec-shapes:

Shapes
======

This section presents an overview of the shape plugins that are released along with the renderer.

In Mitsuba 3, shapes define surfaces that mark transitions between different types of materials. For
instance, a shape could describe a boundary between air and a solid object, such as a piece of rock.
Alternatively, a shape can mark the beginning of a region of space that isn’t solid at all, but
rather contains a participating medium, such as smoke or steam. Finally, a shape can be used to
create an object that emits light on its own.

Shapes are usually declared along with a surface scattering model named *BSDF* (see the :ref:`respective section <sec-bsdfs>`). This BSDF characterizes what happens at the surface. In the XML scene description language, this might look like the following:

.. tabs::
    .. code-tab:: xml

        <scene version=3.0.0>
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
            'bsdf_id' : {
                'type' : 'ref',
                'id' : 'some_bsdf_id'
            }
        }

The following subsections discuss the available shape types in greater detail.