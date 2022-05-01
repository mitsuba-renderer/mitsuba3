.. _sec-integrators:

Integrators
===========

In Mitsuba 3, the different rendering techniques are collectively referred to as
*integrators*, since they perform integration over a high-dimensional space.
Each integrator represents a specific approach for solving the light transport
equation---usually favored in certain scenarios, but at the same time affected
by its own set of intrinsic limitations. Therefore, it is important to carefully
select an integrator based on user-specified accuracy requirements and
properties of the scene to be rendered.

In the XML description language, a single integrator is usually instantiated by
declaring it at the top level within the scene, e.g.

.. tabs::
    .. code-tab:: xml

        <scene version=3.0.0>
            <!-- Instantiate a unidirectional path tracer,
                which renders paths up to a depth of 5 -->
            <integrator type="path">
                <integer name="max_depth" value="5"/>
            </integrator>

            <!-- Some geometry to be rendered -->
            <shape type="sphere">
                <bsdf type="diffuse"/>
            </shape>
        </scene>

    .. code-tab:: python

        'type': 'scene',
        # Instantiate a unidirectional path tracer, which renders
        # paths up to a depth of 5
        'integrator_id': {
            'type': 'path',
            'max_depth': 5
        },

        # Some geometry to be rendered
        'shape_id': {
            'type': 'sphere',
            'bsdf': {
                'type': 'diffuse'
            }
        }


This section gives an overview of the available choices along with their parameters.

Almost all integrators use the concept of *path depth*. Here, a path refers to
a chain of scattering events that starts at the light source and ends at the
camera. It is often useful to limit the path depth when rendering scenes for
preview purposes, since this reduces the amount of computation that is necessary
per pixel. Furthermore, such renderings usually converge faster and therefore
need fewer samples per pixel. Then reference-quality is desired, one should always
leave the path depth unlimited.

The Cornell box renderings below demonstrate the visual effect of a maximum path
depth. As the paths are allowed to grow longer, the color saturation increases
due to multiple scattering interactions with the colored surfaces. At the same
time, the computation time increases.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/integrator_depth_1.jpg
   :caption: max. depth = 1
.. subfigure:: ../../resources/data/docs/images/render/integrator_depth_2.jpg
   :caption: max. depth = 2
.. subfigure:: ../../resources/data/docs/images/render/integrator_depth_3.jpg
   :caption: max. depth = 3
.. subfigure:: ../../resources/data/docs/images/render/integrator_depth_inf.jpg
   :caption: max. depth = :math:`\infty`
.. subfigend::
   :width: 0.23
   :label: fig-integrators-depth

Mitsuba counts depths starting at 1, which corresponds to visible light sources
(i.e. a path that starts at the light source and ends at the camera without any
scattering interaction in between.) A depth-2 path (also known as "direct
illumination") includes a single scattering event like shown here:

.. image:: ../../resources/data/docs/images/integrator/path_explanation.jpg
    :width: 80%
    :align: center