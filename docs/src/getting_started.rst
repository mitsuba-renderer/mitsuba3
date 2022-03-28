Getting Started
===============

Welcome to the Getting Started page of the Mitsuba 3 documentation. Before
getting our hands dirty, you will need to install the Mitsuba 3 and DrJit libraries
in our python environnement if it is not already done. Both libraries are distributed
on `PyPI <https://pypi.org/project/mitsuba/>`_ and can easily be installed with
:monosp:`pip`:

.. code-block:: bash

    pip3 install mitsuba

DrJit being a dependency of Mitsuba 3, it will be automatically installed when executing the command above.

Quickstart
----------

.. nbgallery::
    :caption: Quickstart

    tutorials/getting_started/quickstart/00_drjit_cheat_sheet
    tutorials/getting_started/quickstart/01_rendering
    tutorials/getting_started/quickstart/03_python_renderer


Differentiable rendering
------------------------

.. nbgallery::
    :caption: Differentiable rendering

    tutorials/getting_started/inverse_rendering/01_gradient_based_opt
    tutorials/getting_started/inverse_rendering/02_forward_ad_rendering


Other examples
--------------

.. nbgallery::
    :caption: Other examples

    tutorials/getting_started/examples/granular_phase_function