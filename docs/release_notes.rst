Release notes
=============

Being an experimental research framework, Mitsuba 3 does not strictly follow the
`Semantic Versioning <https://semver.org/>`_ convention. That said, we will
strive to document breaking API changes in the release notes below.


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
