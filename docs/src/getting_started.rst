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

    tutorials/getting_started/quickstart/drjit_cheat_sheet
    tutorials/getting_started/quickstart/rendering

Rendering
----------

.. nbgallery::
    :caption: Rendering

    tutorials/getting_started/rendering/scene_modification
    tutorials/getting_started/rendering/multiple_sensors
    tutorials/getting_started/rendering/python_renderer
    tutorials/getting_started/rendering/polarized_rendering


Differentiable rendering
------------------------
.. _sec-diff-rendering-tutos:

.. nbgallery::
    :caption: Differentiable rendering

    tutorials/getting_started/inverse_rendering/gradient_based_opt
    tutorials/getting_started/inverse_rendering/forward_ad_rendering
    tutorials/getting_started/inverse_rendering/caustics_optimization
    tutorials/getting_started/inverse_rendering/reparam_optimization
    tutorials/getting_started/inverse_rendering/volume_optimization
    tutorials/getting_started/inverse_rendering/polarizer_optimization


Other applications
------------------

.. nbgallery::
    :caption: Other applications

    tutorials/getting_started/others/bsdf_plot
    tutorials/getting_started/others/granular_phase_function
    tutorials/getting_started/others/custom_plugin
