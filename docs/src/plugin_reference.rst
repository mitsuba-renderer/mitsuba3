.. _sec-plugins:

.. image:: ../resources/data/docs/images/banners/banner_06.jpg
    :width: 100%
    :align: center

Plugin reference
================

.. toctree::
    :maxdepth: 1
    :glob:

    generated/plugins_*

Overview
--------

The subsections above describe the available Mitsuba 3 plugins, usually along with example
renderings and a description of what each parameter does. They are separated into subsections
covering textures, surface scattering models, etc.

The documentation of a plugin always starts with a table similar to the one below:

.. pluginparameters::

 * - soft_rays
   - |bool|
   - Try not to damage objects in the scene by shooting softer rays (Default: false)
 * - dark_matter
   - |float|
   - Controls the proportionate amount of dark matter present in the scene. (Default: 0.42)
 * - (Nested plugin)
   - :paramtype:`integrator`
   - A nested integrator which does the actual hard work

Suppose this hypothetical plugin is an integrator named ``amazing``. Then, based on this
description, it can be instantiated from an XML scene file using a custom configuration such as:

.. tabs::
    .. code-tab:: xml

        <integrator type="amazing">
            <boolean name="softer_rays" value="true"/>
            <float name="dark_matter" value="0.44"/>
        </integrator>

    .. code-tab:: python

        {
            'type': 'amazing',
            'softer_rays': True,
            'dark_matter': 0.44
        }

In some cases, plugins also indicate that they accept nested plugins as input arguments. These can
either be *named* or *unnamed*. If the ``amazing`` integrator also accepted the following two parameters:

.. pluginparameters::


 * - (Nested plugin)
   - :paramtype:`integrator`
   - A nested integrator which does the actual hard work
 * - puppies
   - :paramtype:`texture`
   - This must be used to supply a cute picture of puppies

then it can be instantiated e.g. as follows:

.. tabs::
    .. code-tab:: xml

        <integrator type="amazing">
            <boolean name="softer_rays" value="true"/>
            <float name="dark_matter" value="0.44"/>
            <!-- Nested unnamed integrator -->
            <integrator type="path"/>
            <!-- Nested texture named puppies -->
            <texture name="puppies" type="bitmap">
                <string name="filename" value="cute.jpg"/>
            </texture>
        </integrator>

    .. code-tab:: python

        {
            'type': 'amazing',
            'softer_rays': True,
            'dark_matter': 0.44,
            # Nested unnamed integrator
            'foo': {
                'type': 'path'
            },
            # Nested texture named puppies
            'puppies': {
                'type': 'bitmap',
                'filename': 'cute.jpg'
            }
        }

Flags
^^^^^^

Each parameter optionally has flags that are listed in the last column.
These flags indicate whether the parameter is differentiable or not, or whether it introduces
discontinuities and thus needs special treatment.

See :py:class:`mitsuba.ParamFlags` for their documentation.


Scene-wide attributes
---------------------

The Scene object exposes scene-wide attributes.

**Embree BVH mode:** We expose a scene-level flag to enable Embree's "robust"
mode. Enabling this flag makes Embree use slightly slower but more robust ray
intersection computations. Embree's default "fast" mode can miss ray
intersections when a ray perfectly hits the edge between two adjacent triangles.
Enabling the robust intersection mode fixes that in most cases. Note that Embree
cannot guarantee that all intersections are reported for rays that exactly hit a
vertex, which is a known limitation.

**Thread reordering:** Ray intersection methods on the scene can take a
`reorder` argument to specifiy whether or not threads should be shuffled into
coherent groups after the intersection (shader execution reordering). Both this
flag and the one passed to the method must be set in order for the reordering to
take place. By splitting this responsability, we allow users to indiciate if
rendering the scene benefits from SER without having to modify the ray
intersection call site. This feature is only relevant for the CUDA backend and
has no effect in scalar or LLVM variants.


.. pluginparameters::

 * - embree_use_robust_intersections
   - :paramtype:`bool`
   - Whether Embree uses the robust mode flag `RTC_SCENE_FLAG_ROBUST` (Default: |false|).
 * - allow_thread_reordering
   - :paramtype:`bool`
   - Whether or not to reorder threads into coherent groups after a ray
     intersection if requested (Default: |true|).
   - |exposed|

When creating a scene, the scene-wide attributes can be specified as follows:

.. tabs::
    .. code-tab:: xml

        <scene version="3.0.0">
            <boolean name="embree_use_robust_intersections" value="true"/>
        </scene>

    .. code-tab:: python

        {
            'type': 'scene',
            'embree_use_robust_intersections': True,
            'allow_thread_reordering': True,
        }
