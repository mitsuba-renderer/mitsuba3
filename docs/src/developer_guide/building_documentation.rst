Building the documentation
==========================

The Mitsuba documentation is generated using `Sphinx
<https://www.sphinx-doc.org/en/master/>`_. It takes several steps to build the
documentation from scratch, but only the last step will be necessary in most
cases.

From the ``build`` folder, one can run the following commands:

``ninja docstrings``:
    Generate the :file:`include/python/docstr.h` file from the C++ headers.
    This file is used by pybind11 to add docstrings to the Python bindings.

    .. note::

        This step is only necessary if documentation/comments have changed in
        the header files (in which case the python bindings will need to be
        built again after calling this).

``ninja mkdoc-api``:
    Extract documentation (classes, functions, of both Python & C++ portions)
    of the ``scalar_rgb`` variant using `Autodoc
    <http://www.sphinx-doc.org/en/master/usage/extensions/autodoc.html>`_ to
    generate generate API reference documentation (stored in
    ``extracted_rst_api.rst``, ``core_api.rst``, ``render_api.rst`` and
    ``python_api.rst``).

    .. note::

        This step is only necessary if documentation/comments have changed in
        the header files.

``ninja mkdoc``:
    Generate the final HTML documentation including the extracted API and
    plugin documentation. The output will be stored in the ``html``
    subdirectory.
