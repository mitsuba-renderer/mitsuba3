.. _sec-differentiable-rendering:

Differentiable rendering
========================

We now progressively build up a simple example application that showcases
differentiation and optimization of a light transport simulation involving the
well-known Cornell Box scene that can be downloaded `here
<http://mitsuba-renderer.org/scenes/cbox.zip>`_.

Enumerating scene parameters
----------------------------

Since differentiable rendering requires a starting guess (i.e. an initial scene
configuration), most applications will begin by loading an existing scene and
enumerating and selecting scene parameters that will subsequently be
differentiated:

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
line serves this purpose. It returns a dictionary-like ``ParameterMap``
instance, which exposes modifiable and differentiable scene parameters. Passing
it to ``print()`` yields the following summary (abbreviated):

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
object was assigned a unique identifier via the ``id="..."`` attribute in the
XML scene description, this identifier has precedence. For instance, The
``red.reflectance.value`` entry corresponds to the albedo of the following
declaration in the original scene description:

.. code-block:: xml

    <bsdf type="diffuse" id="red">
        <spectrum name="reflectance" value="400:0.04, 404:0.046, ..., 696:0.635, 700:0.642"/>
    </bsdf>

We can also query the ``ParameterMap`` to see the actual parameter value:

.. code-block:: python

    print(params['red.reflectance.value'])

    # Prints:
    # [[0.569717, 0.0430141, 0.0443234]]

Here, we can see how Mitsuba converted the original spectral curve from the
above XML fragment into an RGB value due to the ``gpu_autodiff_rgb`` variant
being used to run this example. 

.. code-block:: python

    params['red.reflectance.value'] = [.6, .0, .0]
    params.update()

In most cases, we will only be interested in differentiating a small subset of
the (typically very large) parameter map. Use the ``ParameterMap.keep()``
method to discard all entries except for the specified list of keys.

.. code-block:: python

    params.keep(['red.reflectance.value'])
    print(params)

    # Prints:
    # ParameterMap[
    #     red.reflectance.value
    # ]

Let's also make a backup copy of this color value for later use.

.. code-block:: python

    from mitsuba.core import Color3f
    param_ref = Color3f(params['red.reflectance.value'])


Differentiating a rendering
---------------------------

In contrast to the :ref:`previous example <sec-rendering-scene>` on using the
Python API to render images, the differentiable rendering path involves two
different specialized functions :py:func:`mitsuba.python.autodiff.render()` and
:py:func:`mitsuba.python.autodiff.render_diff()` that don't involve the scene's
film and directly return GPU arrays containing the generated image (the
difference between the two will be explained shortly.) The function
:py:func:`mitsuba.python.autodiff.write_bitmap()` reshapes the output into an
image of the correct size and exports it to any of the supported image formats
(OpenEXR, PNG, JPG, RGBE, PFM) while automatically performing format conversion
and gamma correction for 8 bit formats.

Using this functionality, we will now generate a reference image.

.. code-block:: python

    # Render a reference image (no derivatives used yet)
    from mitsuba.python.autodiff import render, render_diff, write_bitmap
    image_ref = render(scene)
    crop_size = scene.sensors()[0].film().crop_size()
    write_bitmap('out_ref.png', image_ref, crop_size)


Our first experiment is going to be very simple: we will change the color of
the red wall and then try to recover the original color using differentiation
along with the reference image generated above.

For this, let's first change the current color value: the parameter map enables
such changes without having to reload the scene. The call to the ``update()``
method at the end is mandatory to inform changed scene objects that they should
refresh their internal state.

.. code-block:: python

    # Now, change the parameter to something else
    params['red.reflectance.value'] = [.9, .9, .9]
    params.update()


Mitsuba can optimize scene parameters in *standalone mode* using optimization
algorithms implemented on top of Enoki, or by integrating with PyTorch. We
generally recommend standalone mode unless your computation contains elements
where PyTorch provides a clear advantage (for example, neural network building
blocks like fully connected layers or convolutions). The remainder of this
section discusses standalone mode, and the section on :ref:`PyTorch integration
<sec-pytorch>` shows how to adapt the example code for PyTorch.

Mitsuba ships with standard optimizers including *Stochastic Gradient Descent*
(SGD) and *Nesterov-Accelerated SGD* (both in
:py:class:`mitsuba.python.autodiff.SGD`) and *Adam* :cite:`kingma2014adam`
(:py:class:`mitsuba.python.autodiff.Adam`). We will instantiate the latter and
optimize the ``ParameterMap`` ``params`` with a learning rate of 0.025.

.. code-block:: python

    # Construct an Adam optimizer that will adjust the parameters 'params'
    from mitsuba.python.autodiff import Adam
    opt = Adam(params, lr=.025)

The remaining commands are all part of a loop that executes 100 differentiable
rendering iterations.

.. code-block:: python

    for it in range(100):
        # Perform a differentiable rendering of the scene
        image = render_diff(scene, opt, unbiased=True)

        write_bitmap('out_%03i.png' % it, image, crop_size)

.. code-block:: python


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

