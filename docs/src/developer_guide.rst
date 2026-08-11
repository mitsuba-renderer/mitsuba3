.. image:: ../resources/data/docs/images/banners/banner_05.jpg
    :width: 100%
    :align: center

Developer's Guide
=================

Overview
--------

This section is addressed to the users interested in modifying the core of the
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

* The rendering folder (in `src/render`) contains abstractions needed to
  load and represent scenes containing light sources, shapes, materials, and
  participating media.

* The python folder (in `src/python`) contains components of the system that are
  written in Python, and which access Mitsuba through bindings. This includes
  statistical tests (Chi^2, etc.) and tooling for differentiable rendering.

All other folders in `src` implement Mitsuba 3 plugins such as `bsdf`, `shapes`,
etc.

Coding style
------------

The following style guidelines help make C++ code digestible to Mitsuba's API
documentation tooling, while establishing consistency between C++ and Python
parts.

- Docstrings use ``/** */`` (or ``///`` for single-line ones) comments.
  (Like Doxygen).

- However, the docstrings themselves are written in Google-style
  reStructuredText and *not* Doxygen. In particular, use double backticks for
  inline code and single backticks for cross-references.
  The special ``Args:`` and ``Returns:`` blocks and indentation annotate
  the function signature.

- The elements of tuple return values should be explained bullet points under
  ``Returns:``.

- Python docstrings follow the same Google-style reStructuredText convention,
  so that the C++ and Python parts of the API reference read consistently.

- Comments inside a function body use ``//``.

A concrete exmaple:

.. code-block:: cpp

    /**
     * Sample a point on the surface of this shape
     *
     * This function maps the uniform sample ``sample`` onto the
     * shape's surface area. The asssociated distribution is modeled
     * by `pdf_position()`.
     *
     * Args:
     *     time: The scene time associated with the position sample
     *
     *     sample: A uniformly distributed 2D point on the domain
     *         :math:`[0,1]^2`
     *
     * Returns:
     *     A `PositionSample3f` instance describing the generated sample
     */
    virtual PositionSample3f sample_position(Float time, const Point2f &sample,
                                             Mask active = true) const {
        // A comment inside the function
        ...
    }

See :ref:`sec-writing-documentation` for how these docstrings turn into the
generated API reference.

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
