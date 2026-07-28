.. _sec-integrators:

Integrators
===========

*Integrators* implement the actual sound propagation simulation, tracing
paths between sound sources and a :ref:`microphone <sensor-microphone>` and
accumulating their contributions into a :ref:`tape <film-tape>` film. misuka
provides a forward, primal-only path tracer, :ref:`acoustic_path
<integrator-acoustic_path>`, implemented in C++, as well as differentiable
Python integrators built on Time-Resolved Path Replay Backpropagation
:cite:`acoustic_prb` for gradient-based acoustic optimization. See the
acoustic integrators listed below.

In the XML description language, a single integrator is usually instantiated
by declaring it at the top level within the scene, e.g.

.. tabs::
    .. code-tab:: xml

        <scene version="3.0.0">
            <!-- Instantiate the acoustic path tracer, terminating
                paths after 0.5 seconds of propagation time -->
            <integrator type="acoustic_path">
                <float name="max_time" value="0.5"/>
            </integrator>

            <!-- Some geometry to be rendered -->
            <shape type="rectangle">
                <bsdf type="acousticbsdf"/>
            </shape>
        </scene>

    .. code-tab:: python

        'type': 'scene',
        # Instantiate the acoustic path tracer, terminating paths
        # after 0.5 seconds of propagation time
        'integrator_id': {
            'type': 'acoustic_path',
            'max_time': 0.5
        },

        # Some geometry to be rendered
        'shape_id': {
            'type': 'rectangle',
            'bsdf': {
                'type': 'acousticbsdf'
            }
        }


This section gives an overview of the available choices along with their parameters.

Acoustic integrators use the concept of *path depth*, analogous to light
transport: a path is a chain of scattering events that starts at a sound
source and ends at the microphone. Depth 1 corresponds to directly audible
sources (no reflections), depth 2 adds a single reflection, and so on. Unlike
an image, the recorded quantity is not a per-pixel radiance value but energy
distributed across propagation time and frequency, so limiting the path
depth trades off simulated reflection order against computation time rather
than image noise alone.
