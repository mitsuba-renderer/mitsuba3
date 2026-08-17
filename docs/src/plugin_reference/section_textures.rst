.. _sec-textures:

Textures
========

The following section describes the available texture data sources. In misuka,
textures are objects that can be attached to certain surface scattering model
parameters to introduce spatial variation. In the documentation, these are listed
as supporting the :paramtype:`texture` type.

.. note::

    Textures are inherited from Mitsuba unchanged, but they play a much smaller role
    acoustically. Absorption and scattering coefficients are usually understood as properties of
    a *material* rather than of an individual surface point, so they tend not to vary across a
    surface. The :ref:`acoustic material <bsdf-acousticbsdf>` varies them over *frequency*
    instead, which is why its parameters are usually given as
    :ref:`frequency spectra <sec-spectra-acoustic>`. Use a texture when a single surface
    genuinely needs spatially varying acoustic properties.

Textures take an (optional) ``<transform>`` called :paramtype:`to_uv` which can
be used to translate, scale, or rotate the lookup into the texture accordingly.

An example looks as follows:

.. tabs::
    .. code-tab:: python

        'type': 'scene',

        # .. scene contents ..

        # Create a BSDF that supports textured parameters
        'my_textured_material': {
            'type': '<bsdf_type>',
            '<parameter_name>' : {
                'type': '<texture_type>',
                # .. texture parameters ..
                'to_uv': mi.scalar_rgb.ScalarTransform4f.scale([2, 2, 0]).translate([0.5, 1.0, 0]) # Third dimension is ignored
            }

            # .. non-spatially varying BSDF parameters ..
        }

    .. code-tab:: xml

        <scene version="3.0.0">
            <!-- Create a BSDF that supports textured parameters -->
            <bsdf type=".. BSDF type .." id="my_textured_material">
                <texture type=".. texture type .." name=".. parameter name ..">
                    <!-- .. Texture parameters go here .. -->

                    <transform name="to_uv">
                        <!-- Scale texture by factor of 2 -->
                        <scale x="2" y="2"/>
                        <!-- Offset texture by [0.5, 1.0] -->
                        <translate x="0.5" y="1.0"/>
                    </transform>
                </texture>

                <!-- .. Non-spatially varying BSDF parameters ..-->
            </bsdf>
        </scene>

Similar to BSDFs, named textures can alternatively be defined at the top level of the scene
and later referenced. This is particularly useful if the same texture would be loaded
many times otherwise.

.. tabs::
    .. code-tab:: python

        'type': 'scene',

        # .. scene contents ..

        'texture_id': {
            'type': '<texture_type>',
            # .. texture parameters ..
        },

        # Create a BSDF that supports textured parameters
        'my_textured_material': {
            'type': '<bsdf_type>',
            '<parameter_name>' : {
                'type' : 'ref',
                'id' : 'texture_id'
            }

            # .. non-spatially varying BSDF parameters ..
        }

    .. code-tab:: xml

        <scene version="3.0.0">
            <!-- Create a named texture at the top level -->
            <texture type=".. texture type .." id="my_named_texture">
                <!-- .. Texture parameters go here .. -->
            </texture>

            <!-- Create a BSDF that supports textured parameters -->
            <bsdf type=".. BSDF type ..">
                <!-- Example of referencing a named texture -->
                <ref id="my_named_texture" name=".. parameter name .."/>

                <!-- .. Non-spatially varying BSDF parameters ..-->
            </bsdf>
        </scene>



