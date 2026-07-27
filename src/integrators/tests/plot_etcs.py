# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Plot acoustic energy-time curves (ETCs).

The acoustic integrator tests (``test_acoustic_path.py``,
``test_acoustic_ad_integrators.py``) dump the rendered ETC (``*primal.exr``)
and a copy of the reference ETC (``*ref.exr``) next to this script whenever a
primal test fails.
Run directly::

    python plot_etcs.py
"""

from pathlib import Path

import misuka as mi
mi.set_variant('cuda_acoustic', 'llvm_acoustic', 'scalar_acoustic')

import numpy as np
import pyfar as pf
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

# Sampling rate must match the config's sampling_rate used to render the ETCs.
SAMPLING_RATE = 1000.0

tests_dir = Path(__file__).resolve().parent


def load_etc(path):
    """Load an ETC .exr as a pyfar Signal.

    The tape ETC has frequency bins along the first axis and time bins along the
    second. Squeeze a possible trailing channel axis, then transpose so each
    frequency band becomes a pyfar channel (time is the last axis).
    """
    etc = np.squeeze(np.array(mi.TensorXf(mi.Bitmap(str(path)))))
    return pf.Signal(etc.T, SAMPLING_RATE)


def reference_for(primal_path):
    """Return the reference .exr matching a rendered ``*primal.exr``, or None."""
    ref = primal_path.with_name(primal_path.name.replace('primal.exr', 'ref.exr'))
    return ref if ref.exists() else None


def plot_pair(primal_path, ref_path):
    primal = load_etc(primal_path)
    n_freq = primal.cshape[0]
    ref = load_etc(ref_path) if ref_path is not None else None

    fig, ax = plt.subplots(1, 1, figsize=(10, 6), layout='constrained')

    # One color per frequency bin; solid = rendered (test), dashed = reference.
    for i in range(n_freq):
        pf.plot.time(primal[i], dB=True, ax=ax, color=f'C{i}',
                     linestyle='-', label='_nolegend_')
    if ref is not None:
        for i in range(min(n_freq, ref.cshape[0])):
            pf.plot.time(ref[i], dB=True, ax=ax, color=f'C{i}',
                         linestyle='--', label='_nolegend_')

    # Title: the pair name without the trailing "primal" marker.
    name = primal_path.stem
    for suffix in ('_primal', '-primal'):
        if name.endswith(suffix):
            name = name[: -len(suffix)]
            break
    ax.set_title(name)

    # Legend 1: frequency bin -> color.
    freq_handles = [Line2D([], [], color=f'C{i}', linestyle='-', label=str(i))
                    for i in range(n_freq)]
    freq_legend = ax.legend(handles=freq_handles, title='Frequency bin',
                            loc='upper right')
    ax.add_artist(freq_legend)

    # Legend 2: line style -> test vs reference.
    style_handles = [
        Line2D([], [], color='black', linestyle='-', label='test'),
        Line2D([], [], color='black', linestyle='--', label='reference'),
    ]
    ax.legend(handles=style_handles, loc='lower left')

    out_png = tests_dir / f'{name}.png'
    fig.savefig(out_png, dpi=150)
    print(f'Saved plot to {out_png}')


def main():
    primals = sorted(tests_dir.glob('*primal.exr'))
    if not primals:
        print(f'No *primal.exr files found in {tests_dir}')
        return

    for primal_path in primals:
        plot_pair(primal_path, reference_for(primal_path))

    plt.show()


if __name__ == '__main__':
    main()
