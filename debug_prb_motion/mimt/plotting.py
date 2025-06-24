from collections.abc import Iterable
import drjit as dr
import mitsuba as mi
import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path
from typing import List, Optional

FIGURE_LINEWIDTH        = 237.13594/71.959
FIGURE_WIDTH_ONE_COLUMN = 2*FIGURE_LINEWIDTH
FIGURE_WIDTH_TWO_COLUMN = FIGURE_LINEWIDTH

def disable_ticks(ax):
    """ Disable ticks around plot (useful for displaying images) """
    ax.axes.get_xaxis().set_ticklabels([])
    ax.axes.get_yaxis().set_ticklabels([])
    ax.axes.get_xaxis().set_ticks([])
    ax.axes.get_yaxis().set_ticks([])
    return ax

def set_siggraph_font():
    font = {'family': 'sans-serif',
        'sans-serif': 'Linux Biolinum'}
    mpl.rc('font', **font)
    mpl.rc('text', **{'usetex': False})
    mpl.rc('mathtext', fontset='custom', rm='Linux Biolinum', it='Linux Biolinum:italic', bf='Linux Biolinum:bold')

def generate_figure(integrators: List[str], data: dict, output_path: Path, grad_projection: str='red', square_r_setting_3: bool = True, quantile: float = 0.89, labels: Optional[List[str]] = None, exclude_first_gradient: bool = False, with_colorbar: bool = False):
    grad_projection_fn = None
    if grad_projection == 'red':
        grad_projection_fn = lambda grad: grad[...,0]
    elif grad_projection == 'mean':
        grad_projection_fn = lambda grad: dr.mean(grad, axis=-1)
    elif grad_projection is None:
        grad_projection_fn = lambda grad: grad
    else:
        raise RuntimeError(f"Unknown gradient projection {grad_projection}")

    num_settings = len(list(data.keys()))

    n_rows = num_settings
    n_cols = 2 + len(integrators) + (-1 if exclude_first_gradient else 0)
    width_ratios = [1.]*n_cols

    colorbar_width_ratio = 0.075
    if with_colorbar:
        width_ratios += [colorbar_width_ratio]


    # Optional: the first integrator can be ignored in the figure.
    #           This is useful if the first integrator merely serves the
    #           purpose of generating the primal image and the FD reference
    #           but is otherwise not relevant for the comparison.
    def integrator_index_to_col(j: int):
        return j if not exclude_first_gradient else j-1

    aspect = (n_rows / (n_cols + (colorbar_width_ratio if with_colorbar else 0)))
    fig = plt.figure(1, figsize=(FIGURE_WIDTH_ONE_COLUMN, aspect * FIGURE_WIDTH_ONE_COLUMN), constrained_layout=False)
    gs  = fig.add_gridspec(n_rows, n_cols + int(with_colorbar), wspace=0.05, hspace=0.05, width_ratios=width_ratios)
    r = None
    for i, setting in enumerate(data.keys()):
        setting_data = data[setting]
        q = quantile[i] if isinstance(quantile, Iterable) else quantile
        for j, integrator in enumerate(integrators):
            assert setting_data[j][0] == integrator
            if j == 0:
                # Show primal image
                img = setting_data[j][1]
                ax_img = disable_ticks(fig.add_subplot(gs[i, j]))
                ax_img.imshow(mi.Bitmap(img).convert(srgb_gamma=True))
                
                # Show FD of reference primal image
                grad_fd = grad_projection_fn(setting_data[j][2])
                ax_fd = disable_ticks(fig.add_subplot(gs[i, j + 1]))
                if grad_projection is None:
                    im_fd = ax_fd.imshow(mi.Bitmap(grad_fd).convert(srgb_gamma=True))
                else:
                    # init range
                    r = np.quantile(np.abs(grad_fd), q)
                    # last setting gets a different range
                    if square_r_setting_3 and (i == 2):
                        r = np.quantile(np.abs(grad_fd), q)*2
                    #r = np.maximum(r, 1)
                    im_fd = ax_fd.imshow(grad_fd, cmap='coolwarm', vmin=-r, vmax=r)

                if i == num_settings - 1:
                    ax_img.set_xlabel("Image", fontsize=12)
                    ax_fd.set_xlabel("FD", fontsize=12)
                
                ax_img.set_ylabel(setting, fontsize=12)

            if (j == 0) and exclude_first_gradient:
                continue

            # Show forward-mode gradient
            col = integrator_index_to_col(j)
            grad_fw = grad_projection_fn(setting_data[j][3])
            ax_fw = disable_ticks(fig.add_subplot(gs[i, col + 2]))      
            if grad_projection is None:
                ax_fw.imshow(mi.Bitmap(grad_fw).convert(srgb_gamma=True))
            else:
                ax_fw.imshow(grad_fw, cmap='coolwarm', vmin=-r, vmax=r)

            if i == num_settings - 1:
                integrator_label = labels[col] if labels is not None else f"{integrator}"
                ax_fw.set_xlabel(integrator_label, fontsize=12)

    # Last column is the color bar
    if with_colorbar:
        cax = fig.add_subplot(gs[(n_rows+1)//2-1, -1])
        cbar = fig.colorbar(im_fd, cax=cax)
        cbar.set_ticks([-r, 0, +r])
        cbar.set_ticklabels(['-', 0, '+'], fontsize=12)

    plt.show()

    output_dir = output_path.parent
    output_dir.mkdir(exist_ok=True, parents=True)
    fig.savefig(output_path, facecolor='white', bbox_inches='tight', dpi=300)