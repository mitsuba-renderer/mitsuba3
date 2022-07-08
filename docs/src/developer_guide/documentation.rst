.. _sec-writing-documentation:

Writing documentation
=====================

Additional packages are required to generate the HTML documentation which can
be installed running the following command:

.. code-block:: bash

    pip install -r docs/requirements.txt

If not already done, run `cmake` in the build folder to generate the build
targets related to the documentation:

.. code-block:: bash

  cd build
  cmake -GNinja ..

To generate the HTML documentation, build the `mkdoc` target as follow:

.. code-block:: bash

    ninja mkdoc

This process will generate the documentation HTML files into the `build/html`
folder.

API references generation
-------------------------

The API references need to be generated manually executing the following command:

.. code-block:: bash

    ninja mkdoc-api
    ninja mkdoc # Rebuild the documentation to update the API references

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
