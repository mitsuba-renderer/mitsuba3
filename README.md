<img src="https://github.com/mitsuba-renderer/mitsuba2/raw/master/docs/images/logo_plain.png" width="120" height="120" alt="Mitsuba logo">

# Mitsuba Renderer 2

| Linux                     | Windows                     |
|---------------------------|-----------------------------|
| [![rgl-ci][1]][2]         | [![appveyor][3]][4]         |

[1]: https://rgl-ci.epfl.ch/app/rest/builds/buildType(id:Mitsuba2_Build)/statusIcon.svg
[2]: https://rgl-ci.epfl.ch/viewType.html?buildTypeId=Mitsuba2_Build&guest=1
[3]: https://ci.appveyor.com/api/projects/status/eb84mmtvnt8ko8bh/branch/master?svg=true
[4]: https://ci.appveyor.com/project/wjakob/mitsuba2/branch/master

Mitsuba 2 is a research-oriented rendering system written in portable C++17.
It consists of a small set of core libraries and a wide variety of plugins that
implement functionality ranging from materials and light sources to complete
rendering algorithms.

## Compiling

Compiling from scratch requires CMake and a recent version of XCode on Mac,
Visual Studio 2017 on Windows, or GCC/Clang (Clang is preferred) on Linux.

On Linux and MacOS, compiling should be as simple as

```bash
git clone --recursive https://github.com/mitsuba-renderer/mitsuba2
cd mitsuba2

# Build using CMake & GNU Make (legacy)
cmake .
make -j 4

# OR: Build using CMake & Ninja (recommended, must install Ninja first)
cmake -GNinja .
ninja
```

On Windows, open the generated ``mitsuba.sln`` file after running
``cmake .`` and proceed building as usual from within Visual Studio.

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
using *git submodule*. Unfortunately, as of 2018, pulling from the main
repository won't automatically keep the sub-repositories in sync, which can
lead to various problems. The following command installs a git alias named
``pullall`` that automates these two steps.

```bash
git config --global alias.pullall '!f(){ git pull "$@" && git submodule update --init --recursive; }; f'
```

Afterwards, simply write
```bash
git pullall
```
to stay in sync.

## About

This project was created by [Wenzel Jakob](http://rgl.epfl.ch/people/wjakob)
Significant features and/or improvements to the code were contributed by
Merlin Nimier-David and Sébastien Speierer.
