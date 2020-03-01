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
---------------------------------------

Mitsuba's ability to automatically differentiate entire rendering algorithms
builds on differentiable CUDA array types provided by the Enoki library. Both
are explained in the Enoki documentation: the section on `GPU arrays
<https://enoki.readthedocs.io/en/master/gpu.html>`_ describes the underlying
*just-in-time* (JIT) compiler, which fuses simple operations like additions and
multiplications into larger computational kernels that can be executed on
CUDA-capable GPUs. The linked document also discusses key differences compared
to superficially similar frameworks like PyTorch and TensorFlow. The section on
`Automatic differentiation
<https://enoki.readthedocs.io/en/master/autodiff.html>`_ describes how Enoki
records and simplifies computation graphs and uses them to propagate
derivatives in forward or reverse mode. We recommend that you familiarize
yourself with both of these documents.

Enoki's differentiable types are automatically imported when variant starting
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

