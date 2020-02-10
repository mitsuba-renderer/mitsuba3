Rendering a scene
=================

The simplest possible application of the Python bindings is to simply render an existing scene described in an XML file.
All examples used in the following can be found under :code:`docs/examples` in the Mitsuba 2 repository.
Any file paths used in these code examples, assume the example code is run from the folder it is in (e.g. :code:`docs/examples/01_render_scene/render_scene.py` for this current example).

The following code describes how to render a scene and write out the resulting image either as EXR image or as tonemapped JPG.

.. literalinclude:: ../../examples/01_render_scene/render_scene.py
   :language: python
   :lines: 1-41

When loading a scene, we can also optionally pass in arguments to the scene.
If we want to for example parametrize the number of samples per pixel, we could add the following lines to our Cornell box scene file:

.. code-block:: xml
    :name: cbox-xml

    <default name="spp" value="8"/>
    ...
    <sampler type="independent">
        <integer name="sample_count" value="$spp"/>
    </sampler>

To then set this parameter, we pass it when loading the scene:

.. code-block:: python
    :name: cbox-python-paramer

    scene = load_file(filename, [('spp' ,'1024')])


