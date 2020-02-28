.. _sec-inverse-rendering:

Introduction
============

Mitsuba 2 can be used to solve inverse problems involving light using a
technique known as *differentiable rendering*. It interprets the rendering
algorithm as a function :math:`f(\mathbf{x})` that converts an input
:math:`\mathbf{x}` (the scene description) into an output :math:`\mathbf{y}`
(the rendering). This function :math:`f` is then mathematically differentiated
to obtain :math:`\frac{\mathrm{d}\mathbf{y}}{\mathrm{d}\mathbf{x}}`, providing
a first-order approximation of how a desired change in the output
:math:`\mathbf{y}` (the rendering) can be achieved by changing the inputs
:math:`\mathbf{x}` (the scene description). Together with a differentiable
*objective function* :math:`g(\mathbf{y})` that quantifies the suitability of
tentative scene parameters, a gradient-based optimization algorithm such as
stochastic gradient descent or Adam can then be used to find a sequence of
scene parameters :math:`\mathbf{x}_0`, :math:`\mathbf{x}_1`,
:math:`\mathbf{x}_2`, etc., that successively improve the objective function.
In pictures:

.. image:: ../../../resources/data/docs/images/autodiff/autodiff.jpg
    :width: 100%
    :align: center

Differentiable rendering in Mitsuba is based on variants that use the
``gpu_autodiff`` backend. Note that differentiable rendering on the CPU is
currently not supported for performance reasons, but we have some ideas on
making this faster and plan to incorporate them in the future.

Differentiable calculations using Enoki
=======================================

Mitsuba's ability to differentiate entire rendering algorithms builds on
differentiable CUDA array types provided by the Enoki library. Both are
explained in detail in the Enoki documentation. In particular, the section on
`GPU arrays <https://enoki.readthedocs.io/en/master/gpu.html>`_ describes the
underlying *just-in-time* (JIT) compiler, which fuses simple operations like
additions and multiplications into larger computational kernels that can be
executed on CUDA-capable GPUs. The linked document also discusses key
differences compared to superficially similar frameworks like PyTorch and
TensorFlow. The section on `Automatic differentiation
<https://enoki.readthedocs.io/en/master/autodiff.html>`_ describes how Enoki
records and simplifies computation graphs and uses them to propagate
derivatives in forward or reverse mode.

These differentiable types are automatically imported when variant starting
with ``gpu_autodiff_*`` is specified. They are used in both C++ and Python,
hence it is possible to differentiate larger computations that are partly
implemented in each language. The following program shows a simple example
calculation conducted in Python, which differentiates the function
:math:`f(\mathbf{x})=x_0^2 + x_1^2 + x_2^2`

.. code-block:: python

    import mitsuba
    mitsuba.set_variant('gpu_autodiff_rgb')

    # The C++ type associated with 'Float' is enoki::DiffArray<enoki::CUDAArray<float>>
    from mitsuba.core import Float
    import enoki as ek

    # Initialize a dynamic CUDA floating point array with some values
    x = Float([1, 2, 3])

    # Tell Enoki that we'll later be interested in gradients of
    # an as-of-yet unspecified objective function with respect to 'x'
    ek.set_requires_gradient(x)

    # Example objective function: sum of squares
    y = ek.hsum(x * x)

    # Now back-propagate gradient wrt. 'y' to input variables (i.e. 'x')
    ek.backward(y)

    # Prints: [2, 4, 6]
    print(ek.gradient(x))

Enumerating scene parameters
============================

We now progressively build up a simple example application that showcases
differentiation and optimization of a light transport simulation involving the
well-known Cornell Box scene that can be downloaded `here
<http://mitsuba-renderer.org/scenes/cbox.zip>`_.

Since differentiable rendering requires a starting guess (i.e. an initial scene
configuration), most applications of differentiable rendering will begin by
loading an existing scene and enumerating and selecting scene parameters that
will subsequently be differentiated:

.. code-block:: python

    import enoki as ek
    import mitsuba
    mitsuba.set_variant('gpu_autodiff_rgb')

    from mitsuba.core import Thread
    from mitsuba.core.xml import load_file
    from mitsuba.python.util import traverse

    # Load the Cornell Box
    Thread.thread().file_resolver().append('cbox')
    scene = load_file('cbox/cbox.xml')

    # Find differentiable scene parameters
    params = traverse(scene)
    print(params)

The call to :py:func:`mitsuba.python.util.traverse()` in the second-to-last
line returns a dictionary-like parameter map object, which exposes modifiable
and differentiable scene parameters. Printing its contents yields the following
output (abbreviated):

.. code-block:: text

    ParameterMap[
        ...
        box.reflectance.value,
        white.reflectance.value,
        red.reflectance.value,
        green.reflectance.value,
        light.reflectance.value,
        ...
        OBJMesh.emitter.radiance.value,
        OBJMesh.vertex_count,
        OBJMesh.face_count,
        OBJMesh.faces,
        OBJMesh.vertex_positions,
        OBJMesh.vertex_normals,
        OBJMesh.vertex_texcoords,
        OBJMesh_1.vertex_count,
        OBJMesh_1.face_count,
        OBJMesh_1.faces,
        OBJMesh_1.vertex_positions,
        OBJMesh_1.vertex_normals,
        OBJMesh_1.vertex_texcoords,
        ...
    ]

The Cornell box scene consists of a single light source, and multiple multiple
meshes and BRDFs. Each component contributes certain entries to the above
list---for instance, meshes specify their face and vertex counts in addition to
per-face (``.faces``) and per-vertex data (``.vertex_positions``, ``.vertex_normals``,
``.vertex_texcoords``). Each BRDF adds a ``.reflectance.value`` entry. Not all
of these parameters are differentiable---some, e.g., store integer values.

The names are generated using a simple naming scheme based on the position in
the scene graph and class name of the underlying implementation. Whenever an
object was assigned a unique identifier in the XML scene description, this
identifier has precedence. For instance, The ``red.reflectance.value`` entry
corresponds to the albedo of the following declaration in the original
scene description:

.. code-block:: xml

    <bsdf type="diffuse" id="red">
        <spectrum name="reflectance" value="400:0.04, 404:0.046, ..., 696:0.635, 700:0.642"/>
    </bsdf>

We can also query the ``ParameterMap`` to see the actual parameter values:

.. code-block:: python

    print(params['red.reflectance.value'])

    # Prints:
    # [[0.569717, 0.0430141, 0.0443234]]

In this case, we can see how Mitsuba converted the original spectral curve into
an RGB value due to the ``gpu_autodiff_rgb`` variant being used to run this
example. We can also change individual parameters without having to reload the
scene. The call to the ``update()`` method at the end is mandatory to inform
changed scene objects that they should update their internal state

.. code-block:: python

    params['red.reflectance.value'] = [.1, .2, .3]
    params.update()

In many cases, the parameter map will be extremely large, and our main interest
is to optimize a small subset of the available parameters. The
``ParameterMap.keep()`` method discards all entries except for those contained
in the specified list of keys.

.. code-block:: python

    params.keep(['red.reflectance.value'])
    print(params)

    # Prints:
    # ParameterMap[
    #     red.reflectance.value
    # ]


Differentiating a simple rendering
==================================

.. code-block:: python

    params = traverse(scene)
    params.keep(['red.reflectance.value'])

    # Print current value and save a copy
    param_ref = Vector3f(params['red.reflectance.value'])

    # Render a reference image
    image_ref = render(scene)
    crop_size = scene.sensors()[0].film().crop_size()
    write_bitmap('out.png', image_ref, crop_size)

    # Now, change the parameter to something else
    params['red.reflectance.value'] = [.9, .9, .9]
    params.update()

    opt = Adam(params, lr=.025)

    for it in range(100):
        # Perform a differentiable rendering of the scene
        image = render_diff(scene, opt, unbiased=True)

        write_bitmap('out_%03i.png' % it, image, crop_size)

        # Note: printing the loss is not hugely informative
        # since it is almost purely MC noise
        loss_val = ek.hsum(ek.sqr(image - image_ref)) / len(image)

        # Instead, check convergence to the known parameter
        param = params['red.reflectance.value']
        testing_loss_val = ek.hsum(ek.sqr(param_ref - param))
        print('Iteration %03i: testing loss=%g\r' % (it, testing_loss_val[0]), end='')

        ek.backward(loss_val)
        opt.step()
    print()

