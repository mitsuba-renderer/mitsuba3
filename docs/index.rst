.. only:: not latex

    .. image:: images/misuka_logo.png
        :width: 40%
        :align: center

Getting started
===============

misuka is a research-oriented, differentiable **room-acoustic renderer** for
forward and inverse sound-transport simulation. It is a fully compatible
extension to `Mitsuba 3 <https://mitsuba.readthedocs.io/en/latest/>`_: it reuses
Mitsuba's scene format, geometry, samplers, and the `Dr.Jit
<https://drjit.readthedocs.io/en/latest/>`_ JIT compiler / autodiff engine, and
adds acoustic plugins (an absorbing/scattering material, an energy path tracer,
a microphone sensor, and an Energy-Time-Curve film). It implements `Time-Resolved
Path Replay Backpropagation <https://dl.acm.org/doi/pdf/10.1145/3730900>`_ for
efficient gradient estimation with respect to material properties, source/receiver
positions, and scene geometry.

Because misuka is an extension, the light-transport engine, scene description
language, and Python API are documented upstream. This site documents only the
**acoustic surface**; follow the links above for everything misuka inherits from
Mitsuba 3 and Dr.Jit.

Installation
------------

misuka is built from source, analogously to Mitsuba 3. Follow the
:ref:`developer guide <sec-compiling>` for the full recipe, and make sure the
:monosp:`mitsuba.conf` ``"enabled"`` list contains at least one ``*_acoustic``
variant (e.g. ``llvm_ad_acoustic``) — acoustic scenes cannot be rendered with an
optical variant. See :ref:`sec-variants` for details.

Requirements
^^^^^^^^^^^^

- ``Python >= 3.9``
- (optional) For computation on the GPU: ``Nvidia driver >= 535``
- (optional) For vectorized / parallel computation on the CPU: ``LLVM >= 11.1``
- (optional) For computation on Apple Silicon GPUs: macOS with a Metal-capable GPU

Hello World!
------------

The example below builds a simple shoebox room with a spherical sound source,
places a microphone, and renders an **Energy Time Curve (ETC)** — the acoustic
analogue of an image: energy against propagation time, one row per frequency band.

.. code-block:: python

    import misuka as mi

    mi.set_variant("llvm_ad_acoustic")

    from misuka import ScalarTransform4f as tf

    # A 6 x 8 x 4 m shoebox room with a spherical sound source.
    room_dim     = [6.0, 8.0, 4.0]
    source_pos   = [3.0, 6.0, 1.2]
    receiver_pos = [2.0, 1.0, 1.2]

    scene_dict = {
        "type": "scene",
        # Omnidirectional sound source.
        "emitter": {
            "type": "sphere",
            "radius": 0.1,
            "center": source_pos,
            "emitter": {"type": "area", "radiance": {"type": "uniform", "value": 50}},
        },
        # One absorbing/scattering acoustic material, shared by all six walls.
        "wall": {
            "type": "acousticbsdf",
            "absorption": {"type": "spectrum", "value": [(100, 0.1), (500, 0.5), (20000, 0.4)]},
            "scattering": {"type": "spectrum", "value": [(100, 0.2), (500, 0.5), (20000, 0.8)]},
        },
    }

    # A unit cube of six inward-facing rectangles, placed via a shapegroup and
    # instanced at the room's dimensions.
    walls = {
        "top":    (tf().translate([0, 0, 1]).scale(0.5).translate([1, 1, 0]), True),
        "bottom": (tf().scale(0.5).translate([1, 1, 0]), False),
        "left":   (tf().rotate(axis=[0, -1, 0], angle=90).scale(0.5).translate([1, 1, 0]), True),
        "right":  (tf().translate([1, 0, 1]).rotate(axis=[0, 1, 0], angle=90).scale(0.5).translate([1, 1, 0]), True),
        "front":  (tf().rotate(axis=[1, 0, 0], angle=90).scale(0.5).translate([1, 1, 0]), True),
        "back":   (tf().translate([0, 1, 0]).rotate(axis=[1, 0, 0], angle=90).scale(0.5).translate([1, 1, 0]), False),
    }
    scene_dict["cube"] = {"type": "shapegroup"}
    for surface, (to_world, flip) in walls.items():
        scene_dict["cube"][surface] = {
            "type": "rectangle",
            "bsdf": {"type": "ref", "id": "wall"},
            "to_world": to_world,
            "flip_normals": flip,
        }
    scene_dict["shoebox"] = {
        "type": "instance",
        "geometry": {"type": "ref", "id": "cube"},
        "to_world": tf().scale(room_dim),
    }

    scene = mi.load_dict(scene_dict)

    # A microphone that records an ETC into a `tape` film.
    max_time      = 0.1    # seconds
    sampling_rate = 10000  # time bins per second
    microphone = mi.load_dict({
        "type": "microphone",
        "origin": receiver_pos,
        "direction": source_pos,
        "film": {
            "type": "tape",
            "frequencies": "100, 500, 20000",
            "time_bins": int(max_time * sampling_rate),
        },
    })

    integrator = mi.load_dict({
        "type": "acoustic_path",
        "max_depth": -1,          # -1 = unlimited reflections
        "max_time": max_time,
        "speed_of_sound": 343,
    })

    # Render the ETC (time_bins x frequencies x 1). Increase spp to reduce noise.
    etc = mi.render(scene, sensor=microphone, integrator=integrator, spp=2**16)

For a fully worked version — including a visual preview of the room and plotting
the ETC — see the :doc:`rendering tutorials <src/rendering_tutorials>`.

Citation
--------

When using misuka in academic projects, please cite:

.. code-block:: bibtex

    @article{misuka,
        title   = {{misuka}: An Open-Source Differentiable Room Acoustic Renderer},
        author  = {J\"uterbock, Tobias and Finnendahl, Ugo and Worchel, Markus and
                   Wujecki, Daniel and Alexa, Marc and Weinzierl, Stefan},
        journal = {Proceedings of Meetings on Acoustics},
        volume  = {58},
        number  = {1},
        pages   = {022004:1--022004:13},
        year    = {2026},
        doi     = {10.1121/2.0002193},
    }

When using Time-Resolved Path Replay Backpropagation, please also cite:

.. code-block:: bibtex

    @article{acoustic_prb,
        title   = {Differentiable Geometric Acoustic Path Tracing Using
                   Time-Resolved Path Replay Backpropagation},
        author  = {Finnendahl, Ugo and Worchel, Markus and J\"uterbock, Tobias and
                   Wujecki, Daniel and Brinkmann, Fabian and Weinzierl, Stefan and
                   Alexa, Marc},
        journal = {ACM Transactions on Graphics},
        volume  = {44},
        number  = {4},
        pages   = {82:1--82:17},
        year    = {2025},
        doi     = {10.1145/3730900},
    }

misuka is built on `Mitsuba 3 <https://mitsuba.readthedocs.io/en/latest/>`_; when
appropriate, please also cite the underlying renderer following its
`citation guidelines <https://mitsuba.readthedocs.io/en/latest/#citation>`_.

.. .............................................................................

.. toctree::
   :hidden:

   self

.. toctree::
    :maxdepth: 1
    :caption: Tutorials
    :hidden:

    src/rendering_tutorials
    src/inverse_rendering_tutorials

.. toctree::
    :maxdepth: 1
    :caption: Guides
    :hidden:

    src/key_topics
    src/developer_guide

.. toctree::
    :maxdepth: 1
    :caption: References
    :hidden:

    src/plugin_reference
    src/api_reference

.. toctree::
    :maxdepth: 1
    :caption: Miscellaneous
    :hidden:

    src/optix_setup
    porting_3_6
    release_notes
    zz_bibliography
