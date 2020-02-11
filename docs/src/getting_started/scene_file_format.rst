Scene file format
=================

Mitsuba uses simple XML-based format to represent scenes. As shown in the following XML snippet a scene file should always start with the `scene` tag.

.. code-block:: xml

    <scene version="2.0.0">
        <!-- ... -->
    </scene>

The `version` attribute denotes the release of Mitsuba that was used to create the scene. This information allows mitsuba to always correctly process the file irregardless of any potential future changes in the scene description language.


Mitsuba Object
--------------

The scene file describes a set of instances of plugins (*objects*) and how they should interface with one another. These *objects* are allowed to be nested within each other and optionally accept *properties*, which further characterize their behavior. All objects except for the root object (the `scene`) cause the renderer to search and load a plugin from disk, hense you must provide the plugin name using :code:`type=".."` parameter.

.. figtable::
    :label: table-xml-objects
    :caption: This table lists the different kind of *objects* and their respective tags. It also provides a non-exhaustive list of the available plugins associated to each tag.

    .. list-table::
        :widths: 17 53 30
        :header-rows: 1

        * - XML tag
          - Description
          - `type` examples
        * - `shape`
          - Shape plugins which define surfaces that mark transitions between different types of materials
          - `obj`, `ply`, `serialized`
        * - `bsdf`
          - BSDF descibe the manner in which light interacts with surfaces in the scene. Also sometimes refered to as *material*
          - `diffuse`, `conductor`
        * - `emitter`
          - Light sources plugins.
          - `constant`, `envmap`, `point`
        * - `sensor`
          - Object responsible for recording radiance measurements
          - `perspective`, `orthogonal`
        * - `film`
          - Film plugins defines how conducted measurements are stored and converted into the final output file that is written to disk
          - `hdrfilm`
        * - `sampler`
          - Sample generator plugins used by the `integrator`
          - `independent`
        * - `integrator`
          - Integrators represent a different rendering techniques for solving the light transport equation
          - `path`, `direct`, `depth`
        * - `texture`
          - Texture plugins can be used to represent spatially varying signal
          - `bitmap`
        * - `rfilter`
          - Image reconstruction filter plugin used by the `film`
          - `box`, `gaussian`

The following XML snippet describes a simple scene with a single mesh. An instance of the :ref:`shape-obj` plugin will be used to represent the shape, and a :ref:`bsdf-diffuse` BSDF will be attach to it.

.. code-block:: xml

    <scene version="2.0.0">
        <shape type="obj">
            <string name="filename" value="dragon.obj"/>

            <bsdf type="diffuse">
                <rgb name="reflectance" value="0.8 0.0 0.0"/>
            </bsdf>
        </shape>
    </scene>


Property type
-------------

Different type of properties can be supplied to objects. Each property must define a `name` attribute which denotes the name of a property expected by the plugin. Section :ref:`plugins` describes which properties and their respective name a certain plugin accepts.

Numbers
*******

Integer and floating point values can be passed as follows:

.. code-block:: xml

    <integer name="int_property" value="1234"/>
    <float name="float_property" value="-1.5e3"/>

Booleans
********

Boolean values can be passed as follows:

.. code-block:: xml

    <boolean name="bool_property" value="true"/>

Strings
*******

Strings can be passed in a similar way:

.. code-block:: xml

    <string name="string_property" value="This is a string"/>


RGB color values
****************

.. todo:: Write this section

Color spectra
****************

.. todo::  Write this section

Vectors, Positions
******************

Points and vectors can be specified as follows:

.. code-block:: xml

    <point name="point_property" x="3" y="4" z="5"/>
    <vector name="vector_property" x="3" y="4" z="5"/>

.. note:: It is important that whatever you choose as world-space units (meters, inches, etc.) is used consistently in all places.

Transformations
***************

Transformations are the only kind of property that require more than a single tag. The idea is that starting with the identity, one can build up a transformation using a sequence of commands. For instance a transformation that does a translation followed by a rotation might be written like this:

.. code-block:: xml

    <transform name="trafo_property">
        <translate x="-1" y="3" z="4"/>
        <rotate y="1" angle="45"/>
    </transform>

Mathematically, each incremental transformation in the sequence is left-multiplied onto the current one. The following choices are available:

* Translations:

    .. code-block:: xml

        <translate x="-1" y="3" z="4"/>

* Counter-clockwise rotations around a specified axis. The angle is given in degrees:

    .. code-block:: xml

        <rotate x="0.701" y="0.701" z="0" angle="180"/>

* Scaling operation. The coefficients may also be negative to obtain a flip:

    .. code-block:: xml

        <scale value="5"/> <!-- uniform scale -->
        <scale x="2" y="1" z="-1"/> <!-- non-unform scale -->

* Explicit 4x4 matrices:

    .. code-block:: xml

        <matrix value="0 -0.53 0 -1.79 0.92 0 0 8.03 0 0 0.53 0 0 0 0 1"/>

* `lookat` transformations -- this is primarily useful for setting up cameras. The `origin` coordinates specify the camera origin, `target` is the point that the camera will look at, and the (optional) `up` parameter determines the *upward* direction in the filnal rendered image.

    .. code-block:: xml

        <lookat origin="10, 50, -800" target="0, 0, 0" up="0, 1, 0"/>


References
----------

Quite often, you will find yourself using a object (such as a material) in many places. To avoid having to declare it over and over again, which wastes memory,, you can make use of references. Here is an example of how this works:

.. code-block:: xml

    <scene version="2.0.0">
        <texture type="bitmap" id="my_image">
            <string name="filename" value="textures/my_image.jpg"/>
        </texture>

        <bsdf type="diffuse" id="my_material">
            <!-- Reference the texture named my_image and pass it
                 to the BRDF as the reflectance parameter -->
            <ref name="reflectance" id="my_image"/>
        </bsdf>

        <shape type="obj">
            <string name="filename" value="meshes/my_shape.obj"/>
            <!-- Reference the material named my_material -->
            <ref id="my_material"/>
        </shape>
    </scene>

By providing a unique `id` attribute in the object declaration, the object is bound to that identifier upon instantiation. Referencing this identifier at a later point (using the :code:`<ref id=".."/>` tag) will add the instance to the parent object.

.. note:: Note that while this feature is meant to efficiently handle materials, textures and particiapating media that are referenced from multiple places, it cannot be used to instantiate geometry. (an `instance` plugin should be release for that purpose in an upcoming version the framework).


Include
-------

A scene can be split into multiple pieces for better readability. To include an external file, please ue the following command:

.. code-block:: xml

    <include filename="nested-scene.xml"/>

In this case, the file :code:`nested-scene.xml` must be a proper scene file with a :code:`<scene>` tag at the root.

This feature is sometimes very convenient in conjunction with the :code:`-D key=value` flag of the `mitsuba` command line renderer. This lets you include different parts of a scene configuration by changing the command line parameters (and without having to touch the XML file):

.. code-block:: xml

    <include filename="nested-scene-$version.xml"/>


.. _sec-scene-file-format-params:

Default parameters
------------------

Scene may contain named parameters that are supplied via the command line:

.. code-block:: xml

    <bsdf type="diffuse">
        <rgb name="reflectance" value="$reflectance"/>
    </bsdf>

In this case, an error will occur when loading the scene without an explicit command line argument of the form :code:`-Dreflectanc=something`. For convenience, it is possible to specify a default parameter value that take precedence when no command line arguments are given. The syntax for this is:

.. code-block:: xml

    <default name="reflectance" value="something"/>

and must precede the occurrences of the parameter in the XML file.


Aliases
-------

Sometimes, it can be useful to associate an object with multiple identifiers. This can be accomplished using the :code:`alias as=".."` keyword:

.. code-block:: xml

    <bsdf type="diffuse" id="my_material_1"/>
    <alias id="my_material_1" as="my_material_2"/>

After this statement, the diffuse scattering model will be bound to *both* identifiers :code:`my_material_1` and :code:`my_material_2`.


Relative path and FileResolver
------------------------------

.. todo:: Write this section