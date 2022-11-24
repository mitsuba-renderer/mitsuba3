Release notes
=============

Being an experimental research framework, Mitsuba 3 does not strictly follow the
`Semantic Versioning <https://semver.org/>`_ convention. That said, we will
strive to document breaking API changes in the release notes below.


Mitsuba 3.1.1
-------------

Other improvements
^^^^^^^^^^^^^^^^^^

- Fixed maximum limits for OptiX kernel launches `[a8e698] <https://github.com/mitsuba-renderer/mitsuba3/commit/a8e69898eacde51954bbc91b34924448b4f8c954>`_

Mitsuba 3.1.0
-------------

New features
^^^^^^^^^^^^

- Enable ray tracing against two different scenes in a single kernel `[df79cb] <https://github.com/mitsuba-renderer/mitsuba3/commit/df79cb3e2837e9296bc3e4ff2afb57416af102f4>`_
- Make ``ShapeGroup`` traversable and updatable `[e0871a] <https://github.com/mitsuba-renderer/mitsuba3/commit/e0871aa8ab58b64216247ed189a77e5e009297d2>`_
- Enable differentiation of ``to_world`` in ``instance`` `[54d2d3] <https://github.com/mitsuba-renderer/mitsuba3/commit/54d2d3ab785f8fee4ade8581649ed82d653847cb>`_
- Enable differentiation of ``to_world`` in ``sphere``, ``rectangle``, ``disk`` and ``cylinder`` `[f5dbed] <https://github.com/mitsuba-renderer/mitsuba3/commit/f5dbedec9bab3c45d31255532da07b0c01f5374c>`_ .. `[b5d8c] <https://github.com/mitsuba-renderer/mitsuba3/commit/b5d8c5dc8f33b65613ca27819771950ab9909824>`_
- Enable differentiation of ``to_world`` in ``perspective`` and ``thinlens`` `[ef9f55] <https://github.com/mitsuba-renderer/mitsuba3/commit/ef9f559e0989fd01b43acce90892ba9e0dea255b>`_ .. `[ea513f] <https://github.com/mitsuba-renderer/mitsuba3/commit/ea513f73b65b8776afb75fdc8d40db4b1140345e>`_
- Add ``BSDF::eval_diffuse_reflectance()`` to most BSDF plugins `[59af88] <https://github.com/mitsuba-renderer/mitsuba3/commit/59af884e6fae3a50074921136329d80462b32413>`_
- Add ``mi.OptixDenoiser`` class for simple denoising in Python `[13234] <https://github.com/mitsuba-renderer/mitsuba3/commit/1323497f4e675a8004529eef8404cdc541ade7cf>`_ .. `[55293] <https://github.com/mitsuba-renderer/mitsuba3/commit/552931890df648a5416b0d54d15488f6e766797a>`_
- ``envmap`` plugin can be constructed from ``mi.Bitmap`` object `[9389c8] <https://github.com/mitsuba-renderer/mitsuba3/commit/9389c8d1d16aa7a46d0a54f64eec1d10a1ae1ffd>`_

Other improvements
^^^^^^^^^^^^^^^^^^

- Major performance improvements in ``cuda_*`` variants with new version of Dr.Jit
- Deprecated ``samples_per_pass`` parameter `[8ba85] <https://github.com/mitsuba-renderer/mitsuba3/commit/8ba8528abbad6add1f6a97b30b79ce53c4ff37bf>`_
- Fix rendering progress bar on Windows `[d8db80] <https://github.com/mitsuba-renderer/mitsuba3/commit/d8db806ae286358b31ade67dc714de666b25443f>`_
- ``obj`` file parsing performance improvements on Windows `[28660f] <https://github.com/mitsuba-renderer/mitsuba3/commit/28660f3ab9db8f1da58cc38d2fd309cff4871e7e>`_
- Fix ``mi.luminance()`` for monochromatic modes `[61b95] <https://github.com/mitsuba-renderer/mitsuba3/commit/61b9516a742f29e3a5d20e41c50be90d04509539>`_
- Add bindings for ``PluginManager.create_object()`` `[4ebf70] <https://github.com/mitsuba-renderer/mitsuba3/commit/4ebf700c61e92bb494d605527961882da47a71c0>`_
- Fix ``SceneParameters.update()`` unnecessary hash computation `[f57e74] <https://github.com/mitsuba-renderer/mitsuba3/commit/f57e7416ac263445e1b74eeaf661361f4ba94855>`_
- Fix numerical instabilities with ``box`` filter splatting `[2d8976] <https://github.com/mitsuba-renderer/mitsuba3/commit/2d8976266588e9b782f63f689c68648424b4898d>`_
- Improve ``math::bisect`` algorithm `[7ca09a] <https://github.com/mitsuba-renderer/mitsuba3/commit/7ca09a3ad95cec306c538493fa8450a096560891>`_
- Fix syntax highlighting in documentation and tutorials `[5aa271] <https://github.com/mitsuba-renderer/mitsuba3/commit/5aa271684424eca5a46f93946536bc7d0c1bc099>`_
- Fix ``Optimizer.set_learning_rate`` for ``int`` values `[53143d] <https://github.com/mitsuba-renderer/mitsuba3/commit/53143db05739b964b7a489f58dbd1bd4da87533c>`_
- Various minor improvements to the Python typing stub generation `[b7ef349] <https://github.com/mitsuba-renderer/mitsuba3/commit/f883834a50e3dab694b4fe4ceafdfa1ae3712782>`_ .. `[ad72a53] <https://github.com/mitsuba-renderer/mitsuba3/commit/ad72a5361889bcef1f19b702a28956c1549d26e3>`_
- Minor improvements to the documentation
- Various other minor fixes

Mitsuba 3.0.2
-------------

*September 13, 2022*

- Change behavior of ``<spectrum ..>`` and ``<rgb ..>`` tag at scene loading for better consistency between ``*_rgb`` and ``*_spectral`` variants `[f883834] <https://github.com/mitsuba-renderer/mitsuba3/commit/f883834a50e3dab694b4fe4ceafdfa1ae3712782>`_
- Polarization fixes `[2709889] <https://github.com/mitsuba-renderer/mitsuba3/commit/2709889b9b6970018d58cb0a974f99a885b31dbe>`_, `[06c2960] <https://github.com/mitsuba-renderer/mitsuba3/commit/06c2960b170a655cda831c57b674ec26da7a008f>`_
- Add PyTorch/Mitsuba interoperability tutorial using ``dr.wrap_ad()``
- Fix DLL loading crash when working with Mitsuba and PyTorch in Python `[59d7b35] <https://github.com/mitsuba-renderer/mitsuba3/commit/59d7b35c0a7968957e8469f43c308683b63df5c4>`_
- Fix crash when evaluating Mitsuba ray tracing kernel from another thread in ``cuda`` mode. `[cd0846f] <https://github.com/mitsuba-renderer/mitsuba3/commit/cd0846ffc570b13ece9fb6c1d3a05411d1ce4eef>`_
- Add stubs for ``Float``, ``ScalarFloat`` and other builtin types `[8249179] <https://github.com/mitsuba-renderer/mitsuba3/commit/824917976176cb0a5b2a2b1cf1247e36e6b866ce>`_
- Plugins ``regular`` and ``blackbody`` have renamed parameters: ``wavelength_min``, ``wavelength_max`` (previously ``lambda_min``, ``lambda_max``) `[9d3487c] <https://github.com/mitsuba-renderer/mitsuba3/commit/9d3487c4846c5e9cc2a247afd30c4bbf3cbaae46>`_
- Dr.Jit Python stubs are generated during local builds `[4302caa8] <https://github.com/mitsuba-renderer/mitsuba3/commit/4302caa8bfd200a0edd6455ba64f92eab2be5824>`_
- Minor improvements to the documentation
- Various other minor fixes


Mitsuba 3.0.1
-------------

*July 27, 2022*

- Various minor fixes in documentation
- Added experimental ``batch`` sensor plugin `[0986152] <https://github.com/mitsuba-renderer/mitsuba3/commit/09861525e6c2ab677172dffc6204768c3d424c3e>`_
- Fix LD sampler for JIT modes `[98a8ecb] <https://github.com/mitsuba-renderer/mitsuba3/commit/98a8ecb2390ebf35ef5f54f28cccaf9ab267ea48>`_
- Prevent rebuilding of kernels for each sensor in an optimization `[152352f] <https://github.com/mitsuba-renderer/mitsuba3/commit/152352f87b5baea985511b2a80d9f91c3c945a90>`_
- Fix direction convention in ``tabphase`` plugin `[49e40ba] <https://github.com/mitsuba-renderer/mitsuba3/commit/49e40bad03da536136d3c8563eca6582fcb0e895>`_
- Create TLS module lookup cache for new threads `[6f62749] <https://github.com/mitsuba-renderer/mitsuba3/commit/6f62749d97904471315d2143b96af5ad6548da06>`_

Mitsuba 3.0.0
-------------

*July 20, 2022*

- Initial release
