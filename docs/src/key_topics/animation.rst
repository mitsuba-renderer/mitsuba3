.. _sec-animation:

Animation and motion blur
=========================

Mitsuba can represent and render motion blur due to moving sensors, emitters,
and shapes. This page explains how to specify animated content and describes
backend-specific variation.

Overview
--------

An animated transformation consists of a sequence of keyframes, each pairing a
time with a transformation. In XML, the ``<animation>`` tag takes the place of
``<transform>`` and contains one ``<transform>`` per keyframe, each carrying a
``time`` attribute. In Python, pass a :py:class:`mitsuba.AnimatedTransform4f`
constructed from a dictionary of time-transform pairs (or a list of ``(time,
transform)`` tuples) wherever a transform is expected.

.. tabs::
    .. code-tab:: xml

        <animation name="to_world">
            <transform time="0.0">
                <translate value="0, 0, 0"/>
            </transform>
            <transform time="1.0">
                <translate value="1, 0, 0"/>
            </transform>
        </animation>

    .. code-tab:: python

        'to_world': mi.AnimatedTransform4f({
            0.0: mi.ScalarTransform4f.translate([0, 0, 0]),
            1.0: mi.ScalarTransform4f.translate([1, 0, 0]),
        })

The ``<animation>`` tag may carry an ``id`` so that several objects can share it
via ``<ref>``. See the :ref:`XML scene format description <sec-file-format>`
for details on references.

An animation requires at least two keyframes with distinct times. Use a plain
``<transform>`` for static objects. Animations must be free of shear, which the
interpolated representation cannot express.

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
      - Arbitrary keyframe spacing.
    * - ``point``, ``spot``, ``directional``, ``projector``, ``envmap``
      - Yes
      - Arbitrary keyframe spacing.
    * - ``sunsky``, ``timed_sunsky``
      - No
      - Rejected with an error, see :ref:`below <sec-animation-emitters>`.
    * - Area emitters
      - No
      - Not expressible, see :ref:`below <sec-animation-emitters>`.
    * - Shapes
      - Only via ``instance``
      - Requires **evenly spaced** keyframes and is **not differentiable**,
        see :ref:`below <sec-animation-shapes>`.

Only whole objects can be animated. Deforming geometry is not supported.

.. _sec-animation-shutter:

Sensor shutter
--------------

Motion blur integrates over the sensor's shutter interval specified by the
``shutter_open`` and ``shutter_close`` parameters. Forgetting to do so means
that the scene is rendered at time ``0.0`` without motion blur, since both
parameters default to this value.

.. tabs::
    .. code-tab:: xml

        <sensor type="perspective">
            <float name="shutter_open" value="0.0"/>
            <float name="shutter_close" value="1.0"/>
            <!-- ... -->
        </sensor>

    .. code-tab:: python

        {
            'type': 'perspective',
            'shutter_open': 0.0,
            'shutter_close': 1.0,
            # ...
        }

Keyframes and the shutter interval share the same arbitrary time units. The
keyframes should roughly cover the shutter interval of the frame being
rendered. Storing a much longer sequence of keyframes wastes memory and slows
down ray tracing.

Sensor motion
-------------

The ``perspective``, ``thinlens``, ``orthographic``, ``radiancemeter``, and
``distant`` sensors accept an animated ``to_world`` transformation.

.. tabs::
    .. code-tab:: xml

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

    .. code-tab:: python

        {
            'type': 'perspective',
            'shutter_open': 0.0,
            'shutter_close': 1.0,
            'to_world': mi.AnimatedTransform4f({
                0.0: mi.ScalarTransform4f.look_at(origin=[0, 0, -5], target=[0, 0, 0], up=[0, 1, 0]),
                1.0: mi.ScalarTransform4f.look_at(origin=[2, 0, -5], target=[0, 0, 0], up=[0, 1, 0]),
            }),
        }

.. image:: ../../../resources/data/docs/images/render/animation_sensor.jpg
    :width: 100%
    :align: center

In this example, the camera dollies sideways during the shutter interval while
the scene remains static. The entire frame blurs as a result.

.. _sec-animation-emitters:

Animated emitters
-----------------

Emitters accept an animated ``to_world`` transformation in the same way.

.. tabs::
    .. code-tab:: xml

        <emitter type="spot">
            <animation name="to_world">
                <transform time="0.0">
                    <lookat origin="-3.7, -2.1, 3.6" target="0.3, 0.0, 0.6" up="0, 0, 1"/>
                </transform>
                <transform time="1.0">
                    <lookat origin="-0.9, -4.4, 3.6" target="0.3, 0.0, 0.6" up="0, 0, 1"/>
                </transform>
            </animation>
        </emitter>

    .. code-tab:: python

        {
            'type': 'spot',
            'to_world': mi.AnimatedTransform4f({
                0.0: mi.ScalarTransform4f.look_at(origin=[-3.7, -2.1, 3.6], target=[0.3, 0.0, 0.6], up=[0, 0, 1]),
                1.0: mi.ScalarTransform4f.look_at(origin=[-0.9, -4.4, 3.6], target=[0.3, 0.0, 0.6], up=[0, 0, 1]),
            }),
        }

.. subfigstart::
.. subfigure:: ../../../resources/data/docs/images/render/animation_emitter_t0.jpg
   :caption: Shutter closed at t = 0
.. subfigure:: ../../../resources/data/docs/images/render/animation_emitter.jpg
   :caption: Shutter open over [0, 1]
.. subfigure:: ../../../resources/data/docs/images/render/animation_emitter_t1.jpg
   :caption: Shutter closed at t = 1
.. subfigend::
    :label: fig-animation-emitter

In this example, a spot light moves along an arc around a static bunny. The
geometry stays sharp while the cast shadow smears across the shutter interval.

Two kinds of emitters cannot be animated:

- **Area emitters**. Shapes are animated through instancing, and instances
  cannot carry emitters.
- **Sun and sky emitters**. Rotating the sky dome does not model the sun's
  trajectory. The ``timed_sunsky`` plugin instead moves the sun based on date
  and time parameters.

.. _sec-animation-shapes:

Animated shapes
---------------

Animating shapes requires instancing via the :ref:`instance <shape-instance>`
plugin. The
ray tracing backend (Embree, OptiX, or Metal) then evaluates the motion during
traversal.

.. tabs::
    .. code-tab:: xml

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

    .. code-tab:: python

        'my_group': {
            'type': 'shapegroup',
            'shape': {'type': 'sphere'},
        },
        'my_instance': {
            'type': 'instance',
            'shape': {'type': 'ref', 'id': 'my_group'},
            'to_world': mi.AnimatedTransform4f({
                0.0: mi.ScalarTransform4f.translate([0, 0, 0]),
                1.0: mi.ScalarTransform4f.translate([0, 0, 1]),
            }),
        }

.. image:: ../../../resources/data/docs/images/render/animation_shape.jpg
    :width: 100%
    :align: center

In this example, the bunny sweeps across the frame while turning slightly, under
a static camera and static lighting.

Animated instances come with two restrictions that do not apply to sensors and
emitters:

- **Keyframes must be evenly spaced**. The backends describe motion as a
  keyframe count plus a time interval. Unevenly spaced keyframes raise an
  error.
- **Instance motion is not differentiable**. The backends build their
  acceleration data structures from host-side keyframes, and gradients do not
  propagate to the keyframe parameters of an instance.

Backend differences
*******************

Mitsuba itself evaluates the motion of sensors and emitters. It interpolates
rotations using `spherical linear interpolation ("slerp")
<https://en.wikipedia.org/wiki/Spherical_linear_interpolation>`_, which turns
at a constant angular speed. The ray tracing backend instead evaluates the
motion of instances, and the backends differ in two respects.

.. list-table::
    :header-rows: 1
    :widths: 34 22 44

    * - Backend
      - Rotation interpolation
      - Rays outside the keyframe range
    * - Embree (``llvm_*``, ``scalar_*``)
      - slerp
      - Instance is invisible
    * - OptiX (``cuda_*``) and Metal (``metal_*``)
      - nlerp
      - Instance frozen at the nearest keyframe

Embree furthermore limits each instance to 129 keyframes.

**Rotation interpolation**. OptiX and Metal blend quaternion components
linearly and normalize the result ("nlerp"). This follows the same rotational arc
as slerp but at a non-constant angular speed. The two agree at the keyframes
and at the midpoint between them, and their difference grows with the rotation
between adjacent keyframes: it stays below 0.1° for steps up to about 40° and
reaches 4.5° for a 150° step. Subdividing large rotations with additional
keyframes reduces it.

The teapot below rotates by 150° about the vertical axis between two keyframes
(:monosp:`resources/data/docs/scenes/animation_rotation.xml`). The first pair
of images renders an instantaneous pose using a closed shutter, and the second
pair opens the shutter over the full interval.

.. subfigstart::
.. subfigure:: ../../../resources/data/docs/images/render/animation_rotation_embree.jpg
   :caption: Embree, slerp (-40.5°)
.. subfigure:: ../../../resources/data/docs/images/render/animation_rotation_optix.jpg
   :caption: OptiX and Metal, nlerp (-45.0°)
.. subfigend::
    :label: fig-animation-rotation

.. subfigstart::
.. subfigure:: ../../../resources/data/docs/images/render/animation_rotation_blur_embree.jpg
   :caption: Embree, slerp
.. subfigure:: ../../../resources/data/docs/images/render/animation_rotation_blur_optix.jpg
   :caption: OptiX and Metal, nlerp
.. subfigend::
    :label: fig-animation-rotation-blur

The blurred silhouettes have the same extent but differ in density, which is
visible in the teapot's spout.

**Rays outside the keyframe range**. Instances may use different keyframe
ranges, and the scene-wide time range is their union. The backends disagree
about what happens when a ray's time falls outside the range of a particular
instance: OptiX and Metal freeze it at the first or last keyframe, whereas
Embree makes it invisible and Mitsuba warns about this when loading the scene.
To obtain identical results on all backends, give every animated instance
keyframes that span the whole shutter interval, for example by repeating its
first and last pose on the uniform keyframe grid.

Animation and ``mitsuba.traverse()``
------------------------------------

Through :py:func:`mitsuba.traverse()`, an animated transformation exposes its
keyframes as four tensors.

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

The tensors can be edited independently.

.. code-block:: python

    params = mi.traverse(scene)
    t = mi.TensorXf(params['instance.to_world.translation'])
    t[1, 0] = 2.5                                   # second keyframe, x axis
    params['instance.to_world.translation'] = t
    params.update()

The four tensors must agree on the number of keyframes. Changing it therefore
requires writing all four, followed by another call to
:py:func:`mitsuba.traverse()` to obtain the resized tensors. An animation must
keep at least two keyframes.

Objects without animation store their transformation as a plain 4x4 matrix,
which :py:func:`mitsuba.traverse()` exposes directly under the parameter name.

.. code-block:: python

    params['sensor.to_world'] = mi.ScalarTransform4f.translate([0, 0, 1])
    params.update()

Static objects cannot be turned into animated ones through
:py:func:`mitsuba.traverse()`. Specify an animation when loading the scene
instead.
