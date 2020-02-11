Rendering a scene
=================

The simplest possible application of the Python bindings is to simply render an existing scene
described in an XML file. All examples used in the following can be found under :code:`docs/examples`
in the Mitsuba 2 repository. Any file paths used in these code examples, assume the example code is
run from the folder it is in (e.g. :code:`docs/examples/01_render_scene/render_scene.py` for this
current example).

The following code describes how to render a scene and write out the resulting image either as EXR
image or as tonemapped JPG.

.. literalinclude:: ../../examples/01_render_scene/render_scene.py
   :language: python
   :lines: 1-41

