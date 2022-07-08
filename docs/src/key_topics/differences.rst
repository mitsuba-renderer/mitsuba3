.. _sec-file-format:

Differences to previous versions
================================

Differences to Mitsuba 2
------------------------

Porting Mitsuba 2 Python scripts and C++ plugins to Mitsuba 3 is straightforward
as Mitsuba 3 only underwent minimal API changes. Moreover, Mitsuba 3 maintains
scene compatibility with Mitsuba 2.

Enoki → Dr.Jit
^^^^^^^^^^^^^^^

One of the biggest change is the replacement of the Enoki backend with Dr.Jit.
To account for this in your code, simply replace the namespace definition and
aliases as follow:

.. tabs::
    .. code-tab:: c++

        // Before
        namespace ek = enoki;
        ... = ek::sin(...);

        // Now
        namespace dr = drjit;
        ... = dr::sin(...);

    .. code-tab:: python

        # Before
        import enoki as ek
        ... = ek.sin(...)

        # Now
        import drjit as dr
        ... = dr.sin(...)

Moreover, note the following non-exhaustive list API changes in Dr.Jit (also
valid for C++):

- `ek.zero(..)` → `dr.zeros(..)`
- `ek.min(..)` → `dr.minimum(..)`
- `ek.max(..)` → `dr.maximum(..)`
- `ek.hmin(..)` → `dr.min(..)`
- `ek.hmax(..)` → `dr.max(..)`
- `ek.hsum(..)` → `dr.sum(..)`
- `ek.hprod(..)` → `dr.prod(..)`
- `ek.hmean(..)` → `dr.mean(..)`
- `ek.hsum_async(..)` → `dr.sum(..)`
- `ek.hprod_async(..)` → `dr.prod(..)`
- `ek.hmean_async(..)` → `dr.mean(..)`
- `dr.is_diff_array_v` → `dr.is_diff_v`
- `dr.is_jit_array_v` → `dr.is_jit_v`

In Python Dr.Jit constants switch from CamelCase to snake_case:

- `ek.Pi` → `dr.pi`
- `ek.TwoPi` → `dr.two_pi`
- ...

To learn more about Dr.Jit and its dissimilarities with Enoki, see the `Dr.Jit
documentation <https://readthedocs.org/projects/drjit/badge/?version=latest>`_.

Differences to Mitsuba 0.6
--------------------------

Mitsuba 3 strives to retain scene compatibility with its predecessor Mitsuba
0.6. However, in most other respects, it is a completely new system following a
different set of goals.

Missing features
^^^^^^^^^^^^^^^^

A number of Mitsuba 0.6 features are missing in Mitsuba 2. We plan to port some
of these features in the future and discard others. The following list provides
an overview:

In the following, we list the main missing features:

- **Shapes**: the basic shapes (PLY/OBJ/Serialized triangle meshes, rectangles, spheres, cylinders) are all supported. However, instancing and assemblies of
  hair fibers are still missing.

- **Integrators**: Only surface and volumetric path tracers are provided. The
  following are missing:

  * **Bidirectional path tracing**
  * **Progressive photon mapping**
  * **Path space Metropolis light transport / Manifold exploration / Energy
    redistribution path tracing:**

    Path-space MCMC techniques were an incredibly complex component of the
    previous generation of Mitsuba. We do not currently plan to port these and
    recommend that you stick with Mitsuba 0.6 if your application depends on
    them.

- **Animation**: specification and handling of temporally varying
  transformations, e.g., for motion blur is not yet implemented.

- **Diffusion-based subsurface scattering**: currently missing,
  status undecided.


Scene format
^^^^^^^^^^^^

Mitsuba 3's XML scene format is almost identical to that of Mitsuba 0.6.
Most plugins have the same name and same parameters, and we made sure that
parameters behave in the same way as in Mitsuba 0.6.

One significant change is that Mitsuba 3 uses "``underscore_case``"-style
instead of "``camelCase``"-style capitalization. This is part of a concerted
change to the entire renderer that also touches all C++ and Python interfaces
(see the developer guide for reasons behind this).
To given an example: a perspective camera definition, which
might have looked like the following in Mitsuba 0.6

.. code-block:: xml

    <sensor type="perspective">
        <string name="fovAxis" value="smaller"/>
        <float name="nearClip" value="10"/>
        <float name="farClip" value="2800"/>
        <float name="focusDistance" value="1000"/>
        <transform name="toWorld">
            <translate x="0" y="0" z="-100"/>
        </transform>
        ...
    </sensor>

now reads

.. code-block:: xml

    <sensor type="perspective">
        <string name="fov_axis" value="smaller"/>
        <float name="near_clip" value="10"/>
        <float name="far_clip" value="2800"/>
        <float name="focus_distance" value="1000"/>
        <transform name="to_world">
            <translate value="0, 0, -100"/>
        </transform>
        ...
    </sensor>

The above snippet also shows an unrelated change: the preferred syntax for
specifying positions, translations, etc., was shortened:

.. code-block:: xml

    <!-- old notation -->
    <point name="position" x="0" y="0" z="-100"/>

    <!-- new notation -->
    <point name="position" value="0, 0, -100"/>

All of these changes can be automated, and Mitsuba performs them internally
when it detects a scene with a version number lower than :monosp:`2.0.0`.
Invoke the ``mitsuba`` binary with the ``-u`` parameter if you would like it to
write the updated scene description back to disk.