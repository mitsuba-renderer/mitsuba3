:hide-toc:

.. only:: not latex

.. .. image:: images/mitsuba-logo-white-bg.png
..     :width: 75%
..     :align: center

.. image:: resources/data/docs/images/banners/banner_01.png
        :width: 100%
        :align: center

Mitsuba 3
=========

Mitsuba 3 is a research-oriented rendering system for forward and inverse
simulation. It consists of a small set of core libraries and a wide variety of
plugins that implement functionality ranging from materials and light sources to complete
rendering algorithms. Mitsuba 3 strives to retain scene compatibility with its
predecessors: `Mitsuba 0.6 <https://github.com/mitsuba-renderer/mitsuba>`_ and
`Mitsuba 2 <https://github.com/mitsuba-renderer/mitsuba2>`_.
However, in most other respects, it is a completely new system following a
different set of goals.

Installation
------------

Mitsuba 3 can be installed via :monosp:`pip` from `PyPI
<https://pypi.org/project/mitsuba/>`_. This is the recommended installation
method for most users.

.. code-block:: bash

    pip install mitsuba

This command will install all the necessary dependencies on your system (e.g.
`Dr.Jit`) if not already available.

.. warning::

    For computation and rendering on the GPU, Mitsuba 3 relies on dynamic
    library loading which will seamlessly use the CUDA and OptiX installation
    present within the GPU driver itself. However note that a version of the
    NVidia GPU driver equal or greater than 495.89 is required.

See the :ref:`developer guide <sec-compiling>` for complete instructions on building
from the git source tree.


Hello World!
------------

You should now be all setup to render your first scene with Mitsuba 3. Running
the Mitsuba 3 code below will render the famous Cornell Box scene and write the
rendered image to a file on disk.

.. code-block:: python

    import mitsuba as mi

    mi.set_variant('scalar_rgb')

    img = mi.render(mi.load_dict(mi.cornell_box()))

    mi.Bitmap(img).write('cbox.exr')


.. _sec-quickstart:

Quickstart
----------

For the new users, we put together absolute beginner's tutorials for both DrJit and Mitsuba.

.. panels::
    :body: text-center font-weight-bold

    .. image:: ../resources/data/docs/images/logos/drjit-logo.png
        :height: 50
        :align: center

    Dr.Jit quickstart

    .. link-button:: src/quickstart/drjit_quickstart
        :type: ref
        :text:
        :classes: btn-link stretched-link

    ---

    .. image:: ../resources/data/docs/images/logos/mitsuba-logo.png
        :height: 50
        :align: center

    Mitsuba quickstart

    .. link-button:: src/quickstart/mitsuba_quickstart
        :type: ref
        :text:
        :classes: btn-link stretched-link

.. .............................................................................

.. toctree::
    :maxdepth: 1
    :caption: Tutorials
    :hidden:

    src/rendering_tutorials
    src/inverse_rendering_tutorials
    src/others_tutorials

.. toctree::
    :maxdepth: 1
    :caption: Guides
    :hidden:

    src/how_to_guides
    src/key_topics
    src/developer_guide

.. toctree::
    :maxdepth: 1
    :caption: References
    :hidden:

    src/plugin_reference
    src/api_reference

.. toctree::
    :maxdepth: 1
    :caption: Miscellaneous
    :hidden:

    src/gallery
    release_notes
    zz_bibliography
