.. _sec-writing-documentation:

Writing documentation
=====================

Mitsuba uses a multi-stage documentation generation process that combines C++ docstring extraction, plugin documentation generation, and Sphinx-based HTML generation. This guide explains how the system works and how to build documentation.

Prerequisites
-------------

Install required Python packages:

.. code-block:: bash

    pip install -r docs/requirements.txt

Documentation sources
---------------------

Documentation comes from several sources:

1. **C++ headers** (``include/mitsuba/{core,render}/*.h``): API documentation extracted via docstrings
2. **C++ plugin sources** (``src/{bsdfs,shapes,emitters,...}/*.cpp``): Plugin descriptions and parameters
3. **RST files** (``docs/src/``): User guides, tutorials, and manual content
4. **Jupyter notebooks** (``docs/tutorials/``): Interactive tutorials rendered with nbsphinx

Build process overview
----------------------

The complete documentation build requires multiple steps in a specific order:

.. code-block:: bash

    ninja docstrings    # Extract C++ docstrings → include/mitsuba/python/docstr.h
    ninja               # Build main library and Python bindings
    ninja mkdoc-api     # Generate API reference documentation
    ninja mkdoc         # Build final HTML documentation

Detailed build steps
--------------------

1. **Docstring extraction** (``ninja docstrings``): Parses C++ headers in ``include/mitsuba/`` using ``pybind11_mkdoc`` to generate ``include/mitsuba/python/docstr.h`` for Python bindings.

2. **Main build** (``ninja``): Compiles the C++ library, plugins, and Python bindings with embedded docstrings—required before generating API documentation.

3. **API documentation** (``ninja mkdoc-api``): Introspects Python modules to generate API reference in ``build/html_api/``.

4. **Main documentation** (``ninja mkdoc``): Builds the complete documentation website in ``build/html/`` by running plugin extraction, processing notebooks, and combining all sources.

Notebook tutorials
------------------

We are using the `nbsphinx <https://nbsphinx.readthedocs.io/>`_ Sphinx extension
to render our tutorials in the online documentation.

The thumbnail of a notebook in the gallery can be the output image of a cell in
the notebook. For this, simply add the following to the metadata of that cell:

.. code-block:: json

    {
        "nbsphinx-thumbnail": {}
    }


In order to hide a cell of a notebook in the documentation, add the following to
the metadata of that cell:

.. code-block:: json

    {
        "nbsphinx": "hidden"
    }
