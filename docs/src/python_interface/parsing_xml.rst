Parsing XML code
=================

Mitsuba provides three functions for creating scenes and objects in Python:

- :py:func:`mitsuba.core.xml.load_file`: load files on disk
- :py:func:`mitsuba.core.xml.load_string`: load arbitrary strings
- :py:func:`mitsuba.core.xml.load_dict`: load from Python dictionary (`dict`)

Please refer to this :ref:`chapter <sec-file-format>` for more information regarding the Mitsuba's
XML scene description language.

Loading a scene
---------------

Here is a complete Python example on how to load a Mitsuba scene from an XML
file:

.. code-block:: python

    import os
    import mitsuba
    mitsuba.set_variant('scalar_rgb')
    from mitsuba.core import Thread
    from mitsuba.core.xml import load_file

    # Absolute or relative path to the XML file
    filename = 'path/to/my/scene.xml'

    # Load the scene for an XML file
    scene = load_file(filename)

Because the scene may reference external resources like meshes and textures
using relative paths specified in the XML file, Mitsuba must be informed where
to look for such files. The code above does this via the thread-local
:py:class:`mitsuba.core.FileResolver` class.


Passing arguments to the scene
------------------------------

As explained in the discussion of :ref:`Mitsuba's scene description language
<sec-scene-file-format-params>`, a scene file can contained named parameters
prefixed with a ``$`` sign:

.. code-block:: xml
    :name: cbox-xml

    <!-- sample per pixel parameter -->
    <default name="spp" value="8"/>
    ...
    <sampler type="independent">
        <integer name="sample_count" value="$spp"/>
    </sampler>

These parameters can be defined explicitly using keyword arguments when loading
the scene.

.. code-block:: python

    scene = load_file(filename, spp=1024)


Loading a Mitsuba object
------------------------

It is also possible to create an instance of a single Mitsuba object (e.g. a BSDF) using
the XML parser, as shown in the following Python snippet:

.. code-block:: python

    from mitsuba.core.xml import load_string

    diffuse_bsdf = load_string("<bsdf version='2.0.0' type='diffuse'></bsdf>")

Mitsuba's test suite frequently makes use of this approach to inspect the
behavior of individual system components.


Creating objects using Python dictionaries
------------------------------------------

A more convinient way of constructing Mitsuba objects in Python is to use
:py:func:`mitsuba.core.xml.load_dict` which takes as argument a Python dictionary. This dictionary
should follow a structure similar to the XML structure used for the Mitsuba scene description.

The dictionary should always contain an entry ``"type"`` to specify the name of the plugin to
be instanciated. Keys of the dictionary must be strings and will represent the name of the
properties. The type of the property will be deduced from the Python type for simple
types (e.g. ``bool``, ``float``, ``int``, ``string``, ...). It is possible to provide another dictionary as
the value of an entry. This can be used to create nested objects, as in the XML scene description.

The following snippets illustrate the similarity between the XML code and the
Python dictionary structure:

*XML:*

.. code-block:: xml

    <shape type="obj">
        <string name="filename" value="dragon.obj"/>
        <bsdf type="roughconductor">
            <float name="alpha" value="0.01"/>
        </bsdf>
    </shape>


*Python dictionary:*

.. code-block:: python

    {
        "type" : "obj",
        "filename" : "dragon.obj",
        "something" : {
            "type" : "roughconductor",
            "alpha" : 0.01
        }
    }

Here is a more concrete example on how to use the function:

.. code-block:: python

    from mitsuba.core.xml import load_dict

    sphere = load_dict({
        "type" : "sphere",
        "center" : [0, 0, -10],
        "radius" : 10.0,
        "flip_normals" : False,
        "bsdf" : {
            "type" : "dielectric"
        }
    })

It is possible to provide another Mitsuba object within the Python dictionary instead of using
nested dictionaries:

.. code-block:: python

    # First create a BSDF (could use xml.load_string(..) as well)
    my_bsdf = load_dict({
        "type" : "roughconductor",
        "alpha" : 0.14,
    })

    # Pass the BSDF object in the dictionary
    sphere = load_dict({
        "type" : "sphere",
        "something" : my_bsdf
    })

For convience, a nested dictionary can be provided with a ``"type"`` entry equal
to ``"rgb"`` or ``"spectrum"``. Similarly to the XML parser, the ``"value"`` entry in that
dictionary will be used to instanciate the right `Spectrum` plugin.
(See the :ref:`corresponding section <sec-spectra>`)

Here as some examples of the possible use of the ``"value"`` entry in the nested dictionary:

.. code-block:: python

    # Passing gray-scale value
    "color_property" : {
        "type": "rgb",
        "value": 0.44
    }

    # Passing tri-stimulus values
    "color_property" : {
        "type": "rgb",
        "value": [0.7, 0.1, 0.5]
    }

    # Providing a spectral file
    "color_property" : {
        "type": "spectrum",
        "filename": "filename.spd"
    }

    # Providing a list of (wavelength, value) pairs
    "color_property" : {
        "type": "spectrum",
        "value": [(400.0, 0.5), (500.0, 0.8), (600.0, 0.2)]
    }

The following example constructs a Mitsuba scene using :py:func:`mitsuba.core.xml.load_dict`:

.. code-block:: python

    scene = load_dict({
        "type" : "scene",
        "myintegrator" : {
            "type" : "path",
        },
        "mysensor" : {
            "type" : "perspective",
            "near_clip": 1.0,
            "far_clip": 1000.0,
            "to_world" : ScalarTransform4f.look_at(origin=[1, 1, 1],
                                                   target=[0, 0, 0],
                                                   up=[0, 0, 1]),
            "myfilm" : {
                "type" : "hdrfilm",
                "rfilter" : { "type" : "box"},
                "width" : 1024,
                "height" : 768,
            },
            "mysampler" : {
                "type" : "independent",
                "sample_count" : 4,
            },
        },
        "myemitter" : {"type" : "constant"},
        "myshape" : {
            "type" : "sphere",
            "mybsdf" : {
                "type" : "diffuse",
                "reflectance" : {
                    "type" : "rgb",
                    "value" : [0.8, 0.1, 0.1],
                }
            }
        }
    })

As in the XML scene description, it is possible to reference other objects in the `dict`, as
long as those a declared before the reference takes place in the dictionary. For this purpose,
you can specify a nested dictionary with ``"type":"ref"`` and an ``"id"`` entry. Objects can be
referenced using their ``key`` in the dictionary. It is also possible to reference an
object using it's ``id`` if one was defined.

.. code-block:: python

    {
        "type" : "scene",
        # this BSDF can be referenced using its key "bsdf_id_0"
        "bsdf_key_0" : {
            "type" : "roughconductor"
        },

        "shape_0" : {
            "type" : "sphere",
            "mybsdf" : {
                "type" : "ref",
                "id" : "bsdf_key_0"
            }
        }

        # this BSDF can be referenced using its key "bsdf_key_1" or its id "bsdf_id_1"
        "bsdf_key_1" : {
            "type" : "roughconductor",
            "id" : "bsdf_id_1"
        },

        "shape_2" : {
            "type" : "sphere",
            "mybsdf" : {
                "type" : "ref",
                "id" : "bsdf_id_1"
            }
        },

        "shape_3" : {
            "type" : "sphere",
            "mybsdf" : {
                "type" : "ref",
                "id" : "bsdf_key_1"
            }
        }
    }