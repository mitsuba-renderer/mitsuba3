Parsing XML code
=================

Mitsuba provides two functions for parsing data expressed in :ref:`Mitsuba's
XML scene description language <sec-file-format>`:
:py:func:`mitsuba.core.xml.load_file` to load files on disk, and
:py:func:`mitsuba.core.xml.load_string` to load arbitrary strings.

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

    # Add the scene directory to the FileResolver's search path
    Thread.thread().file_resolver().append(os.path.dirname(filename))

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

    import mitsuba
    mitsuba.set_variant('scalar_rgb')
    from mitsuba.core.xml import load_string

    diffuse_bsdf = load_string("<bsdf version='2.0.0' type='diffuse'></bsdf>")

Mitsuba's test suite frequently makes use of this approach to inspect the
behavior of individual system components.
