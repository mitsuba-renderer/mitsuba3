<p align="center">
  <img src="docs/images/misuka_logo.png" width="300" alt="misuka logo">
</p>

# misuka

|  Documentation  |       PyPI      |
|      :---:      |      :---:      |
| [![docs][1]][2] | [![pypi][3]][4] |

[1]: https://readthedocs.org/projects/misuka/badge/?version=latest
[2]: https://misuka.readthedocs.io/en/latest/
[3]: https://img.shields.io/pypi/v/misuka.svg?color=orange
[4]: https://pypi.org/project/misuka/
[5]: https://dl.acm.org/doi/pdf/10.1145/3730900

## Introduction

misuka is a research-focused room acoustic renderer for forward and inverse sound transport simulation, developed in collaboration between the [Audio Communication Group](https://www.tu.berlin/en/ak) and the [Computer Graphics Group](https://www.cg.tu-berlin.de/) at [TU Berlin](https://www.tu.berlin/).

It is a fully compatible extension to [Mitsuba 3](https://github.com/mitsuba-renderer/mitsuba3), adding plugins for acoustic simulation.
The renderer is described in [misuka: An Open-Source Differentiable Room Acoustic Renderer](https://doi.org/10.1121/2.0002193).


## Main Features

- **Differentiation**: misuka is a differentiable renderer, meaning that it can compute derivatives of the entire simulation with respect to input parameters such as material properties, emitter and receiver positions, and scene geometry. It implements [Time-Resolved Path Replay Backpropagation][5] for efficient gradient estimation.

- **Cross-platform**: misuka has been tested on Linux (``x86_64``), macOS (``arm64``), and Windows (``x86_64``).

- **High performance**: The underlying Dr.Jit compiler fuses rendering code into kernels that achieve state-of-the-art performance using an LLVM backend targeting the CPU, a CUDA/OptiX backend targeting NVIDIA GPUs, and a Metal backend targeting Apple Silicon GPUs, all with hardware-accelerated ray tracing.

- **Python first**: misuka is deeply integrated with Python. Materials, textures, and even full rendering algorithms can be developed in Python, which the system JIT-compiles (and optionally differentiates) on the fly. This enables the experimentation needed for research.

## Disclaimer

misuka is currently under heavy development. Additional features will be added in the near future. The user interface might change.

## Tutorials, documentation

You can find tutorials for forward rendering and gradient-based optimization in the folder `tutorials_acoustic`. More tutorials and a full documentation will be added successively.

## Installation

We provide pre-compiled binary wheels via PyPI. Installing misuka this way is as simple as running

```
pip install misuka
```

on the command line. The Python package includes the following variants by default:

- `scalar_rgb`
- `cuda_ad_acoustic`
- `metal_ad_acoustic`
- `llvm_ad_acoustic`

Additional variants can be enabled by compiling misuka.
Please refer to the [Developer's Guide](https://misuka.readthedocs.io/en/latest/src/developer_guide/compiling.html) for instructions.

### Requirements

- `Python >= 3.9`
- (optional) For computation on the GPU: `Nvidia driver >= 535`
- (optional) For vectorized / parallel computation on the CPU: `LLVM >= 15`
- (optional) For computation on Apple Silicon GPUs: macOS 15 or newer with a Metal-capable GPU

The optional ones are loaded from your system at runtime rather than shipped with misuka.
See [Runtime requirements](https://misuka.readthedocs.io/en/latest/src/runtime_requirements.html) for how to install them and, where needed, how to point misuka at them.

## License

misuka is licensed under the [PolyForm Noncommercial License 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0), which permits academic and private use.
Files inherited from Mitsuba 3 remain under the original BSD-3-Clause license.
See [LICENSE](LICENSE) for the file-level rules.

If you are interested in using misuka commercially, please contact a.jueterbock@tu-berlin.de.


## Citation

When using misuka in academic projects, please cite:

```bibtex
@article{misuka,
  title = {{{misuka}}: {{An}} Open-Source Differentiable Room Acoustic Renderer},
  shorttitle = {Misuka},
  author = {J{\"u}terbock, Tobias and Finnendahl, Ugo and Worchel, Markus and Wujecki, Daniel and Alexa, Marc and Weinzierl, Stefan},
  year = 2026,
  month = jan,
  journal = {Proceedings of Meetings on Acoustics},
  volume = {58},
  number = {1},
  pages = {022004:1--022004:13},
  publisher = {Acoustical Society of America},
  doi = {10.1121/2.0002193}
  issn = {1939-800X},
}
```

If your work uses Time-Resolved Path Replay Backpropagation, please also cite:

```bibtex
@article{acoustic_prb,
  title = {Differentiable Geometric Acoustic Path Tracing Using Time-Resolved Path Replay Backpropagation},
  author = {Finnendahl, Ugo and Worchel, Markus and J{\"u}terbock, Tobias and Wujecki, Daniel and Brinkmann, Fabian and Weinzierl, Stefan and Alexa, Marc},
  year = 2025,
  month = jul,
  issue_date = {August 2025}
  journal = {ACM Transactions on Graphics},
  volume = {44},
  number = {4},
  pages = {82:1--82:17},
  publisher = {Association for Computing Machinery},
  address = {New York, NY, USA},
  issn = {0730-0301},
  doi = {10.1145/3730900},
}
```