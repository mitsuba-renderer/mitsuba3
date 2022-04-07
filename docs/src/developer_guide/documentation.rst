Writing documentation
=====================


Notebook tutorials
------------------

We are using the `nbsphinx<https://nbsphinx.readthedocs.io/>`_ Sphinx extension
to render our tutorials in the online documentation.

The thumbnail of a notebook in the gallery can be the output image of a cell in
the notebook. For this, simply add the following to the metadata of that cell:

.. code-block:: json

    {
        "nbsphinx-thumbnail": {}
    }
