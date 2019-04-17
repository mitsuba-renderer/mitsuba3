import os

import numpy as np

import enoki
from enoki import (UInt32D, FloatD, Vector3fD, cuda_set_log_level,
                   gather, forward, backward, gradient, reattach, hsum, sqr)
import mitsuba
from mitsuba import set_thread_count
from mitsuba.core import Log, EInfo, EDebug, Thread, Bitmap, float_dtype
from mitsuba.core.xml import load_file
from mitsuba.render import Spiral
from mitsuba.render.autodiff import (get_differentiable_parameters, render,
                                     SGD, Adam)
from mitsuba.render.autodiff_utils import (
    stringify_params, float_d_to_bitmap, l2 as l2_loss, compute_size, linearized_block_indices)
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
        assert pixel_format is not None
        bitmap = film.bitmap()
        if pixel_format != 'xyz':
            bitmap = bitmap.convert(Bitmap.EY if pixel_format == 'y' else Bitmap.ERGB,
                                    Struct.EFloat32, False)
        bitmap.write(write_to)
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
                   log_level=0, compute_forward=False, pixel_format='rgb',
                   optimizer=None, loss_function=None, callback=None,
                   block_size=None, image_spp=256, gradient_spp=32,
                   verbose=False):
    """Optimizes a scene's parameters with respect to a reference image.

    Inputs:
        optimized_params: List of parameter names to optimize over.
        image_ref:        Reference image, given as a list of FloatD (one per channel).
                          The channel count should match the give `pixel_format`.
                          If there are multiple sensors in the scene, then it
                          should be a list of reference images (one per sensor).
        max_iterations:   Number of iterations for the optimization algorithm.
        compute_forward:  Whether to compute the forward gradient images for the
                          optimized parameters.
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
    channel_count = 1 if pixel_format == 'y' else 3
    sensor_count = len(scene.sensors())
    reference_images = [image_ref_] if sensor_count == 1 else image_ref_
    check_valid_sensors_and_refs(scene, reference_images, channel_count)

    # Setup block-based rendering
    film = scene.film()
    if not block_size and film.crop_size()[0] != film.crop_size()[1]:
        raise ValueError("Not supported yet: non block-based rendering and non-square image.")
    block_size = block_size or film.crop_size()[0]
    spiral = Spiral(film, block_size)
    block_count = spiral.block_count()

    def render_block(i, block_i, image_ref):
        """
        Handles rendering of one block (color & gradient passes).
        Accumulates gradients and returns (color values, loss for this block).
        """
        crop_size = film.crop_size()
        pixel_count = crop_size[0] * crop_size[1]

        seed = i * block_count + block_i
        seed2 = max_iterations * (block_count + 1) + seed

        if image_spp is not None:
            # Forward pass: high-quality image reconstruction, no gradients
            with opt.no_gradients():
                scene.sampler().seed(seed, pixel_count * image_spp)
                image = render(scene, pixel_format=pixel_format, spp=image_spp)

            # Forward pass: low-spp gradient estimate
            scene.sampler().seed(seed2, pixel_count * gradient_spp)
            image_with_gradients = render(scene, pixel_format=pixel_format, spp=gradient_spp)
        else:
            image_with_gradients = None
            scene.sampler().seed(seed2, pixel_count * gradient_spp)
            image = render(scene, pixel_format=pixel_format, spp=gradient_spp)

        # Optional: render a gradient image for a given parameter
        if compute_forward:
            for k in optimized_params:
                Log(EDebug, '[ ] Computing forward image for parameter {}'.format(k))
                forward(params[k], free_graph=False)
                grad_bitmap = float_d_to_bitmap(gradient(image),
                                                shape=(crop_size[1], crop_size[0], -1))
                forward_fname = 'forward_{:03d}.exr'.format(i)
                grad_bitmap.write(forward_fname)
                Log(EDebug, '[+] Forward image saved to {}'.format(forward_fname))

        # Reattach gradients
        if image_with_gradients is not None:
            if isinstance(image, (list, tuple)):
                for c1, c2 in zip(image, image_with_gradients):
                    reattach(c1, c2)
            else:
                reattach(image, image_with_gradients)

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
        loss = loss_function(image, block_ref)

        Log(EDebug, '[ ] Backpropagating gradients from the loss')
        backward(loss)

        opt.accumulate_gradients()
        del image_with_gradients, block_indices
        return image, loss.numpy().squeeze()

    # ----------------------------------------------------- Main iterations
    for i in range(max_iterations):
        image_np = [
            np.zeros(shape=(film.size()[1], film.size()[0], channel_count), dtype=float_dtype)
            for _ in range(sensor_count)
        ]
        loss = 0.

        # For each sensor and block
        for sensor_i in range(sensor_count):
            scene.set_current_sensor(sensor_i)
            spiral.reset()
            block_i = 0
            while True:
                (offset, size) = spiral.next_block()
                if size[0] == 0:
                    break
                scene.sensor().set_crop_window(size, offset)

                image_block, block_loss = render_block(i, block_i, reference_images[sensor_i])
                loss += block_loss

                # Add block to full image
                block_np = image_block.numpy().reshape((size[1], size[0], -1))
                image_np[sensor_i][offset[1]:offset[1] + size[1],
                                   offset[0]:offset[0] + size[0], :] = block_np

                del image_block
                block_i += 1

        # Rendered all sensors and blocks, apply step
        if verbose:
            gradient_values = stringify_params(params, cb=gradient)
        Log(EDebug, '[+] Backpropagation done')
        opt.step()
        if callback:
            callback(i, params, optimized_params, opt, max_iterations,
                     loss, image_np, image_ref_)

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
            Bitmap(image_np[sensor_i]).write(
                'out_{:03d}_{:d}.exr'.format(i, sensor_i)
                if sensor_count > 1
                else 'out_{:03d}.exr'.format(i)
            )
        if verbose:
            print('\n')

    return params, loss, image_np


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(prog='differentiable.py')
    parser.add_argument('scene', type=str, help='Path to the scene file')
    parser.add_argument('--list', action='store_true',
                        help='List differentiable parameters in the scene and exit')
    parser.add_argument('--forward', action='store_true',
                        help='Also compute a forward gradients image')
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

    differentiable_render(loaded_scene, optimized_params=args.optimized_param, image_ref=ref,
                          max_iterations=args.max_iterations,
                          log_level=args.log_level, compute_forward=args.forward)
