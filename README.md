<img src="https://github.com/mitsuba-renderer/mitsuba2/raw/master/docs/images/logo_plain.png" width="120" height="120" alt="Mitsuba logo">

# Mitsuba Renderer 2
<!--
| Documentation   | Linux             | Windows             |
|      :---:      |       :---:       |        :---:        |
| [![docs][1]][2] | [![rgl-ci][3]][4] | [![appveyor][5]][6] |


[1]: https://readthedocs.org/projects/mitsuba2/badge/?version=master
[2]: https://mitsuba2.readthedocs.io/en/latest/src/getting_started/intro/
[3]: https://rgl-ci.epfl.ch/app/rest/builds/buildType(id:Mitsuba2_Build)/statusIcon.svg
[4]: https://rgl-ci.epfl.ch/viewType.html?buildTypeId=Mitsuba2_Build&guest=1
[5]: https://ci.appveyor.com/api/projects/status/eb84mmtvnt8ko8bh/branch/master?svg=true
[6]: https://ci.appveyor.com/project/wjakob/mitsuba2/branch/master
-->
| Documentation   |
|      :---:      |
| [![docs][1]][2] |


[1]: https://readthedocs.org/projects/mitsuba2/badge/?version=latest
[2]: https://mitsuba2.readthedocs.io/en/latest/src/getting_started/intro/

Mitsuba 2 is a research-oriented rendering system written in portable C++17. It
consists of a small set of core libraries and a wide variety of plugins that
implement functionality ranging from materials and light sources to complete
rendering algorithms. Mitsuba 2 strives to retain scene compatibility with its
predecessor [Mitsuba 0.6](https://github.com/mitsuba-renderer/mitsuba).
However, in most other respects, it is a completely new system following a
different set of goals.

The most significant change of Mitsuba 2 is that it is a *retargetable*
renderer: this means that the underlying implementations and data structures
are specified in a generic fashion that can be transformed to accomplish a
number of different tasks. For example:

1. In the simplest case, Mitsuba 2 is an ordinary CPU-based RGB renderer that
   processes one ray at a time similar to its predecessor [Mitsuba
   0.6](https://github.com/mitsuba-renderer/mitsuba).

2. Alternatively, Mitsuba 2 can be transformed into a differentiable renderer
   that runs on NVIDIA RTX GPUs. A differentiable rendering algorithm is able
   to compute derivatives of the entire simulation with respect to input
   parameters such as camera pose, geometry, BSDFs, textures, and volumes. In
   conjunction with gradient-based optimization, this opens door to challenging
   inverse problems including computational material design and scene reconstruction.

3. Another type of transformation turns Mitsuba 2 into a vectorized CPU
   renderer that leverages Single Instruction/Multiple Data (SIMD) instruction
   sets such as AVX512 on modern CPUs to efficiently sample many light paths in
   parallel.

4. Yet another type of transformation rewrites physical aspects of the
   simulation: Mitsuba can be used as a monochromatic renderer, RGB-based
   renderer, or spectral renderer. Each variant can optionally account for the
   effects of polarization if desired.

In addition to the above transformations, there are
several other noteworthy changes:

1. Mitsuba 2 provides very fine-grained Python bindings to essentially every
   function using [pybind11](https://github.com/pybind/pybind11). This makes it
   possible to import the renderer into a Jupyter notebook and develop new
   algorithms interactively while visualizing their behavior using plots.

2. The renderer includes a large automated test suite written in Python, and
   its development relies on several continuous integration servers that
   compile and test new commits on different operating systems using various
   compilation settings (e.g. debug/release builds, single/double precision,
   etc). Manually checking that external contributions don't break existing
   functionality had become a severe bottleneck in the previous Mitsuba 0.6
   codebase, hence the goal of this infrastructure is to avoid such manual
   checks and streamline interactions with the community (Pull Requests, etc.)
   in the future.

3. An all-new cross-platform user interface is currently being developed using
   the [NanoGUI](https://github.com/mitsuba-renderer/nanogui) library. *Note
   that this is not yet complete.*

## Compiling and using Mitsuba 2

Please see the [documentation](http://mitsuba2.readthedocs.org/en/latest) for
details on how to compile, use, and extend Mitsuba 2.

## About

This project was created by [Wenzel Jakob](http://rgl.epfl.ch/people/wjakob).
Significant features and/or improvements to the code were contributed by
[Merlin Nimier-David](https://merlin.nimierdavid.fr/),
[Guillaume Loubet](https://maverick.inria.fr/Membres/Guillaume.Loubet/),
[Sébastien Speierer](https://github.com/Speierers),
[Delio Vicini](https://dvicini.github.io/),
and [Tizian Zeltner](https://tizianzeltner.com/).
