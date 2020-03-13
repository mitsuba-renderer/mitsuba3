.. _sec-pytorch:

Integration with PyTorch
========================

We briefly show how the example from the earlier section on
:ref:`differentiable rendering <sec-differentiable-rendering>` can be made to
work when combining differentiable rendering with an optimization expressed
using PyTorch. The ability to combine these frameworks enables sandwiching
Mitsuba 2 between neural layers and differentiating the combination end-to-end.

Note that communication and synchronization between Enoki and PyTorch along
with the complexity of traversing two separate computation graph data
structures causes an overhead of 10-20% compared to optimization implemented
using only Enoki. We generally recommend sticking with Enoki unless the problem
requires neural network building blocks like fully connected layers or
convolutions, where PyTorch provides a clear advantage.

As before, we specify the relevant variant, load the scene, and retain
relevant differentiable parameters.

.. code-block:: python

    import mitsuba
    mitsuba.set_variant('gpu_autodiff_rgb')

    import enoki as ek
    from mitsuba.core import Thread, Vector3f
    from mitsuba.core.xml import load_file
    from mitsuba.python.util import traverse
    from mitsuba.python.autodiff import render_torch, write_bitmap
    import torch
    import time

    Thread.thread().file_resolver().append('cbox')
    scene = load_file('cbox/cbox.xml')

    # Find differentiable scene parameters
    params = traverse(scene)

    # Discard all parameters except for one we want to differentiate
    params.keep(['red.reflectance.value'])

The ``.torch()`` method can be used to convert any Enoki CUDA type
into a corresponding PyTorch tensor.

.. code-block:: python

    # Print the current value and keep a backup copy
    param_ref = params['red.reflectance.value'].torch()
    print(param_ref)

The :py:func:`~mitsuba.python.autodiff.render_torch()` function works
analogously to :py:func:`~mitsuba.python.autodiff.render()` except that it
returns a PyTorch tensor.

.. code-block:: python

    # Render a reference image (no derivatives used yet)
    image_ref = render_torch(scene, spp=8)
    crop_size = scene.sensors()[0].film().crop_size()
    write_bitmap('out.png', image_ref, crop_size)

As before, we change one of the input parameters and initialize an optimizer.

.. code-block:: python

    # Change the left wall into a bright white surface
    params['red.reflectance.value'] = [.9, .9, .9]
    params.update()

    # Which parameters should be exposed to the PyTorch optimizer?
    params_torch = params.torch()

    # Construct a PyTorch Adam optimizer that will adjust 'params_torch'
    opt = torch.optim.Adam(params_torch.values(), lr=.2)
    objective = torch.nn.MSELoss()

Note that the scene parameters are effectively duplicated: we represent them
once using Enoki arrays (``params``), and once using PyTorch arrays
(``params_torch``). To perform a differentiable rendering, the function
:py:func:`~mitsuba.python.autodiff.render_torch()` requires that both are given
as arguments. Due to technical specifics of how PyTorch detects differentiable
parameters, it is furthermore necessary that ``params_torch`` is expanded into
a list of keyword arguments (``**params_torch``). The function then keeps both
representation in sync and creates an interface between the underlying
computation graphs.

The main optimization loop looks as follows:

.. code-block:: python

    for it in range(100):
        # Zero out gradients before each iteration
        opt.zero_grad()

        # Perform a differentiable rendering of the scene
        image = render_torch(scene, params=params, unbiased=True,
                             spp=1, **params_torch)

        write_bitmap('out_%03i.png' % it, image, crop_size)

        # Objective: MSE between 'image' and 'image_ref'
        ob_val = objective(image, image_ref)

        # Back-propagate errors to input parameters
        ob_val.backward()

        # Optimizer: take a gradient step
        opt.step()

        # Compare iterate against ground-truth value
        err_ref = objective(params_torch['red.reflectance.value'], param_ref)
        print('Iteration %03i: error=%g' % (it, err_ref * 3))

.. warning::

    **Memory caching**: When a GPU array in Enoki or PyTorch is destroyed, its
    memory is not immediately released back to the GPU. The reason for this is
    that allocating and releasing GPU memory are both extremely expensive
    operations, and any unused memory is therefore instead placed into a cache
    for later re-use.

    The fact that this happens is normally irrelevant when *only* using Enoki
    or *only* using PyTorch, but it can be a problem when using *both* at the
    same time, as the cache of one system may grow sufficiently large that
    allocations by the other system fail, despite plenty of free memory
    technically being available.

    If you notice that your programs crash with out-of-memory errors, try
    passing ``malloc_trim=True`` to the ``render_torch`` function. This
    flushes PyTorch's memory cache before executing any Enoki code, and vice
    versa. This is something of a last resort---generally, it's better to
    reduce memory requirements by lowering the number of samples per pixel,
    as flushing the cache causes severe performance penalty.

.. note::

    The full Python script of this tutorial can be found in the file:
    :file:`docs/examples/10_diff_render/invert_cbox_torch.py`.
