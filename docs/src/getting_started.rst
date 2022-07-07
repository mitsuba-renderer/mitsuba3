.. image:: ../resources/data/docs/images/banners/banner_03.png
    :width: 100%
    :align: center

Getting Started
===============

Overview
--------

This page will help new users to get set up with Mitsuba 3 on their system in a
split-second, from installation to basic use-cases.

Installation
------------

Mitsuba 3 can be installed via :monosp:`pip` from `PyPI
<https://pypi.org/project/mitsuba/>`_. This is the recommended installation
method for most users.

.. code-block:: bash

    pip install mitsuba

This command will install all the necessary dependencies on your system (e.g.
`Dr.Jit`) if not already available.

.. note::

    For computation and rendering on the GPU, Mitsuba 3 relies on dynamic
    library loading which will seamlessly use the CUDA and OptiX installation
    present within the GPU driver itself.

See the :ref:`developer guide <sec-compiling>` for complete instructions on building
from the git source tree.

Quickstart
----------

.. _sec-quickstart:

For the new users, we put together absolute beginner's tutorials for both DrJit and Mitsuba.

.. nbgallery::

    getting_started/quickstart/drjit_quickstart
    getting_started/quickstart/mitsuba_quickstart