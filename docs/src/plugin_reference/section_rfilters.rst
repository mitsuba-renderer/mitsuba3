.. _sec-rfilters:

Reconstruction filters
======================

Image reconstruction filters are responsible for converting a series of radiance samples generated
jointly by the sampler and integrator into the final output image that will be written to disk at
the end of a rendering process. This section gives a brief overview of the reconstruction filters
that are available in Mitsuba. There is no universally superior filter, and the final choice depends
on a trade-off between sharpness, ringing, and aliasing, and computational efficiency.

Desireable properties of a reconstruction filter are that it sharply captures all of the details
that are displayable at the requested image resolution, while avoiding aliasing and ringing.
Aliasing is the incorrect leakage of high-frequency into low-frequency detail, and ringing denotes
oscillation artifacts near discontinuities, such as a light-shadow transiton.