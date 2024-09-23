.. image:: ../resources/data/docs/images/banners/banner_05.jpg
    :width: 100%
    :align: center

Developer's Guide
=================

Overview
--------

This section is addressed to the users interesting in modifying the core of the
system or even contributing to the codebase.

New developers will want to begin by thoroughly reading the documentation of
Dr.Jit before looking at any Mitsuba code. Dr.Jit is a Just-In-Time compiler
that constitutes the foundation of Mitsuba 3. It drives the code transformations
that enable systematic vectorization and automatic differentiation of the
renderer.

Code structure
--------------

The Mitsuba codebase is split into 3 basic support folders:

* The core folder (in `src/core`) implements basic functionality such as
  cross-platform file and bitmap I/O, data structures, scheduling, as well as
  logging and plugin management.

* The rendering folder (in `src/render``) contains abstractions needed to
  load and represent scenes containing light sources, shapes, materials, and
  participating media.

* The python folder (in `src/python`) contains components of the system that are
  written in Python, and which access Mitsuba through bindings. This includes
  statistical tests (Chi^2, etc.) and tooling for differentiable rendering.

All other folders in `src` implement Mitsuba 3 plugins such as `bsdf`, `shapes`,
etc.

Coding style
------------

We've essentially imported Python's `PEP 8 <https://peps.python.org/pep-0008/>`_
into the C++ side (which does not specify a recommended naming convention),
ensuring that code that uses functionality from both languages looks natural.

Contributing
------------

All contributions, bug reports, bug fixes, documentation improvements,
enhancements, and ideas are welcome. If you are brand new to Mitsuba or
open-source development, we recommend going through the GitHub “issues” tracker
to find issues that interest you.

Going further
-------------

.. toctree::
    :maxdepth: 1
    :glob:

    developer_guide/compiling
    developer_guide/documentation
    developer_guide/variants_cpp
    developer_guide/writing_plugin
    developer_guide/testing
