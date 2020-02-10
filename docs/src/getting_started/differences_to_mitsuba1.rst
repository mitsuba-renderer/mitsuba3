Differences to Mitsuba 0.6
===========================
This section is meant to give a compact overview over some of the main changes in Mitsuba 2 compared to Mitsuba 0.6.

Scene format
--------------

Mitsuba 2's XML scene format is almost identical to the one from Mitsuba 0.6.
Most plugins have the same name and same parameters, and we made sure that parameters behave in the same way as in Mitsuba 0.6.

The one main change is that Mitsuba 2 use snake_case instead of camelCase.
A perspective camera definition in Mitsuba 0.6 looked something like

.. code-block:: xml

    <sensor type="perspective">
        <string name="fovAxis" value="smaller"/>
        <float name="nearClip" value="10"/>
        <float name="farClip" value="2800"/>
        <float name="focusDistance" value="1000"/>
        <transform name="toWorld">
            <lookat origin="278, 273, -800"
                    target="278, 273, -799"
                    up    ="  0,   1,    0"/>
        </transform>
        ...
    </sensor>


In Mitsuba 2, the same camera is defined as

.. code-block:: xml

    <sensor type="perspective">
        <string name="fov_axis" value="smaller"/>
        <float name="near_clip" value="10"/>
        <float name="far_clip" value="2800"/>
        <float name="focus_distance" value="1000"/>
        <transform name="to_world">
            <lookat origin="278, 273, -800"
                    target="278, 273, -799"
                    up    ="  0,   1,    0"/>
        </transform>
        ...
    </sensor>



Command line interface
----------------------
The command line interface is very similar to Mitsuba 0.6.
It is still possible to set parameters using ``-Dparameter=0.5``.
One difference is that the thread count is now specified using ``-t 12`` parameter instead of ``-p 12``.



Features currently not supported in Mitsuba 2
---------------------------------------------------------
While Mitsuba 2 already supports a wide range of different integrators, BSDFs and participating media, there
are some features present in Mitsuba 0.6 which are currently not supported in Mitsuba 2.

In the following we list the main missing features

- Path space Metropolis light transport / Manifold exploration
- Bidirectional path tracing
- Photon mapping
- Energy redistribution path tracing
- Analytic shape for hair
- Quasi Monte Carlo or stratified sampling
- Motion blur
- Diffusion-based subsurface scattering

That being said, Mitsuba 2 provides a range of improvements compared to Mitsuba 0.6: full spectral rendering, polarized rendering, vectorized and differentiable rendering.