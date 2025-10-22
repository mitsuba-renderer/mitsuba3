Release notes
=============

Being an experimental research framework, Mitsuba 3 does not strictly follow the
`Semantic Versioning <https://semver.org/>`__ convention. That said, we will
strive to document breaking API changes in the release notes below.

Mitsuba 3.7.1
-------------
*September 17, 2025*

- Upgrade Dr.Jit to version `1.2.0
  <https://github.com/mitsuba-renderer/drjit/releases/tag/v1.2.0>`__. This
  release brings several important improvements and several bug fixes:

  - **Event API** for fine-grained GPU kernel timing and synchronization,
    enabling better performance profiling.
  - **Enhanced CUDA-OpenGL interoperability** with simplified APIs for
    efficient data sharing between CUDA and OpenGL.
  - **Register spilling to shared memory** on CUDA backend, improving
    performance for complex kernels with high register pressure.
  - **Memory view support** for zero-copy data access from Python.

  The remainder lists Mitsuba-specific additions.

- **Improvements**:

  - Improved bump and normal mapping with two important fixes for more robust
    and artifact-free rendering. Both the ``bumpmap`` and ``normalmap`` BSDFs now
    include: (1) **Invalid normal flipping** - ensures perturbed normals are always
    consistent with the geometric normal by flipping shading normals when needed,
    following the approach in Schüssler et al. 2017 :cite:`Schuessler2017Microfacet`.
    (2) **Microfacet-based shadowing** - smooths shadow terminator artifacts using
    the shadowing function from Estevez et al. 2019 :cite:`Estevez2019`. Both
    features are enabled by default but can be disabled via the
    ``flip_invalid_normals`` and ``use_shadowing_function`` parameters for
    backwards compatibility. (commit `e32d71807
    <https://github.com/mitsuba-renderer/mitsuba3/commit/e32d71807>`__,
    contributed by `Delio Vicini <https://github.com/dvicini>`__).
  - Improved sunsky documentation. (PR `#1743
    <https://github.com/mitsuba-renderer/mitsuba3/pull/1743>`__,
    contributed by `Mattéo Santini <https://github.com/matttsss>`__).
  - Added support for vcalls of Texture. (commit `6b1603c77
    <https://github.com/mitsuba-renderer/mitsuba3/commit/6b1603c77>`__).
  - Added Python bindings for ``field<T>`` types. (PR `#1736
    <https://github.com/mitsuba-renderer/mitsuba3/pull/1736>`__,
    contributed by `Delio Vicini <https://github.com/dvicini>`__).
  - Allow multiple Python objects to refer to the same ``Object*``. (PR `#1740
    <https://github.com/mitsuba-renderer/mitsuba3/pull/1740>`__).

- **Bug fixes**:

  - Fixed bug with unintentional reordering of channels when serializing and
    deserializing a Bitmap with more than 10 channels. (commit `e84b18f
    <https://github.com/mitsuba-renderer/mitsuba3/commit/e84b18f01>`__,
    contributed by `Sebastian Winberg <https://github.com/winbergs>`__).
  - Fixed ``hide_emitters`` behavior for ``area`` emitters in ``path``
    integrator and all other integrators. (commits `3c3bf14c
    <https://github.com/mitsuba-renderer/mitsuba3/commit/3c3bf14c1>`__,
    `0755134e0 <https://github.com/mitsuba-renderer/mitsuba3/commit/0755134e0>`__,
    `c967a0a24 <https://github.com/mitsuba-renderer/mitsuba3/commit/c967a0a24>`__).
  - Fixed KDTree reference counting and shutdown procedure. (commit `14c8c9763
    <https://github.com/mitsuba-renderer/mitsuba3/commit/14c8c9763>`__).
  - Fixed compilation issues of the KDTree. (commit `65b38126b
    <https://github.com/mitsuba-renderer/mitsuba3/commit/65b38126b>`__).
  - Prevent NaN values for normals of triangles with zero area. (PR `#1733
    <https://github.com/mitsuba-renderer/mitsuba3/pull/1733>`__,
    contributed by `Delio Vicini <https://github.com/dvicini>`__).
  - Prevent users updating the ``UniformSpectrum`` with a float of size
    different than 1. (PR `#1722 <https://github.com/mitsuba-renderer/mitsuba3/pull/1722>`__,
    contributed by `Mattéo Santini <https://github.com/matttsss>`__).
  - Added Image manipulation tutorial back in the "How-to Guides". (commit `465609174
    <https://github.com/mitsuba-renderer/mitsuba3/commit/465609174>`__,
    contributed by `Baptiste Nicolet <https://github.com/bathal1>`__).
  - Add support for JIT-freeting to the sunsky classes. (commit
    `f07f26c5e <https://github.com/mitsuba-renderer/mitsuba3/commit/f07f26c5e>`__).


Mitsuba 3.7.0
-------------
*August 7, 2025*

- Upgrade Dr.Jit to version `1.1.0
  <https://github.com/mitsuba-renderer/drjit/releases/tag/v1.1.0>`__. The
  following list summarizes major new features added to Dr.Jit. See the `Dr.Jit
  release notes <https://drjit.readthedocs.io/en/v1.1.0/changelog.html>`__ for
  additional detail and various smaller features that are not listed here.

  - **Cooperative vectors** and a **neural network library**: Dr.Jit now
    supports efficient `matrix-vector arithmetic
    <https://drjit.readthedocs.io/en/v1.1.0/coop_vec.html>`__ that compiles to
    tensor core machine instructions on NVIDIA GPUs and packet instructions
    (e.g., AVX512) on the LLVM backend. A modular `neural network library
    <https://drjit.readthedocs.io/en/v1.1.0/nn.html>`__ facilitates evaluating
    and optimizing fully fused MLPs in rendering code. (Dr.Jit PR `#384
    <https://github.com/mitsuba-renderer/drjit/pull/384>`__, Dr.Jit-Core PR
    `#141 <https://github.com/mitsuba-renderer/drjit-core/pull/141>`__).

  - **Hash grid encoding**: Neural network hash grid encoding inspired by
    Instant NGP, including both traditional hash grids and permutohedral
    encodings for high-dimensional inputs.
    (Dr.Jit PR `#390 <https://github.com/mitsuba-renderer/drjit/pull/390>`__,
    contributed by `Christian Döring <https://github.com/DoeringChristian>`__
    and `Merlin Nimier-David <https://merlin.nimierdavid.fr>`__).

  - **Function freezing**: The :py:func:`@drjit.freeze <drjit.freeze>`
    decorator eliminates repeated tracing overhead by `caching and replaying
    JIT-compiled kernels
    <https://drjit.readthedocs.io/en/v1.1.0/freeze.html>`__, which can dramatically
    accelerate programs with repeated computations.
    (Dr.Jit PR `#336 <https://github.com/mitsuba-renderer/drjit/pull/336>`__,
    Dr.Jit-Core PR `#107 <https://github.com/mitsuba-renderer/drjit-core/pull/107>`__,
    contributed by `Christian Döring <https://github.com/DoeringChristian>`__).

  - **Shader Execution Reordering (SER)**: :py:func:`drjit.reorder_threads`
    shuffles threads to reduce warp-level divergence, improving performance for
    branching code.
    (Dr.Jit PR `#395 <https://github.com/mitsuba-renderer/drjit/pull/395>`__,
    Dr.Jit-Core PR `#145 <https://github.com/mitsuba-renderer/drjit-core/pull/145>`__).

  - **New random number generation API**: The function :py:func:`drjit.rng`
    returns a :py:class:`drjit.random.Generator` object (resembling analogous
    API in NumPy, PyTorch, etc.) that computes high-quality random variates
    that are statistically independent within and across parallel streams.
    (Dr.Jit PR `#417 <https://github.com/mitsuba-renderer/drjit/pull/417>`__).

  - **Array resampling and convolution**: New functions
    :py:func:`drjit.resample` and :py:func:`drjit.convolve` support
    differentiable signal processing with various reconstruction filters.
    (Dr.Jit PRs `#358 <https://github.com/mitsuba-renderer/drjit/pull/358>`__,
    `#378 <https://github.com/mitsuba-renderer/drjit/pull/378>`__).

  - **Gradient-based optimizers**: New :py:mod:`drjit.opt` module with
    :py:class:`drjit.opt.SGD`, :py:class:`drjit.opt.Adam`, and
    :py:class:`drjit.opt.RMSProp` optimizers. They improve upon the previous
    Mitsuba versions and include support for adaptive mixed-precision training.
    (Dr.Jit PRs `#345 <https://github.com/mitsuba-renderer/drjit/pull/345>`__,
    `#402 <https://github.com/mitsuba-renderer/drjit/pull/402>`__).

  - **TensorFlow interoperability**: :py:func:`@drjit.wrap <drjit.wrap>`
    enables seamless integration with TensorFlow.
    (Dr.Jit PR `#301 <https://github.com/mitsuba-renderer/drjit/pull/301>`__,
    contributed by `Jakob Hoydis <https://github.com/jhoydis>`__).

  - **Enhanced tensor operations**: New functions :py:func:`drjit.concat`,
    :py:func:`drjit.take`, :py:func:`drjit.take_interp`, and
    :py:func:`drjit.moveaxis` for tensor manipulation.

  - **Performance improvements**: Packet scatter-add operations, optimized
    texture access, and faster :py:func:`drjit.rsqrt` on the LLVM backend
    (Dr.Jit PRs `#343 <https://github.com/mitsuba-renderer/drjit/pull/343>`__,
    `#329 <https://github.com/mitsuba-renderer/drjit/pull/329>`__, `#406
    <https://github.com/mitsuba-renderer/drjit/pull/406>`__, Dr.Jit-Core PR
    `#151 <https://github.com/mitsuba-renderer/drjit-core/pull/151>`__),

  The remainder lists Mitsuba-specific additions.

- **Function freezing**. Using the previously mentioned :py:func:`@dr.freeze
  <drjit.freeze>` feature, it is now possible to *freeze* functions that call
  :py:func:`mi.render() <mitsuba.render>`. Rendering another view (e.g., from a
  different viewpoint or with a different material parameter) then merely
  launches the previously compiled kernels instead of tracing the rendering
  process again. This unlocks significant acceleration when repeatedly
  rendering complex scenes from Python (e.g., in optimization loops or
  real-time applications). Some related changes in Mitsuba were required to
  make this possible. (PRs `#1477
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1477>`__, `#1602
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1602>`__, `#1642
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1642>`__,
  contributed by `Christian Döring <https://github.com/DoeringChristian>`__).

- **AD integrators and moving geometry**. All automatic
  differentiation integrators have been updated to correctly handle continuous
  derivative terms arising from moving geometry. In particular, the
  *continuous* (i.e., non-boundary) derivative of various integrators was
  missing partial derivative terms that could be required in certain geometry
  optimization applications. The updated integrators also run ~30% faster
  thanks to Shader Execution Reordering (SER). (PR `#1680
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1680>`__). We thank
  `Markus Worchel <https://github.com/mworchel>`__, Ugo Pavo Finnendahl, and
  `Marc Alexa <https://www.cg.tu-berlin.de/people/marc-alexa>`__ for bringing
  this issue to our attention.

- **Gaussian splatting**. Two new shape plugins support volumetric rendering
  applications based on 3D Gaussian splatting: :ref:`ellipsoids
  <shape-ellipsoids>` is an anisotropic ellipsoid primitives using closed-form
  ray intersection, while :ref:`ellipsoidsmesh <shape-ellipsoidsmesh>` uses a
  mesh-based representation. The :ref:`volprim_rf_basic integrator
  documentation <integrator-volprim_rf_basic>` integrator renders emissive
  volumes based on them (PR `#1464
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1464>`__, contributed by
  `Sebastien Speierer <https://github.com/Speierers>`__).

- The new :ref:`sunsky <emitter-sunsky>` plugin implements
  Hosek-Wilkie models for the `sun
  <https://ieeexplore.ieee.org/document/6459496>`__ and `sky
  <https://dl.acm.org/doi/10.1145/2185520.2185591>`__, where sampling of the
  latter is based on Nick Vitsas and Konstantinos Vardis' `Truncated Gaussian
  Mixture Model
  <https://diglib.eg.org/items/b3f1efca-1d13-44d0-ad60-741c4abe3d21>`__. (PR
  `#1473 <https://github.com/mitsuba-renderer/mitsuba3/pull/1473>`__, `#1461
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1461>`__, `#1491
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1491>`__, contributed by
  `Mattéo Santini <https://github.com/matttsss>`__).

- **Shader Execution Reordering (SER)**. The
  :py:func:`Scene.ray_intersect() <mitsuba.Scene.ray_intersect>` and
  :py:func:`Scene.ray_intersect_preliminary()
  <mitsuba.Scene.ray_intersect_preliminary>` methods now accept a ``reorder``
  parameter to trigger thread reordering on CUDA backends, which shuffles
  threads into coherent warps based on shape IDs. Performance improvements vary
  by scene complexity (ranging from 0.67x to 1.95x speedup). SER can be
  controlled globally via the scene's ``allow_thread_reordering`` parameter or
  by disabling :py:attr:`drjit.JitFlag.ShaderExecutionReordering`. Most
  integrators have been updated to use SER by default. (PR `#1623
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1623>`__).

- The performance of ray tracing kernels run through the CUDA/OptiX backend
  was significantly improved. Previously, several design decisions kept Mitsuba
  off the OptiX "fast path", which is now fixed. (PRs `#1561
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1561>`__, `#1563
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1563>`__, `#1568
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1568>`__).

- Mitsuba now targets the OptiX 8.0 ABI available on NVIDIA driver version 535
  or newer. (PR `#1480
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1480>`__).

- Bitmap textures now use half precision by default. (PR `#1478
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1478>`__.)

- Improvements to the :py:class:`mitsuba.Shape` interface. (PRs `#1484
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1484>`__, `#1485
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1485>`__).

- The Mitsuba optimizers (e.g. Adam) were removed. They are now aliases to more
  sophisticated implementations in Dr.Jit. (Mitsuba PR `#1569
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1569>`__, Dr.Jit PR `#345
  <https://github.com/mitsuba-renderer/drjit/pull/345>`).

- The ``Transform`` API became more relaxed---for example,
  :py:func:`Transform4f.scale() <mituba.Transform4f.scale>` and
  :py:func:`Transform4f().scale() <mituba.Transform4f.scale>` are now both
  equivalent ways of creating a transformation. This removes an API break
  introduced in Mitsuba version 3.6.0. (PR `#1638
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1638>`__).

- **Refactoring**. The codebase underwent several major refactoring passes to
  remove technical debt:

  1. Removal of the legacy thread system and replacement with standard C++
     constructs (PR `#1622 <https://github.com/mitsuba-renderer/mitsuba3/pull/1622>`__).

  2. Removal of the legacy object system and replacement with standard C++
     constructs; rewrite of the :py:class:`mi.Properties <mitsuba.Properties>`
     and plugin loader implementations (PR `#1630
     <https://github.com/mitsuba-renderer/mitsuba3/pull/1630>`__).

  3. Switched to a new parser and scene IR common to both XML and dictionary
     parsing; further work on :py:class:`mi.Properties <mitsuba.Properties>`
     (PRs `#1669 <https://github.com/mitsuba-renderer/mitsuba3/pull/1669>`__,
     `#1676 <https://github.com/mitsuba-renderer/mitsuba3/pull/1676>`__)

  4. Replaced `Transform4f` by specialized affine and perspective
     transformations. (PR `#1679 <https://github.com/mitsuba-renderer/mitsuba3/pull/1679>`__).

  5. Pass over the test suite to accelerate CI test runs (PR `#1659
     <https://github.com/mitsuba-renderer/mitsuba3/pull/1659>`__)

  This is part of an ongoing effort to modernize and improve legacy Mitsuba code.

- Added an API to easily read/write tensor files from Python and access them
  as Dr.Jit tensor instances in Python/C++ code (PR `#1705
  <https://github.com/mitsuba-renderer/mitsuba3/pull/1705>`__).

- The :ref:`rawconstant <texture-rawconstant>` texture plugin stores raw 1D/3D values without
  any color space conversion or spectral upsampling, useful when exact numerical values need to
  be preserved.  (PR `#1496 <https://github.com/mitsuba-renderer/mitsuba3/pull/1496>`__,
  contributed by `Merlin Nimier-David <https://merlin.nimierdavid.fr>`__).

- Various minor improvements and fixes.
  (PRs `#1350 <https://github.com/mitsuba-renderer/mitsuba3/pull/1350>`__,
  `#1495 <https://github.com/mitsuba-renderer/mitsuba3/pull/1495>`__,
  `#1496 <https://github.com/mitsuba-renderer/mitsuba3/pull/1496>`__,
  `#1527 <https://github.com/mitsuba-renderer/mitsuba3/pull/1527>`__,
  `#1540 <https://github.com/mitsuba-renderer/mitsuba3/pull/1540>`__,
  `#1545 <https://github.com/mitsuba-renderer/mitsuba3/pull/1545>`__,
  `#1547 <https://github.com/mitsuba-renderer/mitsuba3/pull/1547>`__,
  `#1528 <https://github.com/mitsuba-renderer/mitsuba3/pull/1528>`__,
  `#1583 <https://github.com/mitsuba-renderer/mitsuba3/pull/1583>`__,
  `#1522 <https://github.com/mitsuba-renderer/mitsuba3/pull/1522>`__,
  `#1600 <https://github.com/mitsuba-renderer/mitsuba3/pull/1600>`__,
  `#1627 <https://github.com/mitsuba-renderer/mitsuba3/pull/1627>`__,
  `#1628 <https://github.com/mitsuba-renderer/mitsuba3/pull/1628>`__,
  `#1656 <https://github.com/mitsuba-renderer/mitsuba3/pull/1656>`__,
  `#1663 <https://github.com/mitsuba-renderer/mitsuba3/pull/1663>`__,
  `#1668 <https://github.com/mitsuba-renderer/mitsuba3/pull/1668>`__,
  `#1678 <https://github.com/mitsuba-renderer/mitsuba3/pull/1678>`__,
  `#1696 <https://github.com/mitsuba-renderer/mitsuba3/pull/1696>`__, and
  `#1702 <https://github.com/mitsuba-renderer/mitsuba3/pull/1702>`__).

Mitsuba 3.6.4
-------------
*February 4, 2025*

- Upgrade Dr.Jit to version `1.0.5 <https://github.com/mitsuba-renderer/drjit/releases/tag/v1.0.5>`__.
- Fix normalmap `[1a4bea2] <https://github.com/mitsuba-renderer/mitsuba3/commit/1a4bea212c129a5d0239e533107473a5ca89230a>`__
- Fallback mechanism for numerical issues in silhouette sampling `[ce4af8d] <https://github.com/mitsuba-renderer/mitsuba3/commit/ce4af8d31b464f1fc5f52688365eb598272e0153>`__

Mitsuba 3.6.3
-------------
*January 29, 2025*

- Release was retracted

Mitsuba 3.6.2
-------------
*January 16, 2025*

- Enable parallel scene loading by default in ``mitsuba`` CLI (regression)
  `[338898d] <https://github.com/mitsuba-renderer/mitsuba3/commit/338898dcf7b26d70523f22a58d4ac474a6cf8e5c>`__
- Improved ``bitmap`` construction in scalar variants
  `[6af4d37] <https://github.com/mitsuba-renderer/mitsuba3/commit/6af4d377c52bc13b7cafa24cd17b96d68b898f87>`__

Mitsuba 3.6.1
-------------
*January 16, 2025*

- Improve robustness of parallel scene loading
  `[8d48f58] <https://github.com/mitsuba-renderer/mitsuba3/commit/8d48f585f07c6559d9aa346507b5e0c007c02513>`__
- Fixes to ``mi.sample_tea_float``
  `[fd16fbe] <https://github.com/mitsuba-renderer/mitsuba3/commit/fd16fbe2e711379bfb36c3d8bcd5bb066ad0ae82>`__
- Support for complex numbers or quaternions in ``mi.ad.Adam`` optimizer
  `[eff5bf6] <https://github.com/mitsuba-renderer/mitsuba3/commit/eff5bf6eae8cc5448af0193f7be0d0cdbf9c41d2>`__
- Improved error message when ``mi.load_dict`` fails
  `[7db5401] <https://github.com/mitsuba-renderer/mitsuba3/commit/7db5401dcdbdcee70fd28b0736313f1365f279f8>`__
- Add missing implementations for `spot` emitter (for AD)
  `[9336491] <https://github.com/mitsuba-renderer/mitsuba3/commit/933649143dbce3086cb6316a9ee928d29c9053b5>`__

Mitsuba 3.6.0
-------------
*November 25, 2024*

This release comes with a major overhaul of some of the internal components of
Mitsuba 3. Namely, the Python bindings are now created using
`nanobind <https://github.com/wjakob/nanobind>`__ and the just-in-time compiler
Dr.Jit was updated to `version 1.0.0 <https://drjit.readthedocs.io/en/stable/changelog.html#drjit-1-0-0-november-21-2024>`__.

These upgrades lead to the following:

- Performance boost: 1.2x to 2x speedups depending on the JIT backend and scene size
- Improved stubs: auto-completion and type-checking has been greatly improved
- More variants on PyPI: thirteen variants are available in the pre-built wheels

Some breaking changes were made in this process. Please refer to the
`porting guide <https://mitsuba.readthedocs.io/en/v3.6.0/porting_3_6.html>`__ to
get a comprehensive overview of these changes.

This release also includes a series of bug fixes, quality of life improvements
and new features. Here's a non-exhaustive list:

- Support for Embree's robust intersection flag
  `[96e0af2] <https://github.com/mitsuba-renderer/mitsuba3/commit/96e0af2de054c6d21e0ac2f68dd41bcd2cb469e5>`__
- Callback system for variant changes
  `#1367 <https://github.com/mitsuba-renderer/mitsuba3/pull/1367>`__
- ``MeshPtr`` for vectorized ``Mesh`` method calls
  `#1319 <https://github.com/mitsuba-renderer/mitsuba3/pull/1319>`__
- Aliases for the ``ArrayX`` types of Dr.Jit
  `[2e86e5e] <https://github.com/mitsuba-renderer/mitsuba3/commit/2e86e5e013b397391d6a59b09ee8238df03589b4>`__
- Fix attribute evaluation for ``twosided`` BSDFs
  `[5508ee6] <https://github.com/mitsuba-renderer/mitsuba3/commit/5508ee6a392e2b32c1a4360742cbe9c966586458>`__ .. `[7528d9f] <https://github.com/mitsuba-renderer/mitsuba3/commit/7528d9fb2d9012e97ebade224685cc8620a647cd>`__
- A new `guide for using Mitsuba 3 in WSL 2 <https://mitsuba.readthedocs.io/en/v3.6.0/src/optix_setup.html>`__
- ``batch`` sensors expose their inner ``Sensor`` objects when traversed with ``mi.traverse()``
  `#1297 <https://github.com/mitsuba-renderer/mitsuba3/pull/1297>`__
- Python stubs improvements
  `#1260 <https://github.com/mitsuba-renderer/mitsuba3/pull/1260>`__ `#1238 <https://github.com/mitsuba-renderer/mitsuba3/pull/1238>`__
- Updated wheel build process with new variants
  `#1355 <https://github.com/mitsuba-renderer/mitsuba3/pull/1355>`__

Mitsuba 3.5.2
-------------
*June 5, 2024*

Most likely the last release which uses `pybind11 <https://pybind11.readthedocs.io>`__.

- OptiX scene clean-ups could segfault
  `[03f5e13] <https://github.com/mitsuba-renderer/mitsuba3/commit/03f5e1362d0cf1cc8c4edbd6e0e7bfd5ee8705a0>`__

Mitsuba 3.5.1
-------------
*June 5, 2024*

- Upgrade Dr.Jit to `[v0.4.6] <https://github.com/mitsuba-renderer/drjit/releases/tag/v0.4.6>`__
- More robust scene clean-up when using Embree
  `[7bb672c] <https://github.com/mitsuba-renderer/mitsuba3/commit/7bb672c32d64ad9a4996d3c7700d445d2c5750bc>`__
- Support for AOV fields in Python AD integrators
  `[f3b427e] <https://github.com/mitsuba-renderer/mitsuba3/commit/f3b427e02ca9dd1fb2e0fb9b993c67a2779d2052>`__
- Fix potential segfault during OptiX scene clean-up
  `[0bcfc72] <https://github.com/mitsuba-renderer/mitsuba3/commit/0bcfc72b846cd5483109b1323301755e23926e76>`__
- Improve and fix Mesh PMF computations
  `[ced7b22] <https://github.com/mitsuba-renderer/mitsuba3/commit/ced7b2204d7d8beefa149a6c5b43e2ff5796a725>`__ .. `[7d2951a] <https://github.com/mitsuba-renderer/mitsuba3/commit/7d2951a5f3f55a0bda4f40e3c4299441f05e70d5>`__
- ``Shape.parameters_grad_enabled`` now only applies to parameters that introduce visibility discontinuities
  `[3013adb] <https://github.com/mitsuba-renderer/mitsuba3/commit/3013adb4f12a491f8dd37c32bcedf55c7998f9e8>`__
- The ``measuredpolarized`` plugin is now supported in vectorized variants
  `[68b3a5f] <https://github.com/mitsuba-renderer/mitsuba3/commit/68b3a5f20ea00eb83631a7c48585162c6d901a7d>`__
- Fix an issue where the ``constant`` plugin would not reuse kernels
  `[deebe4c] <https://github.com/mitsuba-renderer/mitsuba3/commit/deebe4c64586c129bb0b0280bbaf376e2315991c>`__
- Minor changes to support Nvidia v555 drivers
  `[19bf5a4] <https://github.com/mitsuba-renderer/mitsuba3/commit/19bf5a4d82e760614f766067baf0c8add3bc8a41>`__
- Many numerical and performance improvements to the ``sdfgrid`` shape
  `[455de40] <https://github.com/mitsuba-renderer/mitsuba3/commit/455de408abf7660e1667a1ed810fc6fd903b9db3>`__ .. `[9e156bd] <https://github.com/mitsuba-renderer/mitsuba3/commit/9e156bdf3a33042b16593e3f5de40acb7d22da64>`__

Mitsuba 3.5.0
-------------

- New projective sampling based integrators, see PR `#997 <https://github.com/mitsuba-renderer/mitsuba3/pull/997>`__ for more details.
  Here's a brief overview of some of the major or breaking changes:

  - New ``prb_projective`` and ``direct_projective`` integrators
  - New curve/shadow optimization tutorial
  - Removed reparameterizations
  - Can no longer differentiate ``instance``, ``sdfgrid`` and ``Sensor``'s positions

Mitsuba 3.4.1
-------------
*December 11, 2023*

- Upgrade Dr.Jit to `[v0.4.4] <https://github.com/mitsuba-renderer/drjit/releases/tag/v0.4.4>`__

  - Solved threading/concurrency issues which could break loading of large scenes or long running optimizations
- Scene's bounding box now gets updated on parameter changes
  `[97d4b6a] <https://github.com/mitsuba-renderer/mitsuba3/commit/97d4b6ad4c1ba3471642c177cee01d3adf0bf22e>`__
- Python bindings for ``mi.lookup_ior``
  `[d598d79] <https://github.com/mitsuba-renderer/mitsuba3/commit/d598d79a7d21c76ac9b422b3488137b1d28a33f9>`__
- Fixes to ``mask`` BSDF when differentiated
  `[ee87f1c] <https://github.com/mitsuba-renderer/mitsuba3/commit/ee87f1c01aa1b731bc58057ed9e6944046460a69>`__
- Ray sampling is fixed when ``sample_border`` is used
  `[c10b87b] <https://github.com/mitsuba-renderer/mitsuba3/commit/c10b87b072634db15d55a7dbc55cc3cf8f7c844c>`__
- Rename OpenEXR shared library
  `[9cc3bf4] <https://github.com/mitsuba-renderer/mitsuba3/commit/9cc3bf495da10dcd28e80cc14a145fb178a5ef4c>`__
- Handle phase function differentiation in ``prbvolpath``
  `[5f9eebd] <https://github.com/mitsuba-renderer/mitsuba3/commit/5f9eebd41a3a939096d4509b1d2504586a3bf7c6>`__
- Fixes to linear ``retarder``
  `[8033a80] <https://github.com/mitsuba-renderer/mitsuba3/commit/8033a807091f8315c5cef25f4f1a36a3766fb223>`__
- Avoid copies to host when building 1D distributions
  `[825f44f] <https://github.com/mitsuba-renderer/mitsuba3/commit/825f44f081fb43b23589b2bf0b9b7071af858f2a>`__ .. `[8f71fe9] <https://github.com/mitsuba-renderer/mitsuba3/commit/8f71fe995f40923449478ee05500918710ef27f6>`__
- Fixes to linear ``retarder``
  `[8033a80] <https://github.com/mitsuba-renderer/mitsuba3/commit/8033a807091f8315c5cef25f4f1a36a3766fb223>`__
- Sensor's prinicpal point is now exposed throught ``m̀i.traverse()``
  `[f59faa5] <https://github.com/mitsuba-renderer/mitsuba3/commit/f59faa51929b506608a66522dc841f5317a8d43c>`__
- Minor fixes to ``ptracer`` which could result in illegal memory accesses
  `[3d902a4] <https://github.com/mitsuba-renderer/mitsuba3/commit/3d902a4dbf176c8c8d08e5493f23623659295197>`__
- Other various minor bug fixes

Mitsuba 3.4.0
-------------
*August 29, 2023*

- Upgrade Dr.Jit to v0.4.3
- Add ``mi.variant_context()``: a Python context manager for setting variants
  `[96b219d] <https://github.com/mitsuba-renderer/mitsuba3/commit/96b219d75a69f997623c76611fb6d0b90e2c5c3e>`__
- Emitters may now define a sampling weight
  `[9a5f4c0] <https://github.com/mitsuba-renderer/mitsuba3/commit/9a5f4c0d5f52de7553beb64e82ad139fce879649>`__
- Fix ``bsplinecurve`` and ``linearcurve`` shading frames
  `[3875f9a] <https://github.com/mitsuba-renderer/mitsuba3/commit/3875f9adda5eddf9b233901d52dac6b9238a5c83>`__
- Add implementation of ``LargeSteps`` method for mesh optimizations (includes a new tutorial)
  `[48e6428] <https://github.com/mitsuba-renderer/mitsuba3/commit/48e64283814297bd89306cd4beba718221eacaf3>`__ .. `[130ed55] <https://github.com/mitsuba-renderer/mitsuba3/commit/130ed5522887f5405736f28f2081d04b1c1852c3>`__
- Support for spectral phase functions
  `[c7d5c75] <https://github.com/mitsuba-renderer/mitsuba3/commit/c7d5c75707046ee9ade56604f8a0b1c5b724b729>`__
- Additional resource folders can now be specified in ``mi.load_dict()``
  `[66ea528] <https://github.com/mitsuba-renderer/mitsuba3/commit/66ea5285b1bc9a251eafa0b8449bb0d641e3fa1c>`__
- BSDFs can expose their attributes through a generic ``eval_attribute`` method
  `[cfc425a] <https://github.com/mitsuba-renderer/mitsuba3/commit/cfc425a2b5753127aeb818dab0ebab828dc8f060>`__ .. `[c345d70] <https://github.com/mitsuba-renderer/mitsuba3/commit/c345d700bb273832d4ce2fd753929374fd076d64>`__
- New ``sdfgrid`` shape: a signed distance field on a regular grid
  `[272a5bf] <https://github.com/mitsuba-renderer/mitsuba3/commit/272a5bf10e3590d9ae35144d0819396181bdaef2>`__ .. `[618da87] <https://github.com/mitsuba-renderer/mitsuba3/commit/618da871d19cb36a3879230d3799f3341a657c08>`__
- Support for adjoint differentiation methods through the ``aov`` integrator
  `[c9df8de] <https://github.com/mitsuba-renderer/mitsuba3/commit/c9df8de011e2d835402a4fcc8fe6ef832b4ce40a>`__ .. `[bff5cf2] <https://github.com/mitsuba-renderer/mitsuba3/commit/bff5cf240ad1676eea398c99e32f4d49f0f44925>`__
- Various fixes to ``prbvolpath``
  `[6d78f2e] <https://github.com/mitsuba-renderer/mitsuba3/commit/6d78f2ed30e746a718567a85a740db365e44407b>`__, `[a946691] <https://github.com/mitsuba-renderer/mitsuba3/commit/a946691a0d5272a80ea45f7b5f22f31d697cf290>`__ , `[91b0b7e] <https://github.com/mitsuba-renderer/mitsuba3/commit/91b0b7e7c2732a131fac9149bf1db81429e946b0>`__
- Curve shapes (``bsplinecurve`` and ``linearcurve``) always have back-face culling enabled
  `[188b254] <https://github.com/mitsuba-renderer/mitsuba3/commit/188b25425306fd373e69f07f183f0348d8952496>`__ .. `[01ea7ba] <https://github.com/mitsuba-renderer/mitsuba3/commit/01ea7baedf433dc8c337b29b2741992a3a857ee8>`__
- ``Properties`` can now accept tensor objects, currenlty used in ``bitmap``, ``sdfgrid`` and ``gridvolume``
  `[d030a3a] <https://github.com/mitsuba-renderer/mitsuba3/commit/d030a3a13b0d222e3c6647ebc6ceb0919a2f296b>`__
- New ``hair`` BSDF shading model
  `[91fc8e6] <https://github.com/mitsuba-renderer/mitsuba3/commit/91fc8e6356c95b665853a1d294da5187ea16bd39>`__ .. `[0b9b04a] <https://github.com/mitsuba-renderer/mitsuba3/commit/0b9b04aa2c6ca7d0e1b5f8503317b46f2bb972f8>`__
- Improvements to the ``batch`` sensor (performance, documentation, bug fixes)
  `[527ed22] <https://github.com/mitsuba-renderer/mitsuba3/commit/527ed22c801666efd746aebcfed8c299748777f0>`__ .. `[65e0444] <https://github.com/mitsuba-renderer/mitsuba3/commit/65e0444c59c4d50dd8b8547b05b8a3707353df4a>`__
- Many missing Python bindings were added
- Other various minor bug fixes

Mitsuba 3.3.0
-------------
*April 25, 2023*

- Upgrade Dr.Jit to v0.4.2
- Emitters' members are opaque (fixes long JIT compilation times)
  `[df940c1] <https://github.com/mitsuba-renderer/mitsuba3/commit/df940c128116ffa9518058573aa93dedaca6cc33>`__
- Sensors members are opaque (fixes long JIT compilation times)
  `[c864e08] <https://github.com/mitsuba-renderer/mitsuba3/commit/c864e08f5bfa56388444e8ce0bb2751e35ee33d9>`__
- Fix ``cylinder``'s normals
  `[d9ea8e8] <https://github.com/mitsuba-renderer/mitsuba3/commit/d9ea8e847a0ceea88ad3e28e1e41e36ce800d5b6>`__
- Fix next event estimation (NEE) in volume integrators
- ``mi.xml.dict_to_xml`` now supports volumes
  `[15d63df] <https://github.com/mitsuba-renderer/mitsuba3/commit/15d63df4d3eab283de0c7ed511c312bba504ec46>`__
- Allow extending ``AdjointIntegrator`` in Python
  `[15d63df] <https://github.com/mitsuba-renderer/mitsuba3/commit/c4a8b31ee764a0e6d56d9075708c3c76062854be>`__
- ``mi.load_dict()`` is parallel (by default)
  `[bb672ed] <https://github.com/mitsuba-renderer/mitsuba3/commit/bb672ed7cee006ff37819030b9f269f0da263568>`__
- Upsampling routines now support ``box`` filters
  `[64e2ab1] <https://github.com/mitsuba-renderer/mitsuba3/commit/64e2ab1718e6f6959233b1f0ae18337e7a642684>`__
- The ``Mesh.write_ply()`` function writes ``s, t`` rather than ``u, v`` fields
  `[fe4e448] <https://github.com/mitsuba-renderer/mitsuba3/commit/fe4e4484becc3a7997413f648b4efeb75667554b>`__
- All shapes can hold ``Texture`` attributes which can be evaluated
  `[f6ec944] <https://github.com/mitsuba-renderer/mitsuba3/commit/f6ec944c4beb8b0136dff6136e52bc0851acd931>`__
- Radiative backpropagation style integrators use less memory
  `[c1a9b8f] <https://github.com/mitsuba-renderer/mitsuba3/commit/c1a9b8fa52cea4fff4e25a8169ad8be811b1574e>`__
- New ``bsplinecurve`` and ``linearcurve`` shapes
  `[e4c847f] <https://github.com/mitsuba-renderer/mitsuba3/commit/e4c847fedf9005f80bda58a9f6bcfd05581b884c>`__ .. `[79eb026] <https://github.com/mitsuba-renderer/mitsuba3/commit/79eb026d6d594076994dba2c44de81c63b7806f4>`__

Mitsuba 3.2.1
-------------
*February 22, 2023*

- Upgrade Dr.Jit to v0.4.1
- ``Film`` plugins can now have error-compensated accumulation in JIT modes
  `[afeefed] <https://github.com/mitsuba-renderer/mitsuba3/commit/afeefedc8db0d7381e023f80c00f527ce28725b7>`__
- Fix and add missing Python bindings for ``Endpoint``/``Emitter``/``Sensor``
  `[8f03c7d] <https://github.com/mitsuba-renderer/mitsuba3/commit/8f03c7db7b697a2bac17fe960a8d4a6863bece4d>`__
- Numerically robust sphere-ray intersections
  `[7d46e10] <https://github.com/mitsuba-renderer/mitsuba3/commit/7d46e10154b19945b2e4ee97ba7876ac917692c8>`__ .. `[0b483bf] <https://github.com/mitsuba-renderer/mitsuba3/commit/0b483bff5fdcc6d9663d73626bb1dd46674311a6>`__
- Fix parallel scene loading with Python plugins
  `[93bb99b] <https://github.com/mitsuba-renderer/mitsuba3/commit/93bb99b1ed20a3263b2fd82f1d5ab3a333afc002>`__
- Various minor bug fixes

Mitsuba 3.2.0
-------------
*January 6, 2023*

- Upgrade Dr.Jit to v0.4.0

  - Various bug fixes
  - Stability improvements (race conditions, invalid code generation)
  - Removed 4 billion variable limit
- Add missing Python bindings for ``Shape`` and ``ShapePtr``
  `[bdce950] <https://github.com/mitsuba-renderer/mitsuba3/commit/bdce9509f0504163678e81c6afdd7a8bc9c45340>`__
- Fix Python bindings for ``Scene``
  `[4cd5585] <https://github.com/mitsuba-renderer/mitsuba3/commit/4cd558587d711fb35444d5e21c2ab32f74776e65>`__
- Fix bug which would break the AD graph in ``spectral`` variants
  `[f3ac81b] <https://github.com/mitsuba-renderer/mitsuba3/commit/f3ac81bc5c6ce65d5843dde3a1d5f230353453e3>`__
- Parallel scene loading in JIT variants
  `[48c14a7] <https://github.com/mitsuba-renderer/mitsuba3/commit/48c14a709dcc6da9e44583e85eda5735f1888093>`__ .. `[187da96] <https://github.com/mitsuba-renderer/mitsuba3/commit/187da96afd45e14c17d82909fbbf50cb713c8196>`__
- Fix sampling of ``hg`` ``PhaseFunction``
  `[10d3514] <https://github.com/mitsuba-renderer/mitsuba3/commit/10d3514a0295cad4ac6d440c7ff326561c6da6a2>`__
- Fix `envmap` updating in JIT variants
  `[7bf132f] <https://github.com/mitsuba-renderer/mitsuba3/commit/7bf132f6ae3ec46085a7b24bdb1fcce84983425e>`__
- Expose ``PhaseFunction`` of ``Medium`` objects through ``mi.traverse()``
  `[cca5791] <https://github.com/mitsuba-renderer/mitsuba3/commit/cca5791aac22cdf7b3b12cd7a69f7a6800fc715b>`__

Mitsuba 3.1.1
-------------
*November 25, 2022*

- Fixed maximum limits for OptiX kernel launches
  `[a8e6989] <https://github.com/mitsuba-renderer/mitsuba3/commit/a8e69898eacde51954bbc91b34924448b4f8c954>`__


Mitsuba 3.1.0
-------------

New features
^^^^^^^^^^^^

- Enable ray tracing against two different scenes in a single kernel
  `[df79cb3] <https://github.com/mitsuba-renderer/mitsuba3/commit/df79cb3e2837e9296bc3e4ff2afb57416af102f4>`__
- Make ``ShapeGroup`` traversable and updatable
  `[e0871aa] <https://github.com/mitsuba-renderer/mitsuba3/commit/e0871aa8ab58b64216247ed189a77e5e009297d2>`__
- Enable differentiation of ``to_world`` in ``instance``
  `[54d2d3a] <https://github.com/mitsuba-renderer/mitsuba3/commit/54d2d3ab785f8fee4ade8581649ed82d653847cb>`__
- Enable differentiation of ``to_world`` in ``sphere``, ``rectangle``, ``disk`` and ``cylinder``
  `[b5d8c5d] <https://github.com/mitsuba-renderer/mitsuba3/commit/f5dbedec9bab3c45d31255532da07b0c01f5374c>`__ .. `[b5d8c] <https://github.com/mitsuba-renderer/mitsuba3/commit/b5d8c5dc8f33b65613ca27819771950ab9909824>`__
- Enable differentiation of ``to_world`` in ``perspective`` and ``thinlens``
  `[ea513f7] <https://github.com/mitsuba-renderer/mitsuba3/commit/ef9f559e0989fd01b43acce90892ba9e0dea255b>`__ .. `[ea513f] <https://github.com/mitsuba-renderer/mitsuba3/commit/ea513f73b65b8776afb75fdc8d40db4b1140345e>`__
- Add ``BSDF::eval_diffuse_reflectance()`` to most BSDF plugins
  `[59af884] <https://github.com/mitsuba-renderer/mitsuba3/commit/59af884e6fae3a50074921136329d80462b32413>`__
- Add ``mi.OptixDenoiser`` class for simple denoising in Python
  `[5529318] <https://github.com/mitsuba-renderer/mitsuba3/commit/1323497f4e675a8004529eef8404cdc541ade7cf>`__ .. `[55293] <https://github.com/mitsuba-renderer/mitsuba3/commit/552931890df648a5416b0d54d15488f6e766797a>`__
- ``envmap`` plugin can be constructed from ``mi.Bitmap`` object
  `[9389c8d] <https://github.com/mitsuba-renderer/mitsuba3/commit/9389c8d1d16aa7a46d0a54f64eec1d10a1ae1ffd>`__

Other improvements
^^^^^^^^^^^^^^^^^^

- Major performance improvements in ``cuda_*`` variants with new version of Dr.Jit
- Deprecated ``samples_per_pass`` parameter
  `[8ba8528] <https://github.com/mitsuba-renderer/mitsuba3/commit/8ba8528abbad6add1f6a97b30b79ce53c4ff37bf>`__
- Fix rendering progress bar on Windows
  `[d8db806] <https://github.com/mitsuba-renderer/mitsuba3/commit/d8db806ae286358b31ade67dc714de666b25443f>`__
- ``obj`` file parsing performance improvements on Windows
  `[28660f3] <https://github.com/mitsuba-renderer/mitsuba3/commit/28660f3ab9db8f1da58cc38d2fd309cff4871e7e>`__
- Fix ``mi.luminance()`` for monochromatic modes
  `[61b9516] <https://github.com/mitsuba-renderer/mitsuba3/commit/61b9516a742f29e3a5d20e41c50be90d04509539>`__
- Add bindings for ``PluginManager.create_object()``
  `[4ebf700] <https://github.com/mitsuba-renderer/mitsuba3/commit/4ebf700c61e92bb494d605527961882da47a71c0>`__
- Fix ``SceneParameters.update()`` unnecessary hash computation
  `[f57e741] <https://github.com/mitsuba-renderer/mitsuba3/commit/f57e7416ac263445e1b74eeaf661361f4ba94855>`__
- Fix numerical instabilities with ``box`` filter splatting
  `[2d89762] <https://github.com/mitsuba-renderer/mitsuba3/commit/2d8976266588e9b782f63f689c68648424b4898d>`__
- Improve ``math::bisect`` algorithm
  `[7ca09a3] <https://github.com/mitsuba-renderer/mitsuba3/commit/7ca09a3ad95cec306c538493fa8450a096560891>`__
- Fix syntax highlighting in documentation and tutorials
  `[5aa2716] <https://github.com/mitsuba-renderer/mitsuba3/commit/5aa271684424eca5a46f93946536bc7d0c1bc099>`__
- Fix ``Optimizer.set_learning_rate`` for ``int`` values
  `[53143db] <https://github.com/mitsuba-renderer/mitsuba3/commit/53143db05739b964b7a489f58dbd1bd4da87533c>`__
- Various minor improvements to the Python typing stub generation
  `[b7ef349] <https://github.com/mitsuba-renderer/mitsuba3/commit/f883834a50e3dab694b4fe4ceafdfa1ae3712782>`__ .. `[ad72a53] <https://github.com/mitsuba-renderer/mitsuba3/commit/ad72a5361889bcef1f19b702a28956c1549d26e3>`__
- Minor improvements to the documentation
- Various other minor fixes

Mitsuba 3.0.2
-------------
*September 13, 2022*

- Change behavior of ``<spectrum ..>`` and ``<rgb ..>`` tag at scene loading for better consistency between ``*_rgb`` and ``*_spectral`` variants
  `[f883834] <https://github.com/mitsuba-renderer/mitsuba3/commit/f883834a50e3dab694b4fe4ceafdfa1ae3712782>`__
- Polarization fixes
  `[2709889] <https://github.com/mitsuba-renderer/mitsuba3/commit/2709889b9b6970018d58cb0a974f99a885b31dbe>`__, `[06c2960] <https://github.com/mitsuba-renderer/mitsuba3/commit/06c2960b170a655cda831c57b674ec26da7a008f>`__
- Add PyTorch/Mitsuba interoperability tutorial using ``dr.wrap_ad()``
- Fix DLL loading crash when working with Mitsuba and PyTorch in Python
  `[59d7b35] <https://github.com/mitsuba-renderer/mitsuba3/commit/59d7b35c0a7968957e8469f43c308683b63df5c4>`__
- Fix crash when evaluating Mitsuba ray tracing kernel from another thread in ``cuda`` mode.
  `[cd0846f] <https://github.com/mitsuba-renderer/mitsuba3/commit/cd0846ffc570b13ece9fb6c1d3a05411d1ce4eef>`__
- Add stubs for ``Float``, ``ScalarFloat`` and other builtin types
  `[8249179] <https://github.com/mitsuba-renderer/mitsuba3/commit/824917976176cb0a5b2a2b1cf1247e36e6b866ce>`__
- Plugins ``regular`` and ``blackbody`` have renamed parameters: ``wavelength_min``, ``wavelength_max`` (previously ``lambda_min``, ``lambda_max``)
  `[9d3487c] <https://github.com/mitsuba-renderer/mitsuba3/commit/9d3487c4846c5e9cc2a247afd30c4bbf3cbaae46>`__
- Dr.Jit Python stubs are generated during local builds
  `[4302caa8] <https://github.com/mitsuba-renderer/mitsuba3/commit/4302caa8bfd200a0edd6455ba64f92eab2be5824>`__
- Minor improvements to the documentation
- Various other minor fixes

Mitsuba 3.0.1
-------------
*July 27, 2022*

- Various minor fixes in documentation
- Added experimental ``batch`` sensor plugin
  `[0986152] <https://github.com/mitsuba-renderer/mitsuba3/commit/09861525e6c2ab677172dffc6204768c3d424c3e>`__
- Fix LD sampler for JIT modes
  `[98a8ecb] <https://github.com/mitsuba-renderer/mitsuba3/commit/98a8ecb2390ebf35ef5f54f28cccaf9ab267ea48>`__
- Prevent rebuilding of kernels for each sensor in an optimization
  `[152352f] <https://github.com/mitsuba-renderer/mitsuba3/commit/152352f87b5baea985511b2a80d9f91c3c945a90>`__
- Fix direction convention in ``tabphase`` plugin
  `[49e40ba] <https://github.com/mitsuba-renderer/mitsuba3/commit/49e40bad03da536136d3c8563eca6582fcb0e895>`__
- Create TLS module lookup cache for new threads
  `[6f62749] <https://github.com/mitsuba-renderer/mitsuba3/commit/6f62749d97904471315d2143b96af5ad6548da06>`__

Mitsuba 3.0.0
-------------
*July 20, 2022*

- Initial release
