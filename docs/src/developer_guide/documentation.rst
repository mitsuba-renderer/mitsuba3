Writing documentation
=====================

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

Before building the documentation, make sure to install `pandoc <https://pandoc.org/installing.html>`_
as well as the required libraries:

.. code-block:: bash

    pip install -r docs/requirements.txt