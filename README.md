<p align="center">
  <img src="https://github.com/tjueterb/misuka-data/raw/main/docs/images/misuka_logo_green.svg" width="350" alt="misuka logo">
</p>

# misuka

[1]: https://dl.acm.org/doi/10.1145/3730900
[2]: https://mitsuba.readthedocs.io/en/stable/

## Introduction

misuka is a research-focused room acoustic renderer for forward and inverse sound transport simulation, developed in collaboration between the [Audio Communication Group](https://www.tu.berlin/en/ak) and the [Computer Graphics Group](https://www.cg.tu-berlin.de/) at [TU Berlin](https://www.tu.berlin/).

It is a fully compatible extension to [Mitsuba 3](https://github.com/mitsuba-renderer/mitsuba3), adding plugins for acoustic simulation.


## Main Features

- **Differentiation**: misuka is a differentiable renderer, meaning that it can compute derivatives of the entire simulation with respect to input parameters such as material properties, emitter and receiver positions, and scene geometry. It implements [Time-Resolved Path Replay Backpropagation](1) for efficient gradient estimation.

- **Cross-platform**: misuka has been tested on Linux (``x86_64``), macOS (``arm64``), and Windows (``x86_64``).

- **High performance**: The underlying Dr.Jit compiler fuses rendering code into kernels that achieve state-of-the-art performance using an LLVM backend targeting the CPU and a CUDA/OptiX backend targeting NVIDIA GPUs with ray tracing hardware acceleration.

- **Python first**: misuka is deeply integrated with Python. Materials, textures, and even full rendering algorithms can be developed in Python, which the system JIT-compiles (and optionally differentiates) on the fly. This enables the experimentation needed for research in computer graphics and other disciplines.

## Disclaimer

misuka is currently under heavy development. Additional features will be added in the near future. The user interface might change.

## Tutorials, documentation

You can find tutorials for forward rendering and gradient-based optimization in the folder `tutorials_acoustic`. More tutorials and a full documentation will be added successively.

## Installation

misuka can be compiled analogously to Mitsuba 3.
Please refer the [Mitsuba 3 documentation](https://mitsuba.readthedocs.io/en/latest/src/developer_guide/compiling.html) for instructions.

### Requirements

- `Python >= 3.8`
- (optional) For computation on the GPU: `Nvidia driver >= 535`
- (optional) For vectorized / parallel computation on the CPU: `LLVM >= 11.1`


<!--  Add once published: -->
<!--
When using misuka in academic projects, please cite:

```bibtex
@inproceedings{misuka,
  title = {Misuka: {{An}} Open-Source Differentiable Room Acoustic Renderer},
  booktitle = {Proceedings of {{Meetings}} on {{Acoustics}}},
  author = {J{\"u}terbock, Tobias and Finnendahl, Ugo and Worchel, Markus and Wujecki, Daniel and Alexa, Marc and Weinzierl, Stefan},
  year = {2025},
  publisher = {Acoustical Society of America},
  address = {New Orleans, USA}
}
```
 -->
