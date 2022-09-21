Release notes
=============

Being an experimental research framework, Mitsuba 3 does not strictly follow the
`Semantic Versioning <https://semver.org/>`_ convention. That said, we will
strive to document breaking API changes in the release notes below.


Incoming Release
----------------

- Various minor improvements to the Python typing stub generation `[b7ef349] <https://github.com/mitsuba-renderer/mitsuba3/commit/f883834a50e3dab694b4fe4ceafdfa1ae3712782>`_ .. `[ad72a53] <https://github.com/mitsuba-renderer/mitsuba3/commit/ad72a5361889bcef1f19b702a28956c1549d26e3>`_


Mitsuba 3.0.2
----------------

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
