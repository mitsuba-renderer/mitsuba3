.. _sec-textures:

Textures
========

The following section describes the available texture data sources. In Mitsuba 3,
textures are objects that can be attached to certain surface scattering model
parameters to introduce spatial variation. In the documentation, these are listed
as supporting the :paramtype:`texture` type. See the last sections about
:ref:`BSDFs <sec-bsdfs>` for many examples.

Textures take an (optional) ``<transform>`` called :paramtype:`to_uv` which can
be used to translate, scale, or rotate the lookup into the texture accordingly.

An example in XML looks the following:

.. tabs::
    .. code-tab:: xml

        <scene version=3.0.0>
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

    .. code-tab:: python

        'type': 'scene',

        # .. scene contents ..

        # Create a BSDF that supports textured parameters
        'my_textured_material': {
            'type': '<bsdf_type>',
            '<parameter_name>' : {
                'type': '<texture_type>':
                # .. texture parameters ..
                'to_uv': mi.scalar_rgb.ScalarTransform4f.scale([2, 2, 0]).translate([0.5, 1.0, 0]) # Third dimension is ignored
            }

            # .. non-spatially varying BSDF parameters ..
        }

Similar to BSDFs, named textures can alternatively defined at the top level of the scene
and later referenced. This is particularly useful if the same texture would be loaded
many times otherwise.

.. tabs::
    .. code-tab:: xml

        <scene version=3.0.0>
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

    .. code-tab:: python

        'type': 'scene',

        # .. scene contents ..

        'texture_id': {
            'type': '<texture_type>':
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



