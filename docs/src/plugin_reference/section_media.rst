.. _sec-media:

Participating media
====================

In Mitsuba, participating media are used to simulate materials ranging from fog,
smoke, and clouds, over translucent materials such as skin or milk, to “fuzzy”
structured substances such as woven or knitted cloth. This section describes the
two available types of media (homogeneous and heterogeneous). In practice,
these will be combined with a :ref:`phase function <sec-phasefunctions>`.

Participating media are usually attached to shapes in the scene.
When a shape marks the transition to a participating medium, it is necessary to provide
information about the two media that lie at the interior and exterior of the shape.
This informs the renderer about what happens in the region of space surrounding the surface.
In many practical use cases it is sufficient to only specify an interior medium and to
assume the exterior medium (e.g., air) to not influence the light transport.

.. tabs::
    .. code-tab:: xml

        <scene version=3.0.0>
            <shape type=".. shape type ..">
                .. shape parameters ..

                <medium name="interior" type="... medium type ...">
                    ... medium parameters ...
                </medium>
                <medium name="exterior" type="... medium type ...">
                    ... medium parameters ...
                </medium>
                <!-- Alternatively: reference named media that
                    have been declared previously
                    <ref name="interior" id="myMedium1"/>
                    <ref name="exterior" id="myMedium2"/>
                -->
            </shape>
        </scene>

    .. code-tab:: python

        'type': 'scene',
        'shape_id': {
            'type': '<shape_type>',
            # .. shape parameters ..

            'interior': {
                'type': '<medium_type>',
                # .. medium parameters ..
            },
            'exterior': {
                'type': '<medium_type>',
                # .. medium parameters ..
            }
        }


When a medium permeates a volume of space (e.g. fog) that includes sensors,
it is important to assign the medium to them. This can be done using
the referencing mechanism:

.. tabs::
    .. code-tab:: xml

        <scene version=3.0.0>
            <!-- .. scene contents .. -->

            <medium type="homogeneous" id="fog">
                <!-- .. homogeneous medium parameters .. -->
            </medium>
            <sensor type="perspective">
                <!-- .. perspective camera parameters .. -->
                <!-- Reference the fog medium from within the sensor declaration
                    to make it aware that it is embedded inside this medium -->
                <ref id="fog"/>
            </sensor>
        </scene>

    .. code-tab:: python

        'type': 'scene',

        # .. scene contents ..

        'fog': {
            'type': 'homogeneous',
            # .. homogeneous medium parameters ..
        },

        'sensor_id': {
            'type': 'perspective',
            # .. perspective camera parameters ..

            # Reference the fog medium
            'medium' : {
                'type' : 'ref',
                'id' : 'fog'
            }
        }


