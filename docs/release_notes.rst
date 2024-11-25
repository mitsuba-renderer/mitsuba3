Release notes
=============

Being an experimental research framework, Mitsuba 3 does not strictly follow the
`Semantic Versioning <https://semver.org/>`_ convention. That said, we will
strive to document breaking API changes in the release notes below.

Mitsuba 3.6.0
-------------

*November 25, 2024*

This release comes with a major overhaul of some of the internal components of
Mitsuba 3. Namely, the Python bindings are now created using
`nanobind <https://github.com/wjakob/nanobind>`_ and the just-in-time compiler
Dr.Jit was updated to `version 1.0.0 <https://drjit.readthedocs.io/en/stable/changelog.html#drjit-1-0-0-november-21-2024>`_.

These upgrades lead to the following:

- Performance boost: 1.2x to 2x speedups depending on the JIT backend and scene size
- Improved stubs: auto-completion and type-checking has been greatly improved
- More variants on PyPI: thirteen variants are available in the pre-built wheels

Some breaking changes were made in this process. Please refer to the
`porting guide <https://mitsuba.readthedocs.io/en/v3.6.0/porting_3_6.html>`_ to
get a comprehensive overview of these changes.

This release also includes a series of bug fixes, quality of life improvements
and new features. Here's a non-exhaustive list:

- Support for Embree's robust intersection flag
  `[96e0af2] <https://github.com/mitsuba-renderer/mitsuba3/commit/96e0af2de054c6d21e0ac2f68dd41bcd2cb469e5>`_
- Callback system for variant changes
  `#1367 <https://github.com/mitsuba-renderer/mitsuba3/pull/1367>`_
- ``MeshPtr`` for vectorized ``Mesh`` method calls
  `#1319 <https://github.com/mitsuba-renderer/mitsuba3/pull/1319>`_
- Aliases for the ``ArrayX`` types of Dr.Jit
  `[2e86e5e] <https://github.com/mitsuba-renderer/mitsuba3/commit/2e86e5e013b397391d6a59b09ee8238df03589b4>`_
- Fix attribute evaluation for ``twosided`` BSDFs
  `[5508ee6] <https://github.com/mitsuba-renderer/mitsuba3/commit/5508ee6a392e2b32c1a4360742cbe9c966586458>`_ .. `[7528d9f] <https://github.com/mitsuba-renderer/mitsuba3/commit/7528d9fb2d9012e97ebade224685cc8620a647cd>`_
- A new `guide for using Mitsuba 3 in WSL 2 <https://mitsuba.readthedocs.io/en/v3.6.0/src/optix_setup.html>`_
- ``batch`` sensors expose their inner ``Sensor`` objects when traversed with ``mi.traverse()``
  `#1297 <https://github.com/mitsuba-renderer/mitsuba3/pull/1297>`_
- Python stubs improvements
  `#1260 <https://github.com/mitsuba-renderer/mitsuba3/pull/1260>`_ `#1238 <https://github.com/mitsuba-renderer/mitsuba3/pull/1238>`_
- Updated wheel build process with new variants
  `#1355 <https://github.com/mitsuba-renderer/mitsuba3/pull/1355>`_

Mitsuba 3.5.2
-------------

*June 5, 2024*

Most likely the last release which uses `pybind11 <https://pybind11.readthedocs.io>`_.

- OptiX scene clean-ups could segfault
  `[03f5e13] <https://github.com/mitsuba-renderer/mitsuba3/commit/03f5e1362d0cf1cc8c4edbd6e0e7bfd5ee8705a0>`_

Mitsuba 3.5.1
-------------

*June 5, 2024*

- Upgrade Dr.Jit to `[v0.4.6] <https://github.com/mitsuba-renderer/drjit/releases/tag/v0.4.6>`_
- More robust scene clean-up when using Embree
  `[7bb672c] <https://github.com/mitsuba-renderer/mitsuba3/commit/7bb672c32d64ad9a4996d3c7700d445d2c5750bc>`_
- Support for AOV fields in Python AD integrators
  `[f3b427e] <https://github.com/mitsuba-renderer/mitsuba3/commit/f3b427e02ca9dd1fb2e0fb9b993c67a2779d2052>`_
- Fix potential segfault during OptiX scene clean-up
  `[0bcfc72] <https://github.com/mitsuba-renderer/mitsuba3/commit/0bcfc72b846cd5483109b1323301755e23926e76>`_
- Improve and fix Mesh PMF computations
  `[ced7b22] <https://github.com/mitsuba-renderer/mitsuba3/commit/ced7b2204d7d8beefa149a6c5b43e2ff5796a725>`_ .. `[7d2951a] <https://github.com/mitsuba-renderer/mitsuba3/commit/7d2951a5f3f55a0bda4f40e3c4299441f05e70d5>`_
- ``Shape.parameters_grad_enabled`` now only applies to parameters that introduce visibility discontinuities
  `[3013adb] <https://github.com/mitsuba-renderer/mitsuba3/commit/3013adb4f12a491f8dd37c32bcedf55c7998f9e8>`_
- The ``measuredpolarized`` plugin is now supported in vectorized variants
  `[68b3a5f] <https://github.com/mitsuba-renderer/mitsuba3/commit/68b3a5f20ea00eb83631a7c48585162c6d901a7d>`_
- Fix an issue where the ``constant`` plugin would not reuse kernels
  `[deebe4c] <https://github.com/mitsuba-renderer/mitsuba3/commit/deebe4c64586c129bb0b0280bbaf376e2315991c>`_
- Minor changes to support Nvidia v555 drivers
  `[19bf5a4] <https://github.com/mitsuba-renderer/mitsuba3/commit/19bf5a4d82e760614f766067baf0c8add3bc8a41>`_
- Many numerical and performance improvements to the ``sdfgrid`` shape
  `[455de40] <https://github.com/mitsuba-renderer/mitsuba3/commit/455de408abf7660e1667a1ed810fc6fd903b9db3>`_ .. `[9e156bd] <https://github.com/mitsuba-renderer/mitsuba3/commit/9e156bdf3a33042b16593e3f5de40acb7d22da64>`_

Mitsuba 3.5.0
-------------

- New projective sampling based integrators, see PR `#997 <https://github.com/mitsuba-renderer/mitsuba3/pull/997>`_ for more details.
  Here's a brief overview of some of the major or breaking changes:

  - New ``prb_projective`` and ``direct_projective`` integrators
  - New curve/shadow optimization tutorial
  - Removed reparameterizations
  - Can no longer differentiate ``instance``, ``sdfgrid`` and ``Sensor``'s positions

Mitsuba 3.4.1
-------------

*December 11, 2023*

- Upgrade Dr.Jit to `[v0.4.4] <https://github.com/mitsuba-renderer/drjit/releases/tag/v0.4.4>`_

  - Solved threading/concurrency issues which could break loading of large scenes or long running optimizations
- Scene's bounding box now gets updated on parameter changes
  `[97d4b6a] <https://github.com/mitsuba-renderer/mitsuba3/commit/97d4b6ad4c1ba3471642c177cee01d3adf0bf22e>`_
- Python bindings for ``mi.lookup_ior``
  `[d598d79] <https://github.com/mitsuba-renderer/mitsuba3/commit/d598d79a7d21c76ac9b422b3488137b1d28a33f9>`_
- Fixes to ``mask`` BSDF when differentiated
  `[ee87f1c] <https://github.com/mitsuba-renderer/mitsuba3/commit/ee87f1c01aa1b731bc58057ed9e6944046460a69>`_
- Ray sampling is fixed when ``sample_border`` is used
  `[c10b87b] <https://github.com/mitsuba-renderer/mitsuba3/commit/c10b87b072634db15d55a7dbc55cc3cf8f7c844c>`_
- Rename OpenEXR shared library
  `[9cc3bf4] <https://github.com/mitsuba-renderer/mitsuba3/commit/9cc3bf495da10dcd28e80cc14a145fb178a5ef4c>`_
- Handle phase function differentiation in ``prbvolpath``
  `[5f9eebd] <https://github.com/mitsuba-renderer/mitsuba3/commit/5f9eebd41a3a939096d4509b1d2504586a3bf7c6>`_
- Fixes to linear ``retarder``
  `[8033a80] <https://github.com/mitsuba-renderer/mitsuba3/commit/8033a807091f8315c5cef25f4f1a36a3766fb223>`_
- Avoid copies to host when building 1D distributions
  `[825f44f] <https://github.com/mitsuba-renderer/mitsuba3/commit/825f44f081fb43b23589b2bf0b9b7071af858f2a>`_ .. `[8f71fe9] <https://github.com/mitsuba-renderer/mitsuba3/commit/8f71fe995f40923449478ee05500918710ef27f6>`_
- Fixes to linear ``retarder``
  `[8033a80] <https://github.com/mitsuba-renderer/mitsuba3/commit/8033a807091f8315c5cef25f4f1a36a3766fb223>`_
- Sensor's prinicpal point is now exposed throught ``m̀i.traverse()``
  `[f59faa5] <https://github.com/mitsuba-renderer/mitsuba3/commit/f59faa51929b506608a66522dc841f5317a8d43c>`_
- Minor fixes to ``ptracer`` which could result in illegal memory accesses
  `[3d902a4] <https://github.com/mitsuba-renderer/mitsuba3/commit/3d902a4dbf176c8c8d08e5493f23623659295197>`_
- Other various minor bug fixes

Mitsuba 3.4.0
-------------

*August 29, 2023*

- Upgrade Dr.Jit to v0.4.3
- Add ``mi.variant_context()``: a Python context manager for setting variants
  `[96b219d] <https://github.com/mitsuba-renderer/mitsuba3/commit/96b219d75a69f997623c76611fb6d0b90e2c5c3e>`_
- Emitters may now define a sampling weight
  `[9a5f4c0] <https://github.com/mitsuba-renderer/mitsuba3/commit/9a5f4c0d5f52de7553beb64e82ad139fce879649>`_
- Fix ``bsplinecurve`` and ``linearcurve`` shading frames
  `[3875f9a] <https://github.com/mitsuba-renderer/mitsuba3/commit/3875f9adda5eddf9b233901d52dac6b9238a5c83>`_
- Add implementation of ``LargeSteps`` method for mesh optimizations (includes a new tutorial)
  `[48e6428] <https://github.com/mitsuba-renderer/mitsuba3/commit/48e64283814297bd89306cd4beba718221eacaf3>`_ .. `[130ed55] <https://github.com/mitsuba-renderer/mitsuba3/commit/130ed5522887f5405736f28f2081d04b1c1852c3>`_
- Support for spectral phase functions
  `[c7d5c75] <https://github.com/mitsuba-renderer/mitsuba3/commit/c7d5c75707046ee9ade56604f8a0b1c5b724b729>`_
- Additional resource folders can now be specified in ``mi.load_dict()``
  `[66ea528] <https://github.com/mitsuba-renderer/mitsuba3/commit/66ea5285b1bc9a251eafa0b8449bb0d641e3fa1c>`_
- BSDFs can expose their attributes through a generic ``eval_attribute`` method
  `[cfc425a] <https://github.com/mitsuba-renderer/mitsuba3/commit/cfc425a2b5753127aeb818dab0ebab828dc8f060>`_ .. `[c345d70] <https://github.com/mitsuba-renderer/mitsuba3/commit/c345d700bb273832d4ce2fd753929374fd076d64>`_
- New ``sdfgrid`` shape: a signed distance field on a regular grid
  `[272a5bf] <https://github.com/mitsuba-renderer/mitsuba3/commit/272a5bf10e3590d9ae35144d0819396181bdaef2>`_ .. `[618da87] <https://github.com/mitsuba-renderer/mitsuba3/commit/618da871d19cb36a3879230d3799f3341a657c08>`_
- Support for adjoint differentiation methods through the ``aov`` integrator
  `[c9df8de] <https://github.com/mitsuba-renderer/mitsuba3/commit/c9df8de011e2d835402a4fcc8fe6ef832b4ce40a>`_ .. `[bff5cf2] <https://github.com/mitsuba-renderer/mitsuba3/commit/bff5cf240ad1676eea398c99e32f4d49f0f44925>`_
- Various fixes to ``prbvolpath``
  `[6d78f2e] <https://github.com/mitsuba-renderer/mitsuba3/commit/6d78f2ed30e746a718567a85a740db365e44407b>`_, `[a946691] <https://github.com/mitsuba-renderer/mitsuba3/commit/a946691a0d5272a80ea45f7b5f22f31d697cf290>`_ , `[91b0b7e] <https://github.com/mitsuba-renderer/mitsuba3/commit/91b0b7e7c2732a131fac9149bf1db81429e946b0>`_
- Curve shapes (``bsplinecurve`` and ``linearcurve``) always have back-face culling enabled
  `[188b254] <https://github.com/mitsuba-renderer/mitsuba3/commit/188b25425306fd373e69f07f183f0348d8952496>`_ .. `[01ea7ba] <https://github.com/mitsuba-renderer/mitsuba3/commit/01ea7baedf433dc8c337b29b2741992a3a857ee8>`_
- ``Properties`` can now accept tensor objects, currenlty used in ``bitmap``, ``sdfgrid`` and ``gridvolume``
  `[d030a3a] <https://github.com/mitsuba-renderer/mitsuba3/commit/d030a3a13b0d222e3c6647ebc6ceb0919a2f296b>`_
- New ``hair`` BSDF shading model
  `[91fc8e6] <https://github.com/mitsuba-renderer/mitsuba3/commit/91fc8e6356c95b665853a1d294da5187ea16bd39>`_ .. `[0b9b04a] <https://github.com/mitsuba-renderer/mitsuba3/commit/0b9b04aa2c6ca7d0e1b5f8503317b46f2bb972f8>`_
- Improvements to the ``batch`` sensor (performance, documentation, bug fixes)
  `[527ed22] <https://github.com/mitsuba-renderer/mitsuba3/commit/527ed22c801666efd746aebcfed8c299748777f0>`_ .. `[65e0444] <https://github.com/mitsuba-renderer/mitsuba3/commit/65e0444c59c4d50dd8b8547b05b8a3707353df4a>`_
- Many missing Python bindings were added
- Other various minor bug fixes


Mitsuba 3.3.0
-------------

*April 25, 2023*

- Upgrade Dr.Jit to v0.4.2
- Emitters' members are opaque (fixes long JIT compilation times)
  `[df940c1] <https://github.com/mitsuba-renderer/mitsuba3/commit/df940c128116ffa9518058573aa93dedaca6cc33>`_
- Sensors members are opaque (fixes long JIT compilation times)
  `[c864e08] <https://github.com/mitsuba-renderer/mitsuba3/commit/c864e08f5bfa56388444e8ce0bb2751e35ee33d9>`_
- Fix ``cylinder``'s normals
  `[d9ea8e8] <https://github.com/mitsuba-renderer/mitsuba3/commit/d9ea8e847a0ceea88ad3e28e1e41e36ce800d5b6>`_
- Fix next event estimation (NEE) in volume integrators
- ``mi.xml.dict_to_xml`` now supports volumes
  `[15d63df] <https://github.com/mitsuba-renderer/mitsuba3/commit/15d63df4d3eab283de0c7ed511c312bba504ec46>`_
- Allow extending ``AdjointIntegrator`` in Python
  `[15d63df] <https://github.com/mitsuba-renderer/mitsuba3/commit/c4a8b31ee764a0e6d56d9075708c3c76062854be>`_
- ``mi.load_dict()`` is parallel (by default)
  `[bb672ed] <https://github.com/mitsuba-renderer/mitsuba3/commit/bb672ed7cee006ff37819030b9f269f0da263568>`_
- Upsampling routines now support ``box`` filters
  `[64e2ab1] <https://github.com/mitsuba-renderer/mitsuba3/commit/64e2ab1718e6f6959233b1f0ae18337e7a642684>`_
- The ``Mesh.write_ply()`` function writes ``s, t`` rather than ``u, v`` fields
  `[fe4e448] <https://github.com/mitsuba-renderer/mitsuba3/commit/fe4e4484becc3a7997413f648b4efeb75667554b>`_
- All shapes can hold ``Texture`` attributes which can be evaluated
  `[f6ec944] <https://github.com/mitsuba-renderer/mitsuba3/commit/f6ec944c4beb8b0136dff6136e52bc0851acd931>`_
- Radiative backpropagation style integrators use less memory
  `[c1a9b8f] <https://github.com/mitsuba-renderer/mitsuba3/commit/c1a9b8fa52cea4fff4e25a8169ad8be811b1574e>`_
- New ``bsplinecurve`` and ``linearcurve`` shapes
  `[e4c847f] <https://github.com/mitsuba-renderer/mitsuba3/commit/e4c847fedf9005f80bda58a9f6bcfd05581b884c>`_ .. `[79eb026] <https://github.com/mitsuba-renderer/mitsuba3/commit/79eb026d6d594076994dba2c44de81c63b7806f4>`_


Mitsuba 3.2.1
-------------

*February 22, 2023*

- Upgrade Dr.Jit to v0.4.1
- ``Film`` plugins can now have error-compensated accumulation in JIT modes
  `[afeefed] <https://github.com/mitsuba-renderer/mitsuba3/commit/afeefedc8db0d7381e023f80c00f527ce28725b7>`_
- Fix and add missing Python bindings for ``Endpoint``/``Emitter``/``Sensor``
  `[8f03c7d] <https://github.com/mitsuba-renderer/mitsuba3/commit/8f03c7db7b697a2bac17fe960a8d4a6863bece4d>`_
- Numerically robust sphere-ray intersections
  `[7d46e10] <https://github.com/mitsuba-renderer/mitsuba3/commit/7d46e10154b19945b2e4ee97ba7876ac917692c8>`_ .. `[0b483bf] <https://github.com/mitsuba-renderer/mitsuba3/commit/0b483bff5fdcc6d9663d73626bb1dd46674311a6>`_
- Fix parallel scene loading with Python plugins
  `[93bb99b] <https://github.com/mitsuba-renderer/mitsuba3/commit/93bb99b1ed20a3263b2fd82f1d5ab3a333afc002>`_
- Various minor bug fixes


Mitsuba 3.2.0
-------------

*January 6, 2023*

- Upgrade Dr.Jit to v0.4.0

  - Various bug fixes
  - Stability improvements (race conditions, invalid code generation)
  - Removed 4 billion variable limit
- Add missing Python bindings for ``Shape`` and ``ShapePtr``
  `[bdce950] <https://github.com/mitsuba-renderer/mitsuba3/commit/bdce9509f0504163678e81c6afdd7a8bc9c45340>`_
- Fix Python bindings for ``Scene``
  `[4cd5585] <https://github.com/mitsuba-renderer/mitsuba3/commit/4cd558587d711fb35444d5e21c2ab32f74776e65>`_
- Fix bug which would break the AD graph in ``spectral`` variants
  `[f3ac81b] <https://github.com/mitsuba-renderer/mitsuba3/commit/f3ac81bc5c6ce65d5843dde3a1d5f230353453e3>`_
- Parallel scene loading in JIT variants
  `[48c14a7] <https://github.com/mitsuba-renderer/mitsuba3/commit/48c14a709dcc6da9e44583e85eda5735f1888093>`_ .. `[187da96] <https://github.com/mitsuba-renderer/mitsuba3/commit/187da96afd45e14c17d82909fbbf50cb713c8196>`_
- Fix sampling of ``hg`` ``PhaseFunction``
  `[10d3514] <https://github.com/mitsuba-renderer/mitsuba3/commit/10d3514a0295cad4ac6d440c7ff326561c6da6a2>`_
- Fix `envmap` updating in JIT variants
  `[7bf132f] <https://github.com/mitsuba-renderer/mitsuba3/commit/7bf132f6ae3ec46085a7b24bdb1fcce84983425e>`_
- Expose ``PhaseFunction`` of ``Medium`` objects through ``mi.traverse()``
  `[cca5791] <https://github.com/mitsuba-renderer/mitsuba3/commit/cca5791aac22cdf7b3b12cd7a69f7a6800fc715b>`_


Mitsuba 3.1.1
-------------

*November 25, 2022*

- Fixed maximum limits for OptiX kernel launches
  `[a8e6989] <https://github.com/mitsuba-renderer/mitsuba3/commit/a8e69898eacde51954bbc91b34924448b4f8c954>`_


Mitsuba 3.1.0
-------------

New features
^^^^^^^^^^^^

- Enable ray tracing against two different scenes in a single kernel
  `[df79cb3] <https://github.com/mitsuba-renderer/mitsuba3/commit/df79cb3e2837e9296bc3e4ff2afb57416af102f4>`_
- Make ``ShapeGroup`` traversable and updatable
  `[e0871aa] <https://github.com/mitsuba-renderer/mitsuba3/commit/e0871aa8ab58b64216247ed189a77e5e009297d2>`_
- Enable differentiation of ``to_world`` in ``instance``
  `[54d2d3a] <https://github.com/mitsuba-renderer/mitsuba3/commit/54d2d3ab785f8fee4ade8581649ed82d653847cb>`_
- Enable differentiation of ``to_world`` in ``sphere``, ``rectangle``, ``disk`` and ``cylinder``
  `[b5d8c5d] <https://github.com/mitsuba-renderer/mitsuba3/commit/f5dbedec9bab3c45d31255532da07b0c01f5374c>`_ .. `[b5d8c] <https://github.com/mitsuba-renderer/mitsuba3/commit/b5d8c5dc8f33b65613ca27819771950ab9909824>`_
- Enable differentiation of ``to_world`` in ``perspective`` and ``thinlens``
  `[ea513f7] <https://github.com/mitsuba-renderer/mitsuba3/commit/ef9f559e0989fd01b43acce90892ba9e0dea255b>`_ .. `[ea513f] <https://github.com/mitsuba-renderer/mitsuba3/commit/ea513f73b65b8776afb75fdc8d40db4b1140345e>`_
- Add ``BSDF::eval_diffuse_reflectance()`` to most BSDF plugins
  `[59af884] <https://github.com/mitsuba-renderer/mitsuba3/commit/59af884e6fae3a50074921136329d80462b32413>`_
- Add ``mi.OptixDenoiser`` class for simple denoising in Python
  `[5529318] <https://github.com/mitsuba-renderer/mitsuba3/commit/1323497f4e675a8004529eef8404cdc541ade7cf>`_ .. `[55293] <https://github.com/mitsuba-renderer/mitsuba3/commit/552931890df648a5416b0d54d15488f6e766797a>`_
- ``envmap`` plugin can be constructed from ``mi.Bitmap`` object
  `[9389c8d] <https://github.com/mitsuba-renderer/mitsuba3/commit/9389c8d1d16aa7a46d0a54f64eec1d10a1ae1ffd>`_

Other improvements
^^^^^^^^^^^^^^^^^^

- Major performance improvements in ``cuda_*`` variants with new version of Dr.Jit
- Deprecated ``samples_per_pass`` parameter
  `[8ba8528] <https://github.com/mitsuba-renderer/mitsuba3/commit/8ba8528abbad6add1f6a97b30b79ce53c4ff37bf>`_
- Fix rendering progress bar on Windows
  `[d8db806] <https://github.com/mitsuba-renderer/mitsuba3/commit/d8db806ae286358b31ade67dc714de666b25443f>`_
- ``obj`` file parsing performance improvements on Windows
  `[28660f3] <https://github.com/mitsuba-renderer/mitsuba3/commit/28660f3ab9db8f1da58cc38d2fd309cff4871e7e>`_
- Fix ``mi.luminance()`` for monochromatic modes
  `[61b9516] <https://github.com/mitsuba-renderer/mitsuba3/commit/61b9516a742f29e3a5d20e41c50be90d04509539>`_
- Add bindings for ``PluginManager.create_object()``
  `[4ebf700] <https://github.com/mitsuba-renderer/mitsuba3/commit/4ebf700c61e92bb494d605527961882da47a71c0>`_
- Fix ``SceneParameters.update()`` unnecessary hash computation
  `[f57e741] <https://github.com/mitsuba-renderer/mitsuba3/commit/f57e7416ac263445e1b74eeaf661361f4ba94855>`_
- Fix numerical instabilities with ``box`` filter splatting
  `[2d89762] <https://github.com/mitsuba-renderer/mitsuba3/commit/2d8976266588e9b782f63f689c68648424b4898d>`_
- Improve ``math::bisect`` algorithm
  `[7ca09a3] <https://github.com/mitsuba-renderer/mitsuba3/commit/7ca09a3ad95cec306c538493fa8450a096560891>`_
- Fix syntax highlighting in documentation and tutorials
  `[5aa2716] <https://github.com/mitsuba-renderer/mitsuba3/commit/5aa271684424eca5a46f93946536bc7d0c1bc099>`_
- Fix ``Optimizer.set_learning_rate`` for ``int`` values
  `[53143db] <https://github.com/mitsuba-renderer/mitsuba3/commit/53143db05739b964b7a489f58dbd1bd4da87533c>`_
- Various minor improvements to the Python typing stub generation
  `[b7ef349] <https://github.com/mitsuba-renderer/mitsuba3/commit/f883834a50e3dab694b4fe4ceafdfa1ae3712782>`_ .. `[ad72a53] <https://github.com/mitsuba-renderer/mitsuba3/commit/ad72a5361889bcef1f19b702a28956c1549d26e3>`_
- Minor improvements to the documentation
- Various other minor fixes


Mitsuba 3.0.2
-------------

*September 13, 2022*

- Change behavior of ``<spectrum ..>`` and ``<rgb ..>`` tag at scene loading for better consistency between ``*_rgb`` and ``*_spectral`` variants
  `[f883834] <https://github.com/mitsuba-renderer/mitsuba3/commit/f883834a50e3dab694b4fe4ceafdfa1ae3712782>`_
- Polarization fixes
  `[2709889] <https://github.com/mitsuba-renderer/mitsuba3/commit/2709889b9b6970018d58cb0a974f99a885b31dbe>`_, `[06c2960] <https://github.com/mitsuba-renderer/mitsuba3/commit/06c2960b170a655cda831c57b674ec26da7a008f>`_
- Add PyTorch/Mitsuba interoperability tutorial using ``dr.wrap_ad()``
- Fix DLL loading crash when working with Mitsuba and PyTorch in Python
  `[59d7b35] <https://github.com/mitsuba-renderer/mitsuba3/commit/59d7b35c0a7968957e8469f43c308683b63df5c4>`_
- Fix crash when evaluating Mitsuba ray tracing kernel from another thread in ``cuda`` mode.
  `[cd0846f] <https://github.com/mitsuba-renderer/mitsuba3/commit/cd0846ffc570b13ece9fb6c1d3a05411d1ce4eef>`_
- Add stubs for ``Float``, ``ScalarFloat`` and other builtin types
  `[8249179] <https://github.com/mitsuba-renderer/mitsuba3/commit/824917976176cb0a5b2a2b1cf1247e36e6b866ce>`_
- Plugins ``regular`` and ``blackbody`` have renamed parameters: ``wavelength_min``, ``wavelength_max`` (previously ``lambda_min``, ``lambda_max``)
  `[9d3487c] <https://github.com/mitsuba-renderer/mitsuba3/commit/9d3487c4846c5e9cc2a247afd30c4bbf3cbaae46>`_
- Dr.Jit Python stubs are generated during local builds
  `[4302caa8] <https://github.com/mitsuba-renderer/mitsuba3/commit/4302caa8bfd200a0edd6455ba64f92eab2be5824>`_
- Minor improvements to the documentation
- Various other minor fixes


Mitsuba 3.0.1
-------------

*July 27, 2022*

- Various minor fixes in documentation
- Added experimental ``batch`` sensor plugin
  `[0986152] <https://github.com/mitsuba-renderer/mitsuba3/commit/09861525e6c2ab677172dffc6204768c3d424c3e>`_
- Fix LD sampler for JIT modes
  `[98a8ecb] <https://github.com/mitsuba-renderer/mitsuba3/commit/98a8ecb2390ebf35ef5f54f28cccaf9ab267ea48>`_
- Prevent rebuilding of kernels for each sensor in an optimization
  `[152352f] <https://github.com/mitsuba-renderer/mitsuba3/commit/152352f87b5baea985511b2a80d9f91c3c945a90>`_
- Fix direction convention in ``tabphase`` plugin
  `[49e40ba] <https://github.com/mitsuba-renderer/mitsuba3/commit/49e40bad03da536136d3c8563eca6582fcb0e895>`_
- Create TLS module lookup cache for new threads
  `[6f62749] <https://github.com/mitsuba-renderer/mitsuba3/commit/6f62749d97904471315d2143b96af5ad6548da06>`_

Mitsuba 3.0.0
-------------

*July 20, 2022*

- Initial release
