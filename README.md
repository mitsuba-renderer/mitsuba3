<!-- <img src="https://github.com/mitsuba-renderer/mitsuba3/raw/master/docs/images/logo_plain.png" width="120" height="120" alt="Mitsuba logo"> -->

<img src="https://raw.githubusercontent.com/mitsuba-renderer/mitsuba-data/master/docs/images/banners/banner_01.jpg"
alt="Mitsuba banner">

# Mitsuba Renderer 3

| Documentation   | Linux             | MacOS             | Windows           |
|      :---:      |       :---:       |       :---:       |       :---:       |
| [![docs][1]][2] | [![rgl-ci][3]][4] | [![rgl-ci][5]][6] | [![rgl-ci][7]][8] |

[1]: https://readthedocs.org/projects/mitsuba/badge/?version=latest
[2]: https://mitsuba.readthedocs.io/en/latest/
[3]: https://rgl-ci.epfl.ch/app/rest/builds/buildType(id:Mitsuba3_LinuxAmd64Clang10)/statusIcon.svg
[4]: https://rgl-ci.epfl.ch/viewType.html?buildTypeId=Mitsuba3_LinuxAmd64Clang10&guest=1
[5]: https://rgl-ci.epfl.ch/app/rest/builds/buildType(id:Mitsuba3_LinuxAmd64gcc9)/statusIcon.svg
[6]: https://rgl-ci.epfl.ch/viewType.html?buildTypeId=Mitsuba3_LinuxAmd64gcc9&guest=1
[7]: https://rgl-ci.epfl.ch/app/rest/builds/buildType(id:Mitsuba3_WindowsAmd64msvc2020)/statusIcon.svg
[8]: https://rgl-ci.epfl.ch/viewType.html?buildTypeId=Mitsuba3_WindowsAmd64msvc2020&guest=1

## Introduction

Mitsuba 3 is a research-oriented rendering system for forward and inverse
light-transport simulation. It consists of a small set of core libraries and a
wide variety of plugins that implement functionality ranging from materials and
light sources to complete rendering algorithms. Mitsuba 3 is implemented on top
of [Dr.Jit](https://github.com/mitsuba-renderer/drjit) a just-in-time (JIT)
compiler for ordinary and differentiable computation.

Mitsuba 3 is a *retargetable* renderer: this means that the underlying
implementations and data structures are specified in a generic fashion that can
be transformed to accomplish several different tasks.

## Main Features

- **Cross-platform**: Mitsuba 3 runs natively on Linux (X86_64), macOS (x86_64 & aarch64), and Windows (x86_64)

- **High performance**: Using the Dr.Jit *just-in-time* compiler, computations using Mitsuba 3 can be turned into high-performance fused kernels using either LLVM (when targeting the CPU) or CUDA (when targeting the GPU).

- **Python First**: Mitsuba 3 is built to be deeply integrated into Python, which makes it
more than a simple renderer. It opens the door to flexible scripting with no compromise on performances given the use of a just-in-time compiler.

- **Rendering research toolbox**: Mitsuba 3 provides the tooling necessary to do rendering research and quick prototyping in Python or C++.

- **Differentiation**: Mitsuba 3 is a differentiable renderer, meaning that it can compute derivatives of the entire simulation with respect to input parameters such as camera pose, geometry, BSDFs, textures, and volumes.

- **Spectral & Polarization**: Mitsuba 3 can be used as a monochromatic renderer, RGB-based renderer, or spectral renderer. Each variant can optionally account for the effects of polarization if desired.

## Installation

Mitsuba 3 can be installed via pip from PyPI:

```bash
pip install mitsuba
```

### Requirements

- `Python >= 3.8`
- (optional) For computation on the GPU: `Nvidia driver >= 495.89`
- (optional) For vectorized / parallel computation on the CPU: `LLVM >= 11.1`

## Usage

Here is a simple "Hello World" example that shows how simple it is to render a
scene using Mitsuba 3 from Python:

```python
# Import the library using the alias "mi"
import mitsuba as mi
# Set the variant of the renderer
mi.set_variant('scalar_rgb')
# Load a scene
scene = mi.load_dict(mi.cornell_box())
# Render the scene
img = mi.render(scene)
# Write the rendered image to an EXR file
mi.Bitmap(img).write('cbox.exr')
```

More tutorials can be found in the [documentation][2], covering various
use-cases.

## Compiling from sources

Please see the [documentation](https://mitsuba.readthedocs.io/en/latest/src/developer_guide.html) for
details on how to compile, use, and extend Mitsuba 3.

## About

This project was created by [Wenzel Jakob](http://rgl.epfl.ch/people/wjakob).
Significant features and/or improvements to the code were contributed by
[Sébastien Speierer](https://speierers.github.io/),
[Nicolas Roussel](https://github.com/njroussel),
[Merlin Nimier-David](https://merlin.nimierdavid.fr/),
[Delio Vicini](https://dvicini.github.io/),
[Tizian Zeltner](https://tizianzeltner.com/),
[Baptiste Nicolet](https://bnicolet.com/),
[Miguel Crespo](https://mcrespo.me/),
[Vincent Leroy](https://github.com/leroyvn), and
[Ziyi Zhang](https://github.com/ziyi-zhang).
