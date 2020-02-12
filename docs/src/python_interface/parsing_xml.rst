Parsing XML code
=================

The :code:`mitsuba.core.xml` sub-module provides two methods for parsing XML code following the
scene format convention. :code:`load_file` to parses XML files and :code:`load_string` to parses
XML strings.

Loading a scene
---------------

When dealing with relative path in the scene description, the XML parser relies on the thread local
:code:`FileResolver` for resolving the full paths. It is therefore necessary to add the scene directory to
the :code:`FileResolver`'s search path in order to properly load the scene's data (e.g. mesh, textures, ...).

Here is a simple Python example on how to load a Mitsuba scene from an XML file:

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


Passing arguments to the scene
------------------------------

As explained in :ref:`sec-scene-file-format-params`, a scene file can contained named parameters:

.. code-block:: xml
    :name: cbox-xml

    <!-- sample per pixel parameter -->
    <default name="spp" value="8"/>
    ...
    <sampler type="independent">
        <integer name="sample_count" value="$spp"/>
    </sampler>

In Python, it is possible to set those parameters when loading the scene using the :code:`parameters` argument:

.. code-block:: python

    scene = load_file(filename, parameters=[('spp' ,'1024')])


Loading a Mitsuba object
------------------------

It is also possible to create an instance of a single Mitsuba object (e.g. a BSDF) using
the XML parser in Python, as shown in the following Python snippet:

.. code-block:: python

    import mitsuba
    mitsuba.set_variant('scalar_rgb')
    from mitsuba.core.xml import load_string

    diffuse_bsdf = load_string("<bsdf version='2.0.0' type='diffuse'></bsdf>")
