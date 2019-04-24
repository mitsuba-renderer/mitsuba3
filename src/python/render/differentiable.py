import os

import numpy as np

import enoki
from enoki import (UInt32D, FloatD, Vector3fD, cuda_set_log_level,
                   gather, forward, backward, gradient, reattach, hsum, sqr, requires_gradient)
import mitsuba
from mitsuba import set_thread_count
from mitsuba.core import Log, EInfo, EDebug, Thread, Bitmap, Struct, float_dtype
from mitsuba.core.xml import load_file
from mitsuba.render import Spiral, ImageBlock
from mitsuba.render.autodiff import (get_differentiable_parameters, render,
                                     SGD, Adam)
from mitsuba.render.autodiff_utils import (stringify_params, float_d_to_bitmap, l2 as l2_loss,
                                           compute_size, linearized_block_indices, image_to_float_d)
from mitsuba.test.util import fresolver_append_path


DEFAULT_OPTIMIZED_PARAMS = ['/Scene/BSDF[id="box"]/ContinuousSpectrum/value']


@fresolver_append_path
def load_scene(filename, *args, **kwargs):
    """Prepares the file resolver and loads a Mitsuba scene from the given path."""
    fr = Thread.thread().file_resolver()
    here = os.path.dirname(__file__)
    fr.append(here)
    fr.append(os.path.join(here, filename))
    fr.append(os.path.dirname(filename))

    scene = load_file(filename, *args, **kwargs)
    assert scene is not None
    return scene


def simple_render(scene, vectorize=False, write_to=None, pixel_format=None):
    """Performs a simple CPU render of the given scene, for comparison with GPU renders."""
    integrator = scene.integrator()
    integrator.render(scene, vectorize=vectorize)
    film = scene.sensor().film()
    if write_to:
        from mitsuba.core import Struct
        assert isinstance(pixel_format, Bitmap.EPixelFormat)
        film.bitmap().convert(pixel_format, Struct.EFloat32, False).write(write_to)
    return film


def check_valid_sensors_and_refs(scene, reference_images, channel_count):
    """Ensures that all sensors in the scene have the same film size (for simplicity).
    Also verifies that the right number of reference images were provided."""
    sensors = scene.sensors()
    film_size = sensors[0].film().size()
    assert len(reference_images) == len(sensors)

    # Multiple sensors
    for i, im in enumerate(reference_images):
        assert (len(im) == np.product(film_size)
                if channel_count == 1 else len(im) == channel_count)
        assert np.all(film_size == sensors[i].film().size())


def optimize_scene(scene, optimized_params, image_ref_, max_iterations=8,
                   log_level=0, pixel_format='rgb',
                   optimizer=None, loss_function=None, callback=None,
                   block_size=None, pass_count=1, image_spp=256, gradient_spp=32,
                   verbose=False):
    """Optimizes a scene's parameters with respect to a reference image.

    Inputs:
        optimized_params: List of parameter names to optimize over.
        image_ref:        Reference image, given as a list of FloatD (one per channel).
                          The channel count should match the give `pixel_format`.
                          If there are multiple sensors in the scene, then it
                          should be a list of reference images (one per sensor).
        max_iterations:   Number of iterations for the optimization algorithm.
        optimizer:        A function or constructor used to build the optimizer from
                          a DifferentiableParameters object. A reasonable default is
                          created if None.
        loss_function:    The loss function `loss(image, image_ref)`. Should be implemented
                          in terms of Enoki types, since the parameters are FloatD.
                          The L2 loss is used by default if None.
        callback:         Called at the end of each iteration:
                              callback(iteration, params, optimized_params, opt,
                                       max_iterations, loss, image, image_ref)
                          Can be used to e.g. post-process the parameters.
        block_size:       If not None, render the image block by block and accumulate the gradients
                          and loss. This enables usage of more samples per pixel, but is only valid
                          if the loss is defined as a pixelwise sum.
                          By default, renders the whole image as one block.
        pass_count:       Number of image passes. Can be used alternatively or in
                          conjunction with `block_size` to split the amount of work
                          done in one render pass, e.g. due to memory constraints.
                          The given samples per pixels are further multiplied by
                          the pass count.
        image_spp:        Number of samples per pixel to use to compute the image (color values),
                          with gradient computation turned off. This can typically be higher than
                          the `gradient_spp` value as memory requirement is lower.
                          If None, the color & gradient images will be the same (single pass).
        gradient_spp:     Number of samples per pixel to use to estimate gradients.
        verbose:          Enables extra prints and sets logging level to Debug.
    """

    cuda_set_log_level(log_level)
    set_thread_count(1)

    if verbose:
        Thread.thread().logger().set_log_level(EDebug)

    params = get_differentiable_parameters(scene)

    # Discard all differentiable parameters except the selected ones
    params.keep(optimized_params)
    # Instantiate an optimizer
    opt = optimizer(params) if optimizer else SGD(params, lr=0.5, momentum=0.5)
    # Loss function
    loss_function = loss_function or l2_loss

    # Setup multi-sensor rendering
    channel_count = 1 if pixel_format == Bitmap.EY else 3
    sensor_count = len(scene.sensors())
    reference_images = [image_ref_] if sensor_count == 1 else image_ref_
    check_valid_sensors_and_refs(scene, reference_images, channel_count)

    # Setup block-based rendering
    film = scene.film()
    if not block_size and film.crop_size()[0] != film.crop_size()[1]:
        raise ValueError("Not supported yet: non block-based rendering and non-square image.")
    block_size = block_size or film.crop_size()[0]
    spiral = Spiral(film, block_size, passes=pass_count)
    block_count = spiral.block_count() * pass_count
    found_gradients = False

    def render_block(i, block_i, image_ref):
        """
        Handles rendering of one block (color & gradient passes).
        Accumulates gradients and returns (color values, loss for this block).
        Note that the loss is multiplied by the pixel count in this block to
        allow for proper re-normalization.
        """
        nonlocal found_gradients
        crop_size = film.crop_size()
        pixel_count = crop_size[0] * crop_size[1]

        seed = i * block_count + block_i
        seed2 = max_iterations * (block_count + 1) + seed

        if image_spp is not None:
            # Forward pass: high-quality image reconstruction, no gradients
            with opt.no_gradients():
                scene.sampler().seed(seed, pixel_count * image_spp)
                block = render(scene, pixel_format=pixel_format, spp=image_spp)

            # Forward pass: low-spp gradient estimate
            scene.sampler().seed(seed2, pixel_count * gradient_spp)
            block_with_gradients = render(scene, pixel_format=pixel_format, spp=gradient_spp)
        else:
            block_with_gradients = None
            scene.sampler().seed(seed2, pixel_count * gradient_spp)
            block = render(scene, pixel_format=pixel_format, spp=gradient_spp)

        # Cropped block values (differentiable)
        block_values = image_to_float_d(block, pixel_format, crop_border=True)

        # Reattach gradients if needed
        if block_with_gradients is not None:
            block_gradient_values = image_to_float_d(
                block_with_gradients, pixel_format, crop_border=True)
            reattach(block_values, block_gradient_values)

        # Extract part of the reference image corresponding to this block
        linearized = linearized_block_indices(
            film.crop_offset(), crop_size, film.size())
        assert len(linearized) == pixel_count
        block_indices = UInt32D(linearized)
        if channel_count == 1:
            block_ref = gather(image_ref, block_indices)
        else:
            block_ref = Vector3fD(
                gather(image_ref[0], block_indices),
                gather(image_ref[1], block_indices),
                gather(image_ref[2], block_indices),
            )

        # Compute loss and backpropagate
        # TODO: the cropped block is still not exactly right to compute the loss,
        # since border pixels will be too dark compared to the reference
        # ('spread' of energy by the reconstruction filter). We'd need 'high
        # quality edges' (enlarging the block and discarding more) to get correct
        # values at the border.
        loss = loss_function(block_values, block_ref)

        Log(EDebug, '[ ] Backpropagating gradients from the loss')
        # Note that some blocks may not have gradients w.r.t. the parameters of interest
        block_weight = np.product(crop_size)
        if requires_gradient(loss):
            backward(loss)
            opt.accumulate_gradients(weight=block_weight)
            found_gradients = True

        del block_indices, block_values, block_with_gradients
        return block, block_weight * loss[0]

    # ---------------------------
    rfilter = film.reconstruction_filter()
    bsize = rfilter.border_size()

    # Image reconstructed from blocks, including border
    reconstructed_images = [
        ImageBlock(Bitmap.EXYZAW, film.crop_size(), warn=False,
                   filter=rfilter, border=(bsize > 0))
        for _ in range(sensor_count)
    ]

    # ----------------------------------------------------- Main iterations
    for i in range(max_iterations):
        for im in reconstructed_images:
            im.clear()
            im.clear_d()
        loss = 0.

        # For each sensor and block (including potentially multiple passes)
        for sensor_i in range(sensor_count):
            scene.set_current_sensor(sensor_i)
            spiral.reset()
            spiral.set_passes(pass_count)
            block_i = 0
            loss_i = 0.
            while True:
                (offset, size) = spiral.next_block()
                if size[0] == 0:
                    break
                scene.sensor().set_crop_window(size, offset)

                image_block, block_loss = render_block(i, block_i, reference_images[sensor_i])
                loss_i += block_loss

                # Add block to full image
                image_block.set_offset(offset)
                assert np.all(image_block.offset() == offset) and np.all(
                    image_block.size() == size)

                # Accumulate block
                reconstructed_images[sensor_i].put(image_block)

                del image_block
                block_i += 1
            loss += loss_i / (pass_count * np.product(scene.sensor().film().size()))

        if not found_gradients:
            raise RuntimeError('Found no gradients associated to optimized variables '
                               'in any of the {} blocks.\nparams = {}'.format(block_i, params))

        # Convert reconstructed images and crop out border
        images_np = []
        for im in reconstructed_images:
            b = im.bitmap().convert(pixel_format, Struct.EFloat32, False)
            image_np = np.array(b, copy=True)
            if bsize > 0:
                image_np = image_np[bsize:-bsize, bsize:-bsize, :]
            images_np.append(image_np)

        # Rendered all sensors and blocks, apply step
        if verbose:
            gradient_values = stringify_params(params, cb=gradient)
        Log(EDebug, '[+] Backpropagation done')
        opt.step()
        if callback:
            callback(i, params, optimized_params, opt, max_iterations,
                     loss, images_np, image_ref_)

        # Print iteration info and save current image
        if verbose:
            param_values = stringify_params(params)
            Log(EInfo,
                "[{:03d}]: loss = {:.6f},\n"
                "        parameter values = {},\n"
                "        gradients = {}".format(
                    i, loss, param_values, gradient_values))
        else:
            print("[{:03d}]: loss = {:.6f}      ".format(i, loss), end='\r')

        for sensor_i in range(sensor_count):
            Bitmap(images_np[sensor_i]).write(
                'out_{:03d}_{:d}.exr'.format(i, sensor_i)
                if sensor_count > 1
                else 'out_{:03d}.exr'.format(i)
            )
        if verbose:
            print('\n')

    return params, loss, images_np


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(prog='differentiable.py')
    parser.add_argument('scene', type=str, help='Path to the scene file')
    parser.add_argument('--list', action='store_true',
                        help='List differentiable parameters in the scene and exit')
    parser.add_argument('--log_level', default=0, type=int,
                        help='Verbosity level for the autodiff backend')
    parser.add_argument('--max_iterations', default=8, type=int,
                        help='Maximum number of optimization iterations')
    parser.add_argument('--optimized_param', type=str,
                        nargs='*', default=DEFAULT_OPTIMIZED_PARAMS,
                        help='Optimized parameters (may be specified multiple times)')
    args = parser.parse_args()

    loaded_scene = load_scene(args.scene)

    if args.list:
        diff_params = get_differentiable_parameters(loaded_scene)
        print(diff_params)
        exit(0)

    ref = FloatD.full(0., np.product(loaded_scene.film().size()))

    optimize_scene(loaded_scene, optimized_params=args.optimized_param, image_ref=ref,
                   max_iterations=args.max_iterations, log_level=args.log_level)
