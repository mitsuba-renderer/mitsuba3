.. _sec-animation:

Animation and motion blur
=========================

Mitsuba can interpolate a transformation over time and integrate over a camera
shutter interval to render motion blur. This document covers what can be
animated, how a ray obtains its time value, and how each ray tracing backend
resolves the resulting motion.

The XML syntax of the ``<animation>`` tag itself is described in
:ref:`sec-file-format`.

Overview
--------

An animated transformation is a sequence of keyframes, each pairing a time with
a transformation matrix. Every keyframe is decomposed into a **scale**, a
**rotation** (quaternion) and a **translation**. Evaluating the animation at
some time interpolates rotations using `spherical linear interpolation (Slerp)
<https://en.wikipedia.org/wiki/Spherical_linear_interpolation>`_ and scales and
translations linearly.

Two consequences follow from the decomposition:

* Transformations containing **shear** cannot be represented and are rejected
  with an error. (A single, non-animated keyframe is fine: it is evaluated as
  the original matrix and never goes through the decomposition.)

* Keyframe times must be **distinct**. Evaluating outside of the keyframe time
  range **clamps** to the first or last keyframe, respectively, rather than
  extrapolating.

In XML, an animation replaces the ``<transform>`` tag:

.. code-block:: xml

    <animation name="to_world">
        <transform time="0.0">
            <translate value="0, 0, 0"/>
        </transform>
        <transform time="1.0">
            <translate value="1, 0, 0"/>
        </transform>
    </animation>

An ``<animation>`` may carry an ``id`` so that several objects can share one set
of keyframes via ``<ref>``.

From Python, pass a :py:class:`mitsuba.AnimatedTransform4f` wherever a
transformation is expected. It accepts a dictionary mapping times to
transformations, or a list of ``(time, transform)`` pairs:

.. code-block:: python

    from mitsuba import ScalarTransform4f as T

    scene = mi.load_dict({
        'type': 'scene',
        'group': {'type': 'shapegroup', 'shape': {'type': 'sphere'}},
        'instance': {
            'type': 'instance',
            'group': {'type': 'ref', 'id': 'group'},
            'to_world': mi.AnimatedTransform4f({
                0.0: T().translate([0, 0, 0]),
                1.0: T().translate([0, 0, 1])
            })
        }
    })

.. _sec-animation-shutter:

The shutter interval
--------------------

.. important::

    Animating an object is not by itself enough to see motion. Rays carry a
    ``time`` value, and by default **every ray is traced at time 0**, which
    renders an animated scene as if it were frozen at its first keyframe. To
    see motion blur, open the sensor's shutter.

A sensor exposes two parameters that define the shutter interval:

.. list-table::
    :header-rows: 1
    :widths: 25 15 60

    * - Parameter
      - Default
      - Description
    * - ``shutter_open``
      - ``0.0``
      - Time at which the shutter opens
    * - ``shutter_close``
      - ``0.0``
      - Time at which the shutter closes

Each camera ray then draws its time uniformly from that interval, using one
sample dimension of the sampler:

.. code-block:: text

    time = shutter_open + next_1d() * (shutter_close - shutter_open)

Because ``shutter_close`` defaults to ``0.0``, the interval is empty unless it
is set explicitly, and every ray receives ``time = shutter_open``. The shutter
interval and the keyframe times live on the same, arbitrary time axis; it is up
to the scene to keep them consistent. A shutter that covers only part of the
keyframe range simply renders that part of the motion.

.. code-block:: xml

    <sensor type="perspective">
        <float name="shutter_open" value="0.0"/>
        <float name="shutter_close" value="1.0"/>
        <!-- ... -->
    </sensor>

The number of distinct time samples per pixel equals the sample count, so motion
blur generally needs a higher ``sample_count`` than a static render of the same
scene to avoid a "ghosting" appearance of a few discrete copies.

What can be animated
--------------------

.. list-table::
    :header-rows: 1
    :widths: 28 20 52

    * - Object
      - Animated ``to_world``
      - Notes
    * - Sensors
      - Yes
      - Keyframes may be spaced arbitrarily.
    * - ``point``, ``spot``, ``directional``, ``projector``, ``envmap``
      - Yes
      - Keyframes may be spaced arbitrarily.
    * - ``sunsky``, ``timed_sunsky``
      - No
      - Rejected with an error, see :ref:`below <sec-animation-emitters>`.
    * - Area emitters
      - No
      - Not expressible, see :ref:`below <sec-animation-emitters>`.
    * - Shapes
      - Only via ``instance``
      - Requires **evenly spaced** keyframes, see
        :ref:`below <sec-animation-shapes>`.

Deforming geometry is not supported: only rigid transformations of whole objects
can be animated. Vertex positions cannot vary over time.

Animated sensors
----------------

The ``perspective``, ``thinlens``, ``orthographic``, ``radiancemeter`` and
``distant`` sensors evaluate their ``to_world`` transformation at the time of
each ray, so a moving or rotating camera produces camera motion blur without any
further setup. The same transformation is used when a light path connects back
to the sensor, so bidirectional techniques stay consistent.

.. code-block:: xml

    <sensor type="perspective">
        <float name="shutter_open" value="0.0"/>
        <float name="shutter_close" value="1.0"/>

        <animation name="to_world">
            <transform time="0.0">
                <lookat origin="0, 0, -5" target="0, 0, 0" up="0, 1, 0"/>
            </transform>
            <transform time="1.0">
                <lookat origin="2, 0, -5" target="0, 0, 0" up="0, 1, 0"/>
            </transform>
        </animation>
    </sensor>

.. image:: ../../../resources/data/docs/images/render/animation_sensor.jpg
    :width: 100%
    :align: center

Here *nothing in the scene moves*: the camera dollies sideways, so the whole
frame smears, floor included
(:monosp:`docs/scenes/animation_sensor.xml`).

Camera transformations may not contain scale factors, animated or not; the
``perspective`` and ``thinlens`` plugins reject them.

.. _sec-animation-emitters:

Animated emitters
-----------------

Emitters that own a ``to_world`` transformation interpolate it at the time of
the interaction being shaded, so a moving light source blurs correctly in both
directions of transport.

.. subfigstart::
.. subfigure:: ../../../resources/data/docs/images/render/animation_emitter_t0.jpg
   :caption: Shutter closed at :math:`t = 0`
.. subfigure:: ../../../resources/data/docs/images/render/animation_emitter.jpg
   :caption: Shutter open over :math:`[0, 1]`
.. subfigure:: ../../../resources/data/docs/images/render/animation_emitter_t1.jpg
   :caption: Shutter closed at :math:`t = 1`
.. subfigend::
    :label: fig-animation-emitter

A spot light travels in an arc around a static bunny
(:monosp:`docs/scenes/animation_emitter.xml`). The geometry stays sharp while
the shadow smears between the two extremes.

Note that the light's *position* has to move, not merely its aim: shadows are
cast from where the light is, so a spot that only pivots leaves its shadow
perfectly still and produces no recognisable motion. Note also that a single
blurred frame is genuinely ambiguous -- a smeared shadow is hard to distinguish
from a soft shadow cast by a large area light -- which is why the two shutter
extremes are shown alongside it here.

Two limitations are worth knowing about:

**Area emitters cannot be animated.** An area emitter is attached to a shape,
and a shape can only be animated by placing it inside a ``shapegroup`` and
referencing it from an ``instance``. Because ``shapegroup`` rejects shapes that
carry an emitter ("Instancing of emitters is not supported"), the two features
cannot be combined. Use an analytic emitter for moving light sources.

**The sunsky emitters reject animated transformations.** Rotating a sky dome
with keyframes would not reproduce the correct sun position or sky appearance.
Use ``timed_sunsky`` instead, which is itself time-dependent: it maps the ray
time through its own ``shutter_open`` / ``shutter_close`` parameters onto a
date and time-of-day window (``window_start_time``, ``window_end_time`` and the
``start_*`` / ``end_*`` date parameters) and evaluates the physically correct sun
position for that moment.

.. _sec-animation-shapes:

Animated shapes
---------------

Everything above describes how Mitsuba itself evaluates an animated
transformation. Shapes are different: they are animated through the ``instance``
plugin, and the resulting motion is resolved by the acceleration structure of
the active ray tracing backend. Those backends impose extra requirements and do
not all interpolate rotations the same way.

.. code-block:: xml

    <shape type="shapegroup" id="my_group">
        <shape type="sphere"/>
    </shape>

    <shape type="instance">
        <ref id="my_group"/>
        <animation name="to_world">
            <transform time="0.0">
                <translate value="0, 0, 0"/>
            </transform>
            <transform time="1.0">
                <translate value="0, 0, 1"/>
            </transform>
        </animation>
    </shape>

.. image:: ../../../resources/data/docs/images/render/animation_shape.jpg
    :width: 100%
    :align: center

The bunny sweeps across the frame while turning slightly; the camera and the
lighting are static, so only the instance smears
(:monosp:`docs/scenes/animation_shape.xml`).

.. note::

    Animation of shapes requires **evenly spaced** keyframes due to constraints
    in the OptiX and Embree APIs/implementations. An instance whose keyframes
    are not uniformly spaced is rejected with an error. This restriction does
    not apply to sensors and emitters.

Backend differences
*******************

Support and rotation behavior per backend:

.. list-table::
    :header-rows: 1
    :widths: 20 20 60

    * - Backend
      - Rotation
      - Animated instances
    * - Embree (``llvm_*``, ``scalar_*``)
      - Slerp
      - Fully supported; matches Mitsuba's own evaluation.
    * - OptiX (``cuda_*``)
      - Nlerp
      - Fully supported, but the SRT motion transform interpolates the
        quaternion components *linearly* and renormalizes them.
    * - Metal (``metal_*``)
      - Linear
      - Fully supported, but Metal's motion instance descriptors interpolate the
        keyframe *matrices* linearly rather than the rotation itself.
    * - Native kd-tree
      - --
      - Instancing is unsupported altogether, so the scene is rejected with an
        error before animation matters.

The scene below makes the three rules visible. A teapot rotates 150 degrees
about the vertical axis between two keyframes, and the shutter is *closed*
(``shutter_open == shutter_close``) so that each image is the instantaneous
pose rather than a blur (:monosp:`docs/scenes/animation_rotation.xml`).

.. subfigstart::
.. subfigure:: ../../../resources/data/docs/images/render/animation_rotation_embree.jpg
   :caption: Embree, slerp (:math:`-40.5^\circ`)
.. subfigure:: ../../../resources/data/docs/images/render/animation_rotation_optix.jpg
   :caption: OptiX, nlerp (:math:`-45.0^\circ`)
.. subfigure:: ../../../resources/data/docs/images/render/animation_rotation_metal.jpg
   :caption: Metal, matrix lerp (:math:`-63.6^\circ`, shrunk)
.. subfigend::
    :label: fig-animation-rotation

The sampled time matters when comparing backends. Slerp and nlerp agree
*exactly* at :math:`t = 0`, :math:`t = 0.5` and :math:`t = 1`, because the
normalised average of two quaternions is the midpoint of the great-circle arc
between them. Their disagreement peaks near :math:`t = 0.23` and
:math:`t = 0.77`, which is the moment shown above.

Opening the shutter over the whole interval turns those pose differences into
differences in the final image:

.. subfigstart::
.. subfigure:: ../../../resources/data/docs/images/render/animation_rotation_blur_embree.jpg
   :caption: Embree, slerp
.. subfigure:: ../../../resources/data/docs/images/render/animation_rotation_blur_optix.jpg
   :caption: OptiX, nlerp
.. subfigure:: ../../../resources/data/docs/images/render/animation_rotation_blur_metal.jpg
   :caption: Metal, matrix lerp
.. subfigend::
    :label: fig-animation-rotation-blur

Slerp and nlerp sweep the same arc between the same endpoints and differ only in
how fast they traverse it, so the two blurs cover the same region and differ
only in density -- visible in the smeared spout, but easy to miss. Metal is a
different matter: because the shrink is worst in the middle of the sweep (down
to 0.26 of the original width for a 150 degree turn), the teapot spends most of
the shutter collapsed, and the blur bears little resemblance to a rotating
object at all.

Nlerp and slerp follow the same rotation path, but nlerp traverses it at a
non-constant angular rate, so a CUDA render can differ slightly from an LLVM one
for instances that rotate a lot between two keyframes. The deviation depends
only on the angle between the two keyframes, and stays below 0.1 degrees up to a
rotation of roughly 43 degrees. Beyond that it grows quickly: about 0.9 degrees
at 90 degrees, and 4.5 degrees at 150 degrees. Subdividing large rotations into
additional keyframes reduces the difference.

The Metal backend deviates the most, because Metal interpolates the
transformation matrices themselves. A linear blend of two rotation matrices is
not a rotation: it both turns by the wrong angle and *shrinks*. In the example
above the axes in the plane of rotation contract to 0.58 of their length, and at
the midpoint of a 150 degree turn they contract to 0.26. As above, subdividing
the rotation into more keyframes reduces the error -- halving the angle between
keyframes roughly quarters the shrink.

In all three backends, a ray whose time falls outside an instance's keyframe
range is clamped to the first or last keyframe, matching Mitsuba's own
evaluation.

Animation and ``mitsuba.traverse()``
------------------------------------

Through :py:func:`mitsuba.traverse()`, a transformation exposes its keyframes as
four tensors, one per interpolated component:

.. list-table::
    :header-rows: 1
    :widths: 22 18 60

    * - Parameter
      - Shape
      - Contents
    * - ``times``
      - ``(N,)``
      - Keyframe times, strictly increasing
    * - ``scale``
      - ``(N, 3)``
      - Per-axis scale factors
    * - ``rotation``
      - ``(N, 4)``
      - Rotation quaternions, ``(x, y, z, w)``
    * - ``translation``
      - ``(N, 3)``
      - Translations

Editing one component leaves the others untouched:

.. code-block:: python

    params = mi.traverse(scene)
    t = mi.TensorXf(params['instance.to_world.translation'])
    t[1, 0] = 2.5                                   # second keyframe, x axis
    params['instance.to_world.translation'] = t
    params.update()

The four tensors are views into a single packed buffer, so they must always
agree on the number of keyframes. **Changing the number of keyframes therefore
means writing all four together**, with a matching row count; writing only some
of them raises an error. Since the set of exposed parameters depends on the
keyframe count (see below), :py:func:`mitsuba.traverse()` has to be called again
afterwards.

.. code-block:: python

    params = mi.traverse(animated)
    params['times']       = mi.TensorXf([0.0, 1.0], shape=(2,))
    params['scale']       = mi.TensorXf([1.0] * 6, shape=(2, 3))
    params['rotation']    = mi.TensorXf([0, 0, 0, 1] * 2, shape=(2, 4))
    params['translation'] = mi.TensorXf([0, 0, 0,
                                        1, 0, 0], shape=(2, 3))
    params.update()

While the transformation holds a **single** keyframe it additionally exposes a
``transform`` parameter holding the plain 4x4 matrix. That matrix is the
representation actually evaluated in that case, and it takes precedence if it is
written together with the views:

.. code-block:: python

    params['sensor.to_world.transform'] = mi.Transform4f().translate([0, 0, 1])


