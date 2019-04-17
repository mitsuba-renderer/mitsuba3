import numpy as np

from mitsuba.core import Bitmap
from mitsuba.render import ImageBlock

import enoki
from enoki import (UInt32D, FloatC, FloatD, Vector2fC, Vector3fC, Vector4fC,
                   Vector2fD, Vector3fD, Vector4fD, Matrix4fC, Matrix4fD,
                   set_requires_gradient, detach, gather, hsum, eq, select, rcp)

# ------------------------------------------------------------ Optimization callbacks


def upsample_data(side, dimensions, values_):
    """
    Upsamples the linearized N-dimensional numpy array `values_` assuming it has
    size side^N. The data is trilinearly interpolated.
    """
    from scipy.interpolate import interpn
    values = values_.copy().reshape([side] * dimensions)

    t = np.linspace(0, 1, side)
    side2 = 2 * side
    if dimensions == 1:
        grid = np.mgrid[:side2] / (side2 - 1)
    elif dimensions == 2:
        grid = np.mgrid[:side2, :side2] / (side2 - 1)
    elif dimensions == 3:
        grid = np.mgrid[:side2, :side2, :side2] / (side2 - 1)
    else:
        raise ValueError(
            'upsample_data: unsupported dimensionality {} (expected 1, 2 or 3)'.format(dimensions))

    grid = grid.reshape((dimensions, -1)).T
    interpolated = interpn([t] * dimensions, values, grid, method='linear')

    # if dimensions == 3:
    #     # Sanity check: corners should match
    #     interpolated2 = interpolated.reshape((side2, side2, side2))
    #     sub = interpolated2[::side2 - 1, ::side2 - 1, ::side2 - 1]
    #     sub_ref = values[::side - 1, ::side - 1, ::side - 1]
    #     assert np.allclose(sub, sub_ref), "\n{}\nvs\n{}".format(sub, sub_ref)
    return interpolated


def multigrid_callback(levels, dimensions, stride=15, schedule=1):
    """
    Generates a callback function which will double the data resolution every
    `stride` iteration until maximum level `levels`.
    For example, setting `levels = 3` means that data will be upsampled 3 times.
    """
    # Prepare a list of iterations when upscaling should be triggered
    if schedule == 1:
        thresholds = [i * stride for i in range(1, levels + 1)]
    else:
        thresholds = [stride]
        current_stride = stride
        for _ in range(1, levels):
            thresholds.append(thresholds[-1] + int(current_stride))
            current_stride *= schedule
    print('[+] Upsampling schedule:', thresholds)

    def callback(i, params, optimized_params, *_):
        if i not in thresholds:
            return
        level = thresholds.index(i) + 1

        assert len(optimized_params) == 1
        key = optimized_params[0]

        size = len(params[key])
        if dimensions == 1:
            side = size
        elif dimensions == 2:
            side = int(np.sqrt(size))
        elif dimensions == 3:
            side = int(np.cbrt(size))
        else:
            raise ValueError('multigrid_callback: unsupported dimensionality {}'.format(dimensions))
        assert size == side ** dimensions

        upsampled = upsample_data(side, dimensions, params[key].numpy())
        params[key] = FloatD(upsampled)

        set_requires_gradient(params[key])
        extra = ('' if level >= levels
                 else '. Next upsample: iteration {}.'.format(thresholds[level]))
        print('[{:03d}]: Upsampled data {:>3d} -> {:>3d} (level {} / {}){}'.format(
            i, side, 2 * side, level, levels, extra))

    return callback


def learning_rate_callback(high, low, linear=False, midpoint=0.5):
    """
    Creates a callback function which updates the learning rate at each
    step accordining to a decay schedule.

    Inputs:
        high:     Learning rate value at iteration 0.
        low:      Learning rate value at the last iteration.
        linear:   If false, use an exponential schedule.
        midpoint: For the exponential schedule, tweak the decay rate such
                  that the learning rate value: `low + (high-low)/2`
                  is reached at iteration `midpoint * max_iterations`.
                  Unused for linear schedules
    """
    assert low > 0.
    if not linear:
        assert midpoint > 0. and midpoint <= 1.
        decay_rate = np.exp(-np.log(2) / midpoint)

    def callback(iteration, params, _, opt, max_iterations, *args):
        t = iteration / float(max_iterations)
        if linear:
            lr = (low - high) * t + high
        else:
            lr = (high - low) * (decay_rate ** t) + low

        # Ensure that the JIT does not consider the learning rate as a literal
        # TODO: use literal=False
        opt.lr = FloatC(lr) * 1

    return callback


def clamp_parameters(low, high=np.inf):
    """Clamps all parameters to the range [low, high]."""
    from enoki import clamp

    def callback(i, params, *_):
        for k, p in params.items():
            params[k] = enoki.clamp(detach(p), low, high)
            set_requires_gradient(params[k])
    return callback


def combine_callbacks(*callbacks):
    def combined(*args, **kwargs):
        for c in callbacks:
            c(*args, **kwargs)
    return combined

# ------------------------------------------------------------ Data helpers


def compute_size(data):
    """Returns the length of the innermost array in `data`."""
    t = type(data)
    if t is FloatD or t is FloatC:
        return len(data)
    if (t is Vector2fD or t is Vector3fD or t is Vector4fD) or \
       (t is Vector2fC or t is Vector3fC or t is Vector4fC):
        return len(data[0])
    if t is Matrix4fD or t is Matrix4fC:
        return len(data[0, 0])
    raise RuntimeError("Optimizer: invalid parameter type " + str(t))


def image_to_float_d(image, pixel_format, crop_border=True):
    W = None
    if isinstance(image, ImageBlock):
        channels = image.bitmap_d()
        border = image.border_size()
        if border > 0 and crop_border:
            # Crop out the border pixels (references typically don't include them)
            small_size = image.size()
            full_size = small_size + 2 * border
            coords = np.mgrid[border:border + small_size[1],
                              border:border + small_size[0]].astype(int)
            # Linearize (looks unusual because of the numpy <-> Enoki layout mismatch)
            indices = UInt32D(coords[0, ...].ravel() * full_size[0]
                              + coords[1, ...].ravel())
            for i, c in enumerate(channels):
                channels[i] = gather(c, indices)

        X, Y, Z, A, W = channels
        del A
    elif isinstance(image, Bitmap):
        channels = []
        arr = np.array(image, copy=False)
        for i in range(image.channel_count()):
            channels.append(FloatD(arr[:, :, i].flatten()))
        if len(channels) == 1:
            assert pixel_format == 'y'
            return channels[0]
        elif len(channels) == 3:
            assert pixel_format == 'rgb'
            return Vector3fD(channels[0], channels[1], channels[2])
        else:
            assert len(channels) == 5
            X, Y, Z, _, W = channels
    else:
        raise RuntimeError('Cannot convert image of type: {}'.format(type(image)))

    if W is not None:
        W = select(eq(W, 0), 0, rcp(W))
        X *= W
        Y *= W
        Z *= W

    if pixel_format == 'y':
        return Y
    elif pixel_format == 'xyz':
        return Vector3fD(X, Y, Z)
    elif pixel_format == 'rgb':
        return Vector3fD(
            3.240479 * X + -1.537150 * Y + -0.498535 * Z,
            -0.969256 * X + 1.875991 * Y + 0.041556 * Z,
            0.055648 * X + -0.204043 * Y + 1.057311 * Z
        )


def float_d_to_bitmap(values, shape):
    return Bitmap(values.numpy().reshape(*shape))


def linearized_block_indices(offset, crop_size, full_size):
    """Returns the linearized indices of pixels within a crop window."""
    # TODO: test this more!
    XY = np.mgrid[offset[1]:offset[1] + crop_size[1], offset[0]:offset[0] + crop_size[0]]
    X, Y = XY[0, ...].ravel(), XY[1, ...].ravel()
    linearized = X * full_size[0] + Y
    return linearized

# ------------------------------------------------------------ Losses


def l1(y, ref):
    return hsum(
        hsum(enoki.abs(y - ref))
    ) / compute_size(y)


def l2(y, ref):
    return hsum(
        hsum(enoki.sqr(y - ref))
    ) / compute_size(y)


def mape(y, ref, offset=1e-3):
    return hsum(
        hsum(enoki.abs(y - ref) / (offset + enoki.abs(ref)))
    ) / compute_size(y)


def mrse(y, ref, offset=1e-3):
    return hsum(
        hsum(enoki.sqr(y - ref) / (offset + enoki.sqr(ref)))
    ) / compute_size(y)


# ------------------------------------------------------------ Formatting helpers
def indent(s, amount=2):
    return '\n'.join((' ' * amount if i > 0 else '') + l
                     for i, l in enumerate(s.splitlines()))


def stringify_params(params, cb=None):
    cb = cb or (lambda a: a)
    strings = [
        str(', '.join(str(vv) for vv in cb(v).numpy()))
        if len(v) <= 10
        else str(v)
        for _, v in params.items()
    ]
    return ', '.join(strings)
