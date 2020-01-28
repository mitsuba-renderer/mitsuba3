<img src="https://github.com/mitsuba-renderer/mitsuba2/raw/master/docs/images/logo_plain.png" width="120" height="120" alt="Mitsuba logo">

# Mitsuba Renderer 2

<!--
CI is disabled during refactoring phase

| Linux                     | Windows                     |
|---------------------------|-----------------------------|
| [![rgl-ci][1]][2]         | [![appveyor][3]][4]         |

[1]: https://rgl-ci.epfl.ch/app/rest/builds/buildType(id:Mitsuba2_Build)/statusIcon.svg
[2]: https://rgl-ci.epfl.ch/viewType.html?buildTypeId=Mitsuba2_Build&guest=1
[3]: https://ci.appveyor.com/api/projects/status/eb84mmtvnt8ko8bh/branch/master?svg=true
[4]: https://ci.appveyor.com/project/wjakob/mitsuba2/branch/master
-->

Mitsuba 2 is a research-oriented rendering system written in portable C++17. It
consists of a small set of core libraries and a wide variety of plugins that
implement functionality ranging from materials and light sources to complete
rendering algorithms.

Mitsuba 2 strives to retain scene compatibility with its predecessor [Mitsuba
0.6](https://github.com/mitsuba-renderer/mitsuba). However, in most other
respects, it is a completely new system following a different set of goals.

The most significant change of Mitsuba 2 is that it is a *retargetable*
renderer: this means that the underlying implementations and data structures
are specified in a generic fashion that can be transformed to accomplish a
number of different tasks.

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
   renderer that leverages leverage Single Instruction/Multiple Data (SIMD)
   instruction sets such as AVX512 on modern CPUs to efficiently sample many
   light paths in parallel.

4. Yet another type of transformation rewrites physical aspects of the
   simulation: Mitsuba can be used as a monochromatic renderer, RGB-based
   renderer, or spectral renderer. Each variant can optionally account for the
   effects of polarization if desired.

In addition to the above transformations, there are
several other noteworthy changes:

1. Mitsuba 2 provides very fine-grained Python bindings to essentially every
   function thanks to [pybind11](https://github.com/pybind/pybind11). This
   makes it possible to import the renderer into a Jupyter notebook and develop
   new algorithms interactively while visualizing their behavior using plots.

2. The renderer includes a large automated test suite written in Python, and
   its development relies on several continuous integration servers that
   compile and test new commits on different operating systems using various
   compilation settings (e.g. debug/release builds, single/double precision,
   etc).

3. An all-new cross-platform user interface was developed using the
   [NanoGUI](https://github.com/mitsuba-renderer/nanogui) library.

## Documentation

Mitsuba 2 is still under heavy development and does not provide detailed
documentation of plugin parameters, etc. In the meantime, we refer you to the
[Mitsuba 0.5.0
documentation](https://www.mitsuba-renderer.org/releases/current/documentation.pdf).

## Development

Mitsuba 2 is a completely new codebase, and existing Mitsuba 0.6 plugins will
require significant changes to be compatible with the architecture of the new
system.

New developers will want to begin by thoroughly reading the documentation of
[Enoki](https://enoki.readthedocs.io/en/master/index.html) before looking at
any Mitsuba code. Enoki is a template library for vector and matrix arithmetic
that constitutes the foundation of Mitsuba 2. It also drives the code
transformations, enabling systematic vectorization and automatic
differentiation of the renderer.

Another major code change is that Mitsuba 2 code uses an ``underscore_case``
naming convention for function names and variables (in contrast to Mitsuba 0.4,
which used ``camelCase`` everywhere). We've essentially imported Python's [PEP
8](https://www.python.org/dev/peps/pep-0008) into the C++ side (which does not
specify a recommended naming convention), ensuring that code that uses
functionality from both languages looks natural.

## Compiling on Linux

Please install the following prerequisites (e.g. on Ubuntu):

```
sudo libxrandr-dev libxinerama-dev libxcursor-dev libz-dev libpng-dev libjpeg-dev cmake ninja-build
```

Also, note that Mitsuba requires a somewhat recent version of Python (the
minimum is **Python 3.6**).

Following this, compiling should be as simple as

```bash
git clone --recursive https://github.com/mitsuba-renderer/mitsuba2
cd mitsuba2
cmake -GNinja .
ninja
```

## Compiling on MacOS

On MacOS, you will need to install XCode, CMake, and
[Ninja](https://ninja-build.org/). Following this, compiling should be as
simple as

```bash
git clone --recursive https://github.com/mitsuba-renderer/mitsuba2
cd mitsuba2
cmake -GNinja .
ninja
```

## Compiling on Windows

Compilation on Windows requires Visual Studio 2019. Open the generated
``mitsuba.sln`` file after running ``cmake .`` and proceed building as usual
from within Visual Studio.

## Running Mitsuba

Once Mitsuba is compiled, run the ``setpath.sh/bat`` script to configure
environment variables (``PATH/LD_LIBRARY_PATH/PYTHONPATH``) that are
required to run Mitsuba.

```bash
# On Linux / Mac OS
$ source setpath.sh

# On Windows
C:\...\mitsuba2> setpath
```

## Running the tests

To run the test suite, simply invoke ``pytest``:

```bash
$ pytest
```

The build system also exposes a ``pytest`` target that executes ``setpath`` and
parallelizes the test execution.

```bash
# if using Makefiles
$ make pytest

# if using ninja
$ ninja pytest
```

## Staying up-to-date

Mitsuba organizes its software dependencies in a hierarchy of sub-repositories
using *git submodule*. Unfortunately, pulling from the main repository won't
automatically keep the sub-repositories in sync, which can lead to various
problems. The following command installs a git alias named ``pullall`` that
automates these two steps.

```bash
git config --global alias.pullall '!f(){ git pull "$@" && git submodule update --init --recursive; }; f'
```

Afterwards, simply write
```bash
git pullall
```
to stay in sync.

## Frequently Asked Questions

- Differentiable rendering fails with the error message "Failed to load Optix library":
It is likely that the version of OptiX installed on your system is not compatible with the video
driver (the two must satisfy version requirements that are detailed on the OptiX website).

## About

This project was created by [Wenzel Jakob](http://rgl.epfl.ch/people/wjakob).
Significant features and/or improvements to the code were contributed by
[Merlin Nimier-David](https://merlin.nimierdavid.fr/),
[Guillaume Loubet](https://maverick.inria.fr/Membres/Guillaume.Loubet/),
[Sébastien Speierer](https://github.com/Speierers),
[Delio Vicini](https://dvicini.github.io/),
and [Tizian Zeltner](https://tizianzeltner.com/).
