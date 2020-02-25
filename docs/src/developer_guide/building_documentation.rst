Building the documentation
==========================

The Mitsuba documentation is generated using the `Sphinx <https://www.sphinx-doc.org/en/master/>`_
tool. It takes several steps to build the documentation from scratch, but only the last step will
be necessary in most cases.

From the ``build`` folder, one can run the following commands:

``make docstrings``:
    Generates the ``docstr.h`` file from the C++ headers. This will be used by pybind11 to add
    docstrings to the python bindings.

    .. note:: This step is only necessary if documentation/comments has be changed in the header
              files (in which case the python bindings will need to be built again).

``make mkdoc-api``:
    Extracts the python binding's documentation for all the members (classes, functions, ...) of the
    ``scalar_rgb`` Mitsuba python library using the `Autodoc <http://www.sphinx-doc.org/en/master/usage/extensions/autodoc.html>`_
    Sphinx extension in order to automatically generate an API reference documentation.
    This API documentation will be properly translated to RST and written down to
    the ``extracted_rst_api.rst`` file. It also generates the ``core_api.rst``, ``render_api.rst``
    and ``python_api.rst`` which include different parts of the extracted RST to form the final
    API documentation.

    .. note:: This step is only necessary if documentation/comments has be changed in the header
              files. It should be ran after calling ``make docstrings`` and rebuilding the python
              bindings.

              See ``docs/docs_api/conf.py`` for more information.

    .. warning:: The API documentation will only be generated for the ``scalar_rgb`` variant of the
                 renderer.

``make mkdoc``:
    Generates the final HTML documentation including the extracted API and plugin's documentation.

    .. note:: The plugin's documentation should be written directly in the ``.cpp`` files using
              the RST language. Those will get extracted automatically when running this command.

