.. _sec-file-format:

Scene file format
=================

Mitsuba uses a simple and general XML-based format to represent scenes. Since
the framework's philosophy is to represent discrete blocks of functionality as
plugins, a scene file can be interpreted as "recipe" specifying which plugins
should be instantiated and how they should be put together. In the following,
we will look at a few examples to get a feeling for the scope of the format.

A simple scene with a single mesh and the default lighting and camera setup
might look something like this:

.. code-block:: xml

    <scene version="2.0.0">
        <shape type="obj">
            <string name="filename" value="dragon.obj"/>
        </shape>
    </scene>

The ``version`` attribute in the first line denotes the release of Mitsuba that
was used to create the scene. This information allows Mitsuba to correctly
process the file regardless of any potential future changes in the scene
description language.

This example already contains the most important things to know about format:
it consists of *objects* (such as the objects instantiated by the ``<scene>`` or
``<shape>`` tags), which can furthermore be nested within each other. Each object
optionally accepts *properties* (such as the ``<string>`` tag) that characterize
its behavior. All objects except for the root object (the ``<scene>``) cause the
renderer to search and load a plugin from disk, hence you must provide the
plugin name using ``type=".."`` parameter.

The object tags also let the renderer know *what kind* of object is to be
instantiated: for instance, any plugin loaded using the ``<shape>`` tag must
conform to the *Shape* interface, which is certainly the case for the plugin
named ``obj`` (it contains a :ref:`Wavefront OBJ loader <shape-obj>`).
Similarly, you could write

.. code-block:: xml

    <scene version="2.0.0">
        <shape type="sphere">
            <float name="radius" value="10"/>
        </shape>
    </scene>

This loads a different plugin (``sphere``) which is still a *Shape* but instead
represents a :ref:`sphere <shape-sphere>` configured with a radius of 10
world-space units. Mitsuba ships with a large number of plugins; please refer
to the next chapter for a detailed overview of them.

The most common scene setup is to declare an integrator, some geometry, a
sensor (e.g. a camera), a film, a sampler and one or more emitters. Here is a
more complex example:

.. code-block:: xml

    <scene version="2.0.0">
        <integrator type="path">
            <!-- Instantiate a path tracer with a max. path length of 8 -->
            <integer name="max_depth" value="8"/>
        </integrator>

        <!-- Instantiate a perspective camera with 45 degrees field of view -->
        <sensor type="perspective">
            <!-- Rotate the camera around the Y axis by 180 degrees -->
            <transform name="to_world">
                <rotate y="1" angle="180"/>
            </transform>
            <float name="fov" value="45"/>

            <!-- Render with 32 samples per pixel using a basic
                 independent sampling strategy -->
            <sampler type="independent">
                <integer name="sample_count" value="32"/>
            </sampler>

            <!-- Generate an EXR image at HD resolution -->
            <film type="hdrfilm">
                <integer name="width" value="1920"/>
                <integer name="height" value="1080"/>
            </film>
        </sensor>

        <!-- Add a dragon mesh made of rough glass (stored as OBJ file) -->
        <shape type="obj">
            <string name="filename" value="dragon.obj"/>

            <bsdf type="roughdielectric">
                <!-- Tweak the roughness parameter of the material -->
                <float name="alpha" value="0.01"/>
            </bsdf>
        </shape>

        <!-- Add another mesh, this time, stored using Mitsuba's own
             (compact) binary representation -->
        <shape type="serialized">
            <string name="filename" value="lightsource.serialized"/>
            <transform name="toWorld">
                <translate x="5" y="-3" z="1"/>
            </transform>

            <!-- This mesh is an area emitter -->
            <emitter type="area">
                <rgb name="radiance" value="100,400,100"/>
            </emitter>
        </shape>
    </scene>

This example introduces several new object types (``integrator``, ``sensor``,
``bsdf``, ``sampler``, ``film``, and``emitter``) and property types
(``integer``, ``transform``, and ``rgb``). As you can see in the example,
objects are usually declared at the top level except if there is some inherent
relation that links them to another object. For instance, BSDFs are usually
specific to a certain geometric object, so they appear as a child object of a
shape. Similarly, the sampler and film affect the way in which rays are
generated from the sensor and how it records the resulting radiance samples,
hence they are nested inside it. The following table provides an overview of
the available object types:

.. figtable::
    :label: table-xml-objects
    :caption: This table lists the different kind of *objects* and their respective tags. It also provides an exemplary list of plugins for each category.

    .. list-table::
        :widths: 17 53 30
        :header-rows: 1

        * - XML tag
          - Description
          - `type` examples
        * - `bsdf`
          - BSDF describe the manner in which light interacts with surfaces in the scene (i.e., the *material*)
          - `diffuse`, `conductor`
        * - `emitter`
          - Emitter plugins specify light sources and their characteristic emission profiles.
          - `constant`, `envmap`, `point`
        * - `film`
          - Film plugins convert measurements into the final output file that is written to disk
          - `hdrfilm`
        * - `integrator`
          - Integrators implement rendering techniques for solving the light transport equation
          - `path`, `direct`, `depth`
        * - `rfilter`
          - Reconstruction filters control how the `film` converts a set of samples into the output image
          - `box`, `gaussian`
        * - `sampler`
          - Sample generator plugins used by the `integrator`
          - `independent`
        * - `sensor`
          - Sensor plugins like cameras are responsible for measuring radiance
          - `perspective`, `orthogonal`
        * - `shape`
          - Shape puglins define surfaces that mark transitions between different types of materials
          - `obj`, `ply`, `serialized`
        * - `texture`
          - Texture plugins represent spatially varying signals on surfaces
          - `bitmap`


Properties
----------

This subsection documents all of the ways in which properties can be supplied
to objects. If you are more interested in knowing which properties a certain
plugin accepts, you should look at the :ref:`plugin documentation
<sec-plugins>` instead.

Numbers
*******

Integer and floating point values can be passed as follows:

.. code-block:: xml

    <integer name="int_property" value="1234"/>
    <float name="float_property" value="-1.5e3"/>

Note that you must adhere to the format expected by the object, i.e. you can't
pass an integer property to an object that expects a floating-point property
associated with that name.

Booleans
********

Boolean values can be passed as follows:

.. code-block:: xml

    <boolean name="bool_property" value="true"/>

Strings
*******

Passing strings is similarly straightforward:

.. code-block:: xml

    <string name="string_property" value="This is a string"/>

Vectors, Positions
******************

Points and vectors can be specified as follows:

.. code-block:: xml

    <point name="point_property" value="3, 4, 5"/>
    <vector name="vector_property" value="3, 4, 5"/>

.. note::

    Mitsuba does not dictate a specific unit for position values (meters,
    centimeters, inches, etc.). The only requirement is that you consistently
    use one convention throughout the scene specification.

RGB Colors
**********

In Mitsuba, colors are either specified using the ``<rgb>`` or ``<spectrum>`` tags.
The interpretation of a RGB color value like

.. code-block:: xml

    <rgb name="color_property" value="0.2, 0.8, 0.4"/>

depends on the variant of the renderer that is currently active. For instance,
``scalar_rgb`` will simply forward the color value to the underlying plugin
without changes. In contrast, ``scalar_spectral`` operates in the spectral
domain where a RGB value is not meaningful---worse, there is an infinite set of
spectra corresponding to each RGB color. Mitsuba uses the method of Jakob and
Hanika :cite:`Jakob2019Spectral` to choose a plausible smooth spectrum amongst
all of these these possibilities. An example is shown below:

.. image:: ../../../resources/data/docs/images/variants/upsampling.jpg
  :width: 100%
  :align: center


Color spectra
*************

A more accurate way or specifying color information involves the ``<spectrum>``
tag, which records a reflectance/intensity value for multiple discrete
wavelengths specified in *nanometers*.

.. code-block:: xml

    <spectrum name="color_property" value="400:0.56, 500:0.18, 600:0.58, 700:0.24"/>

The resulting spectrum uses linearly interpolation for in-between wavelengths
and equals zero outside of the specified wavelength range. The following short-hand
notation creates a spectrum that is uniform across wavelengths:

.. code-block:: xml

    <spectrum name="color_property" value="0.5"/>

When spectral power or reflectance distributions are obtained from measurements
(e.g. at 10nm intervals), they are usually quite unwieldy and can clutter the
scene description. For this reason, there is yet another way to pass a spectrum
by loading it from an external file:

.. code-block:: xml

    <spectrum name="color_property" filename="measured_spectrum.spd"/>

The file should contain a single measurement per line, with the corresponding
wavelength in nanometers and the measured value separated by a space. Comments
are allowed. Here is an example:

.. code-block:: text

    # This file contains a measured spectral power/reflectance distribution
    406.13 0.703313
    413.88 0.744563
    422.03 0.791625
    430.62 0.822125
    435.09 0.834000
    ...

For more details regarding spectral information in Mitsuba 2, please have a look
at the :ref:`corresponding section <sec-spectra>` in the plugin documentation.

Transformations
***************

Transformations are the only kind of property that require more than a single
tag. The idea is that, starting with the identity, one can build up a
transformation using a sequence of commands. For instance, a transformation
that does a translation followed by a rotation might be written like this:

.. code-block:: xml

    <transform name="trafo_property">
        <translate value="-1, 3, 4"/>
        <rotate y="1" angle="45"/>
    </transform>


Mathematically, each incremental transformation in the sequence is
left-multiplied onto the current one. The following choices are available:

* Translations:

  .. code-block:: xml

      <translate value="-1, 3, 4"/>

* Counter-clockwise rotations around a specified axis. The angle is given in degrees:

  .. code-block:: xml

      <rotate value="0.701, 0.701, 0" angle="180"/>

* Scaling operation. The coefficients may also be negative to obtain a flip:

  .. code-block:: xml

      <scale value="5"/>        <!-- uniform scale -->
      <scale value="2, 1, -1"/> <!-- non-uniform scale -->

* Explicit 4x4 matrices in row-major order:

  .. code-block:: xml

      <matrix value="0 -0.53 0 -1.79 0.92 0 0 8.03 0 0 0.53 0 0 0 0 1"/>

* Explicit 3x3 matrices in row-major order. Internally, this will be converted to a 4x4 matrix with the same last row and column as the identity matrix.

  .. code-block:: xml

      <matrix value="0.57 0.2 0 0.1 -1 0 0 0 1"/>


* `lookat` transformations -- this is primarily useful for setting up cameras. The `origin` coordinates specify the camera origin, `target` is the point that the camera will look at, and the (optional) `up` parameter determines the *upward* direction in the final rendered image.

  .. code-block:: xml

      <lookat origin="10, 50, -800" target="0, 0, 0" up="0, 1, 0"/>

References
----------

Quite often, you will find yourself using an object (such as a material) in
many places. To avoid having to declare it over and over again, which wastes
memory, you can make use of references. Here is an example of how this works:

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

By providing a unique `id` attribute in the object declaration, the object is
bound to that identifier upon instantiation. Referencing this identifier at a
later point (using the ``<ref id=".."/>`` tag) will add the instance to the
parent object.

.. note::

    Note that while this feature is meant to efficiently handle materials,
    textures, and particiapating media that are referenced from multiple
    places, it cannot be used to instantiate geometry. (the `instance` plugin
    should be used for that purpose. This is not yet part of Mitsuba 2
    but will be added at a later point.)

.. _sec-scene-file-format-params:

Default parameters
------------------

Scene may contain named parameters that are supplied via the command line:

.. code-block:: xml

    <bsdf type="diffuse">
        <rgb name="reflectance" value="$reflectance"/>
    </bsdf>

In this case, an error will be raised when the scene is loaded without an
explicit command line argument of the form ``-Dreflectance=...``. For
convenience, it is possible to specify a default parameter value that take
precedence when no command line arguments are given. The syntax for this is:

.. code-block:: xml

    <default name="reflectance" value="something"/>

and must precede the occurrences of the parameter in the XML file.

Including external files
------------------------

A scene can be split into multiple pieces for better readability. To include an
external file, please use the following command:

.. code-block:: xml

    <include filename="nested-scene.xml"/>

In this case, the file ``nested-scene.xml`` must be a proper scene file with a
``<scene>`` tag at the root.

This feature is often very convenient in conjunction with the ``-D key=value``
flag of the ``mitsuba`` command line renderer. This enables including different
variants of a scene configuration by changing the command line parameters,
without without having to touch the XML file:

.. code-block:: xml

    <include filename="nested-scene-$version.xml"/>

Aliases
-------

It is sometimes useful to associate an object with multiple identifiers. This
can be accomplished using the ``alias as=".."`` tag:

.. code-block:: xml

    <bsdf type="diffuse" id="my_material_1"/>
    <alias id="my_material_1" as="my_material_2"/>

After this statement, the diffuse scattering model will be bound to *both*
identifiers ``my_material_1`` and ``my_material_2``.

External resource folders
-------------------------

Using the ``path`` tag, it is possible to add a path to the list of search paths. This can
be useful for instance when some meshes and textures are stored in a different directory, (e.g. when
shared with other scenes). If the path is a relative path, Mitsuba 2 will first try to interpret it
relative to the scene directory, then to other paths that are already on the search path (e.g. added
using the ``-a <path1>;<path2>;..`` command line argument).

.. code-block:: xml

    <path value="../../my_resources"/>