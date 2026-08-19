.. _sec-writing-documentation:

Writing documentation
=====================

misuka uses a multi-stage documentation generation process that combines C++ docstring extraction, plugin documentation generation, and Sphinx-based HTML generation. This guide explains how the system works and how to build documentation.

Prerequisites
-------------

Install required Python packages:

.. code-block:: bash

    pip install -r docs/requirements.txt

Then reconfigure with CMake. The ``mkdoc`` and ``mkdoc-api`` targets are only
defined if ``sphinx-build`` was on ``PATH`` at configure time, and a build
directory configured before that fails with ``ninja: error: unknown target 'mkdoc'``.

Documentation sources
---------------------

Documentation comes from several sources:

1. **C++ headers** (``include/mitsuba/{core,render,ui}/*.h``): API documentation extracted via docstrings
2. **C++ plugin sources** (``src/{bsdfs,shapes,sensors,films,integrators,...}/*.cpp``): Plugin descriptions and parameters. The built plugin reference is filtered to misuka's acoustic thin fork (see ``docs/generate_plugin_doc.py``). ``shapes``, ``textures``, ``samplers`` and ``rfilters`` are kept whole, the rest are allow-listed down to the plugins misuka ships.
3. **RST files** (``docs/src/``): User guides, tutorials, and manual content
4. **Jupyter notebooks** (``tutorials_acoustic/``, symlinked by ``docs/conf.py`` into ``docs/src/rendering`` and ``docs/src/inverse_rendering``): Interactive tutorials rendered with nbsphinx

Build process overview
----------------------

The complete documentation build requires multiple steps in a specific order:

.. tabs::

    .. code-tab:: bash Linux / macOS

        ninja docstrings    # Extract C++ docstrings → include/mitsuba/python/docstr.h
        ninja               # Build main library and Python bindings
        ninja mkdoc-api     # Generate API reference documentation
        ninja mkdoc         # Build final HTML documentation

    .. code-tab:: powershell Windows

        # Docstring extraction not available on Windows - see below.
        cmake --build . --config Release
        cmake --build . --config Release --target mkdoc-api
        cmake --build . --config Release --target mkdoc

The ``docstrings`` target is only defined on Unix, so the Windows tab starts at the second step and picks up the committed ``docstr.h``.
If you edit a C++ header on Windows, make a note in your pull request that the docstrings need to be regenerated.

Detailed build steps
--------------------

1. **Docstring extraction -- Unix only** (``ninja docstrings``): Parses C++ headers in ``include/mitsuba/`` using `pybind11_mkdoc <https://github.com/pybind/pybind11_mkdoc>`_ to generate ``include/mitsuba/python/docstr.h`` for Python bindings.

   ``docstr.h`` is tracked by git, run this command and commit the result whenever you edit a header comment.
   This step is not available on Windows.
   If you have edited a header file and don't have access to a Unix-based machine (Linux or macOS), make a note in your pull request.

   .. important::

       The generated file depends on the *enabled variants*, not just on the headers.
       ``MI_EXTERN_CLASS`` and ``MI_EXTERN_STRUCT`` expand to one ``extern template`` declaration per enabled variant, and ``pybind11_mkdoc`` emits a numbered ``__doc_mitsuba_<Name>_<N>`` entry for each of them.
       Regenerating with a shortened or extended ``"enabled"`` list therefore adds or removes empty entries for every class declared with those macros, which buries the real changes in hundreds of lines of noise.
       Restore the default variant set before regenerating.

   .. note::

       ``pybind11_mkdoc`` looks for libclang under ``/usr/lib*/llvm-*/lib/libclang.so.1``, a layout that only Debian-style distributions use.
       On distributions that install libclang directly into ``/usr/lib``, such as Arch, this step fails with ``Failed to find a LLVM installation``.
       Point it at the system LLVM instead by adding these environment variables to your ``.bashrc`` or ``.zshrc`` (note that the error message names ``LIBLLVM_DIR_PATH`` while the code actually reads ``LLVM_DIR_PATH``):

       .. code-block:: bash

           # replace with your system's paths
           export LLVM_DIR_PATH=/usr
           export LIBCLANG_PATH=/usr/lib/libclang.so

       macOS needs no configuration, ``pybind11_mkdoc`` finds Xcode's libclang on its own as long as the full Xcode is installed (see :ref:`compiling - macOS <sec-compiling-macos>`), the command line tools alone are not enough.

       The libclang version also affects how symbols are named.
       A regeneration on a different LLVM release can rename entries, for example ``__doc_mitsuba_SGGXPhaseFunctionParams_operator_const_Array`` to ``__doc_mitsuba_SGGXPhaseFunctionParams_operator_const_drjit_Array``.
       Such renames are harmless as long as nothing references the entry, but they make the diff harder to read.


2. **Main build** (``ninja``): Compiles the C++ library, plugins, and Python bindings with embedded docstrings, required before generating API documentation.

3. **API documentation** (``ninja mkdoc-api``): Introspects Python modules to generate API reference in ``build/html_api/``.

   .. note::

       This step imports the built ``misuka`` package, so run it from a shell that
       has sourced ``setpath`` (see :ref:`sec-python-environments`), otherwise
       Sphinx aborts with ``ModuleNotFoundError: No module named 'misuka'``. It
       also needs the ``llvm_ad_rgb`` variant, which is enabled by default.

   .. important::

       This step rewrites the tracked file ``docs/generated/extracted_rst_api.rst``.
       Its contents are read off the built binary and differ per platform and variant set, so discard the diff unless you are regenerating on Linux with the default variants.

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
