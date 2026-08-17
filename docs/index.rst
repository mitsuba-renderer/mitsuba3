.. only:: not latex

    .. image:: images/misuka_logo.png
        :width: 60%
        :align: center

Getting started
===============

misuka is a research-oriented, differentiable **room-acoustic renderer** for
forward and inverse sound-transport simulation. It is a fully compatible
extension to :external+mitsuba:doc:`Mitsuba 3 <index>`: it reuses
Mitsuba's scene format, geometry, samplers, and the
:external+drjit:doc:`Dr.Jit <index>` JIT compiler / autodiff engine, and
adds acoustic plugins (an absorbing/scattering material, several acoustic path tracers,
a microphone sensor, and an energy-time curve film). It implements `Time-Resolved
Path Replay Backpropagation <https://dl.acm.org/doi/pdf/10.1145/3730900>`_ for
efficient gradient estimation with respect to material properties, source/receiver
positions, and scene geometry.

Because misuka is an extension, the light-transport engine, scene description
language, and Python API are documented upstream. This site documents only
**acoustic rendering functionality**. Follow the links above for everything
misuka inherits from Mitsuba 3 and Dr.Jit.

Installation
------------

A PyPi package will be released very soon!
For now, either :ref:`install from source <sec-installing-from-source>` or
follow the :ref:`compilation guide <sec-compiling>` if you plan on working on
the code.

.. misuka can be installed via :monosp:`pip` from `PyPI
.. <https://pypi.org/project/misuka/>`_. This is the recommended method of installation.

.. .. code-block:: bash

..     pip install misuka

.. This command will also install :monosp:`Dr.Jit` on your system if not already available.

.. See the :ref:`developer guide <sec-compiling>` for complete instructions on building
.. from the git source tree.

Requirements
^^^^^^^^^^^^

- ``Python >= 3.9``
- (optional) For computation on the GPU: ``Nvidia driver >= 535``
- (optional) For vectorized / parallel computation on the CPU: ``LLVM >= 15``
- (optional) For computation on Apple Silicon GPUs: macOS 15 or newer with a
  Metal-capable GPU

The backends load their compiler and driver libraries from your system at
runtime. See :ref:`sec-runtime-requirements` for how to install them, and how to
troubleshoot an unavailable backend.

Where to go next
----------------

This documentation is divided into three main parts: Tutorials, Guides, and
References. If you are new to misuka, start with the tutorials and read
:ref:`sec-acoustic-rendering` alongside them.

Tutorials
^^^^^^^^^

For new users, we put together a set of tutorials.
Each tutorial is a Jupyter notebook that you can read directly in your browser,
or download and run yourself.
See the :doc:`rendering tutorials <src/rendering_tutorials>` and
:doc:`inverse rendering tutorials <src/inverse_rendering_tutorials>`.

Guides
^^^^^^

The :doc:`Key Topics <src/key_topics>` contain background information about
how misuka works.
The topic :ref:`sec-acoustic-rendering` covers the basics of acoustic rendering
and the differences between misuka's functionality and Mitsuba 3.
It is a must read if you haven't used misuka before.

The :doc:`Developer Guide <src/developer_guide>`, especially :ref:`sec-compiling`,
is relevant for you if you want to compile misuka yourself, either because you
want to build additional variants or change the code.

Mitsuba 3's :doc:`How-to Guides <src/how_to_guides>` are a great reference that
explains some features of Mitsuba and misuka in greater detail. Since misuka is
a compatible extension, they apply to acoustic scenes unchanged.

References
^^^^^^^^^^

The :doc:`Plugin reference <src/plugin_reference>` documents the plugins
(integrators, materials, shapes, ...) that acoustic scenes are built from, along
with the parameters each of them accepts. Mitsuba plugins that are not useful
for acoustic rendering are left out. They still load, and they are documented in
the :external+mitsuba:ref:`Mitsuba plugin reference <sec-plugins>`.

The :doc:`API reference <src/api_reference>` covers the full Python API. It also
lists functions that misuka inherits from Mitsuba and that are rarely needed for
acoustic rendering. They are kept for completeness.

License
-------

misuka is licensed under the `PolyForm Noncommercial License 1.0.0
<https://polyformproject.org/licenses/noncommercial/1.0.0>`_, which permits academic
and private use. Files inherited from Mitsuba 3 remain under the original BSD-3-Clause
license. See the full license on `github
<https://github.com/misuka-renderer/misuka/blob/master/LICENSE>`_.

If you are interested in using misuka commercially, please contact
a.jueterbock@tu-berlin.de.

Citation
--------

When using misuka in academic projects, please cite:

.. code-block:: bibtex

    @article{misuka,
        title   = {{misuka}: An Open-Source Differentiable Room Acoustic Renderer},
        author  = {J{\"u}terbock, Tobias and Finnendahl, Ugo and Worchel, Markus and
                   Wujecki, Daniel and Alexa, Marc and Weinzierl, Stefan},
        journal = {Proceedings of Meetings on Acoustics},
        volume  = {58},
        number  = {1},
        pages   = {022004:1--022004:13},
        year    = {2026},
        doi     = {10.1121/2.0002193},
    }

When using Time-Resolved Path Replay Backpropagation, please also cite:

.. code-block:: bibtex

    @article{acoustic_prb,
        title   = {Differentiable Geometric Acoustic Path Tracing Using
                   Time-Resolved Path Replay Backpropagation},
        author  = {Finnendahl, Ugo and Worchel, Markus and J{\"u}terbock, Tobias and
                   Wujecki, Daniel and Brinkmann, Fabian and Weinzierl, Stefan and
                   Alexa, Marc},
        journal = {ACM Transactions on Graphics},
        volume  = {44},
        number  = {4},
        pages   = {82:1--82:17},
        year    = {2025},
        doi     = {10.1145/3730900},
    }

misuka is built on :external+mitsuba:doc:`Mitsuba 3 <index>`. When
appropriate, please also cite the underlying renderer following its
:external+mitsuba:ref:`citation guidelines </index.rst#citation>`.

.. .............................................................................

.. toctree::
   :hidden:

   self
   src/runtime_requirements

.. toctree::
    :maxdepth: 1
    :caption: Tutorials
    :hidden:

    src/rendering_tutorials
    src/inverse_rendering_tutorials

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

    release_notes
    zz_bibliography
