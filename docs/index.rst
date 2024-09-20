.. only:: not latex

.. .. image:: images/mitsuba-logo-white-bg.png
..     :width: 75%
..     :align: center

.. image:: resources/data/docs/images/banners/banner_01.jpg
        :width: 100%
        :align: center

Getting started
===============

Mitsuba 3 is a research-oriented rendering system for forward and inverse
light-transport simulation. It consists of a small set of core libraries and a
wide variety of plugins that implement functionality ranging from materials and
light sources to complete rendering algorithms. Mitsuba 3 strives to retain
scene compatibility with its predecessors: `Mitsuba 0.6
<https://github.com/mitsuba-renderer/mitsuba>`_ and `Mitsuba 2
<https://github.com/mitsuba-renderer/mitsuba2>`_. However, in most other
respects, it is a completely new system following a different set of goals.

Installation
------------

Mitsuba 3 can be installed via :monosp:`pip` from `PyPI
<https://pypi.org/project/mitsuba/>`_. This is the recommended method of installation.

.. code-block:: bash

    pip install mitsuba

This command will also install :monosp:`Dr.Jit` on your system if not already available.

See the :ref:`developer guide <sec-compiling>` for complete instructions on building
from the git source tree.

When using the `Windows Subsystem for Linux 2 (WSL2)
<https://learn.microsoft.com/en-us/windows/wsl/compare-versions#whats-new-in-wsl-2>`__,
you must follow the :ref:`linked instructions <optix-wsl2>` to enable hardware-accelerated
ray tracing on NVIDIA GPUs.

Requirements
^^^^^^^^^^^^

- ``Python >= 3.8``
- (optional) For computation on the GPU: ``NVidia driver >= 495.89``
- (optional) For vectorized / parallel computation on the CPU: ``LLVM >= 11.1``

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

For the new users, we put together absolute beginner's tutorials for both Dr.Jit and Mitsuba.

.. grid:: 2

    .. grid-item-card:: Dr.Jit quickstart
        :class-title: sd-text-center sd-font-weight-bold
        :link: src/quickstart/drjit_quickstart.html

        .. image:: ../resources/data/docs/images/logos/drjit-logo.png
            :height: 200
            :align: center


    .. grid-item-card:: Mitsuba quickstart
        :class-title: sd-text-center sd-font-weight-bold
        :link: src/quickstart/mitsuba_quickstart.html

        .. image:: ../resources/data/docs/images/logos/mitsuba-logo.png
            :height: 200
            :align: center


Video tutorials
---------------

The following `YouTube playlist <https://www.youtube.com/playlist?list=PLI9y-85z_Po6da-pyTNGTns2n4fhpbLe5>`_ contains various video tutorials related
to Mitsuba 3 and Dr.Jit, perfect to get you started with those two libraries.

.. youtube:: LCsjK6Cbv6Q

Citation
--------

When using Mitsuba 3 in academic projects, please cite:

.. code-block:: bibtex

    @software{jakob2022mitsuba3,
        title = {Mitsuba 3 renderer},
        author = {Wenzel Jakob and Sébastien Speierer and Nicolas Roussel and Merlin Nimier-David and Delio Vicini and Tizian Zeltner and Baptiste Nicolet and Miguel Crespo and Vincent Leroy and Ziyi Zhang},
        note = {https://mitsuba-renderer.org},
        version = {3.0.1},
        year = 2022,
    }

.. .............................................................................

.. toctree::
   :hidden:

   self

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
    src/optix_setup
    porting_3_6
    release_notes
    zz_bibliography
