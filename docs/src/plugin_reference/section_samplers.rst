.. _sec-samplers:

Samplers
========

When rendering an image, Mitsuba 3 has to solve a high-dimensional integration problem that involves
the geometry, materials, lights, and sensors that make up the scene. Because of the mathematical
complexity of these integrals, it is generally impossible to solve them analytically — instead, they
are solved numerically by evaluating the function to be integrated at a large number of different
positions referred to as samples. Sample generators are an essential ingredient to this process:
they produce points in a (hypothetical) infinite dimensional hypercube :math:`[0, 1]^{\infty}`
that constitute the canonical representation of these samples.

To do its work, a rendering algorithm, or integrator, will send many queries to the sample
generator. Generally, it will request subsequent 1D or 2D components of this infinite-dimensional
*point* and map them into a more convenient space (for instance, positions on surfaces). This allows
it to construct light paths to eventually evaluate the flow of light through the scene.
