.. _sec-samplers:

Samplers
========

When rendering, misuka has to solve a high-dimensional integration problem that involves the
geometry, materials, sources, and sensors that make up the scene. Because of the mathematical
complexity of these integrals, it is generally impossible to solve them analytically -- instead, they
are solved numerically by evaluating the function to be integrated at a large number of
different positions referred to as samples. Sample generators are an essential ingredient to this
process: they produce points in a (hypothetical) infinite dimensional hypercube
:math:`[0, 1]^{\infty}` that constitute the canonical representation of these samples.

To do its work, a rendering algorithm, or integrator, will send many queries to the sample
generator. Generally, it will request subsequent 1D or 2D components of this infinite-dimensional
*point* and map them into a more convenient space (for instance, positions on surfaces). This allows
it to construct paths and eventually evaluate the energy transport through the scene.

.. note::

    Samplers are inherited from Mitsuba unchanged and behave identically for acoustic rendering.
    The paths they help construct carry sound energy rather than light, and the quantity being
    estimated is an :ref:`energy-time curve <sec-acoustic-rendering>` rather than an image, but
    nothing about sample generation differs. One practical consequence of the acoustic output is
    that ``spp`` counts samples *per frequency band*, not per pixel.
