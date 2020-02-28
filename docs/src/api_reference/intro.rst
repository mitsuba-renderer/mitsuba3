.. _sec-api:

Introduction
============

This API reference documentation was automatically generated using the `Autodoc
<http://www.sphinx-doc.org/en/master/usage/extensions/autodoc.htm>`_ Sphinx
extension.

Autodoc automatically processes the documentation of Mitsuba's Python bindings,
hence all C++ function and class signatures are documented through their Python
counterparts. Mitsuba's bindings mimic the C++ API as closely as possible,
hence this documentation should still prove valuable even for C++ developers.

The documentation is split into three-submodules:

- :ref:`sec-api-core`: Core functionality that is not directly related to rendering algorithms.
- :ref:`sec-api-render`: Interfaces of components like rendering algorithms, sensors, emitters, textures, participating media, etc.
- :ref:`sec-api-python`: Higher-level functionality that is written in Python: infrastructure for automatic differentiation, testing (Chi^2 test), etc.

.. warning::

    The documentation was extracted from the ``scalar_rgb`` variant of the
    renderer. Other variants will use different types in many function and
    attribute signatures. Such changes should mostly be "obvious" from the
    context. For example, ``scalar_spectral`` will replace all RGB types by
    color spectra, and ``gpu_rgb`` will replace scalar floating point values
    with Enoki CUDA arrays, etc.
