.. _sec-rfilters:

Reconstruction filters
======================

Reconstruction filters are responsible for converting a series of samples generated jointly by the
sampler and integrator into the final output that will be written to disk at the end of a rendering
process. This section gives a brief overview of the reconstruction filters that are available in
misuka. There is no universally superior filter, and the final choice depends on a trade-off between
sharpness, ringing, and aliasing, and computational efficiency.

Desirable properties of a reconstruction filter are that it sharply captures all of the details
that are displayable at the requested output resolution, while avoiding aliasing and ringing.
Aliasing is the incorrect leakage of high-frequency into low-frequency detail, and ringing denotes
oscillation artifacts near discontinuities, such as a light-shadow transition in an image.

.. note::

    The filters below are inherited from Mitsuba unchanged, but acoustically they act along a
    different axis. A conventional film reconstructs over the two *spatial* axes of an image,
    whereas the :ref:`tape <film-tape>` film reconstructs over *time* only. The filter width is
    therefore measured in time bins and controls how far a single arrival is smeared across
    neighboring bins. Frequency bands are never filtered into each other, no matter which filter
    is chosen.

    As for any film, the default is a :ref:`gaussian <rfilter-gaussian>` filter. A
    :ref:`box <rfilter-box>` filter is often the better choice for forward rendering, because it
    deposits each arrival in the single bin matching its propagation time and so leaves the ETC
    temporally unsmoothed. The :ref:`differentiable acoustic integrators <sec-integrators>` are
    the exception. Whenever one of them tracks derivatives with respect to time, which is what
    gradients with respect to time-dependent scene parameters such as geometry need, the
    reconstruction filter has to be differentiable for those time gradients to be correct.
    :ref:`acoustic_ad <integrator-acoustic_ad>` and
    :ref:`acoustic_ad_threepoint <integrator-acoustic_ad_threepoint>` always track them, the
    :ref:`acoustic_prb <integrator-acoustic_prb>` variants whenever ``track_time_derivatives``
    is enabled. A gaussian with ``stddev`` set to 0.25 time bins is recommended
    there, since it permits gradient estimation without smoothing the ETC significantly. See
    :ref:`sec-acoustic-rendering` for the ETC and its time axis.
