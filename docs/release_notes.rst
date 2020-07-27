Release notes
=============

Being an experimental research framework, Mitsuba 2 does not follow the
`Semantic Versioning <https://semver.org/>`_ convention. That said, we will
strive to document breaking API changes in the release notes below.


Incoming release
----------------

- TBA

Mitsuba 2.2.1
-------------

*July 27, 2020*

- Fix warnings, build and tests on Windows (MSVC)

Mitsuba 2.2.0
-------------

*July 22, 2020*

- Instancing support via ``shapegroup`` and ``instance`` plugins (`#170 <https://github.com/mitsuba-renderer/mitsuba2/pull/170>`_)
- Various sampler plugins: ``stratified``, ``multijitter``, ``orthogonal``,
  ``ldsampler`` (`#187 <https://github.com/mitsuba-renderer/mitsuba2/pull/187>`_)
- ``bumpmap`` BSDF plugin (`#201 <https://github.com/mitsuba-renderer/mitsuba2/pull/201>`_)
- ``normalmap`` BSDF plugin
- Raytracing API improvements (`#209 <https://github.com/mitsuba-renderer/mitsuba2/pull/209>`_)
- Differentiable surface interaction (`#209 <https://github.com/mitsuba-renderer/mitsuba2/pull/209>`_)
- Add ``<path>`` XML tag (`#165 <https://github.com/mitsuba-renderer/mitsuba2/pull/165>`_)
- `BlenderMesh` shape plugin
- `Projector` emitter plugin
- Add support for textured area light sampling
- Add ``Mesh::eval_parameterization`` to parameterize the mesh using UV values
- Various bug fixes and other improvements

Mitsuba 2.1.0
-------------

*May 19, 2020*

- Switch to OptiX 7 for GPU ray tracing backend (`#88 <https://github.com/mitsuba-renderer/mitsuba2/pull/88>`_)
- Mesh memory layout redesign (`#88 <https://github.com/mitsuba-renderer/mitsuba2/pull/88>`_)
- Custom shape support for OptiX raytracing backend (`#135 <https://github.com/mitsuba-renderer/mitsuba2/pull/135>`_)
- ``xml.dict_to_xml()`` for writing XML scene file from Python (`#131 <https://github.com/mitsuba-renderer/mitsuba2/pull/131>`_)

Mitsuba 2.0.1
-------------

*May 18, 2020*

- ``xml.load_dict()`` for Mitsuba scene/object construction in Python (`#122 <https://github.com/mitsuba-renderer/mitsuba2/pull/122>`_)
- Scene traversal mechanism enhancement (`#108 <https://github.com/mitsuba-renderer/mitsuba2/pull/108>`_)
- Support premultiplied alpha correctly (`#104 <https://github.com/mitsuba-renderer/mitsuba2/pull/104>`_)
- Spot light plugin (`#100 <https://github.com/mitsuba-renderer/mitsuba2/pull/100>`_)
- Add Blender mesh constructor (`40db7be <https://github.com/mitsuba-renderer/mitsuba2/commit/40db7be01215>`_)
- Irradiancemeter sensor plugin (`b19985b <https://github.com/mitsuba-renderer/mitsuba2/commit/b19985b28568>`_)
- Radiancemeter sensor plugin (`#83 <https://github.com/mitsuba-renderer/mitsuba2/pull/83>`_)
- Distant emitter plugin (`#60 <https://github.com/mitsuba-renderer/mitsuba2/pull/60>`_)
- Custom shape support for Embree raytracing backend (`2c1b63f <https://github.com/mitsuba-renderer/mitsuba2/commit/2c1b63f9d1de>`_)
- Add ability to specify Python default variant (`4472b55 <https://github.com/mitsuba-renderer/mitsuba2/commit/4472b55d080f>`_)
- Add pytest -m "not slow" option (`4597466 <https://github.com/mitsuba-renderer/mitsuba2/commit/4597466d8ca7>`_)
- Moment integrator and Z-tests (`74b2bf6 <https://github.com/mitsuba-renderer/mitsuba2/commit/74b2bf658c7f>`_)
- Spatially-varying `alpha` for roughconductor and roughdielectric (`cac8741 <https://github.com/mitsuba-renderer/mitsuba2/commit/cac8741de935>`_)
- Various bug fixes and other improvements

Mitsuba 2.0.0
-------------

*March 3, 2020*

- Initial release
