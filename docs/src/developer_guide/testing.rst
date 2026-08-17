Testing
=======

To run the test suite, simply invoke ``pytest``:

.. code-block:: bash

    pytest

    # or to run a single test file
    pytest src/bsdfs/tests/test_diffuse.py


If you want to skip the execution of the slower tests, you can do so by running
the test suite with the following flag.

.. code-block:: bash

    pytest -m 'not slow'

The build system also exposes a ``pytest`` target that executes ``setpath`` and
parallelizes the test execution.

.. code-block:: bash

    ninja pytest

Testing multiple variants
-------------------------

The system provides a variety of Python fixture to automatically run unit tests
on subsets of the available variants. Those should be passed a first argument to
the test function like in the following example:

.. code-block:: python

    # This test will run on all available variants
    def test_hello_world(variants_all):
        print(f'Hello {mi.variant()}')
        assert True

.. warning::

    :monosp:`variants_all` and :monosp:`variants_all_scalar` **exclude acoustic
    variants by design**. These groups (and the plain :monosp:`variants_all_*`
    families below) predate misuka's acoustic extension and are meant for
    upstream-style optical tests. Acoustic-aware tests must use
    :monosp:`variants_all_acoustic` or one of the more specific acoustic groups
    below. See the ``variant_groups`` comment in ``src/conftest.py`` for the
    rationale.

Here is the current list of available fixtures (from the ``variant_groups`` dict
in ``src/conftest.py``):

- :monosp:`variants_any_scalar`: one ``scalar_*`` variant (excluding acoustic) if available
- :monosp:`variants_any_llvm`: one ``llvm_*`` variant (excluding acoustic) if available
- :monosp:`variants_any_cuda`: one ``cuda_*`` variant (excluding acoustic) if available
- :monosp:`variants_any_metal`: one ``metal_*`` variant (excluding acoustic) if available
- :monosp:`variants_any_acoustic`: one ``*_acoustic`` variant if available
- :monosp:`variants_all`: all variants, **excluding acoustic**
- :monosp:`variants_all_optical`: alias for :monosp:`variants_all` (all variants, excluding acoustic)
- :monosp:`variants_all_scalar`: all ``scalar_*`` variants, excluding acoustic
- :monosp:`variants_all_rgb`: all ``*_rgb`` variants
- :monosp:`variants_all_rgb_unpolarized`: all ``*_rgb`` variants, excluding polarized
- :monosp:`variants_all_spectral`: all ``*_spectral`` variants
- :monosp:`variants_all_acoustic`: all ``*_acoustic`` variants
- :monosp:`variants_all_backends_once`: at least one variant per backend (scalar/llvm/cuda/metal), excluding acoustic
- :monosp:`variants_vec_backends_once`: one ``llvm_*``, one ``cuda_*``, and one ``metal_*`` variant if available, excluding acoustic
- :monosp:`variants_vec_backends_once_rgb`: one ``llvm_*_rgb``, one ``cuda_*_rgb``, and one ``metal_*_rgb`` variant if available
- :monosp:`variants_vec_backends_once_spectral`: one ``llvm_*_spectral``, one ``cuda_*_spectral``, and one ``metal_*_spectral`` variant if available
- :monosp:`variants_vec_rgb`: all non-scalar ``*_rgb`` variants
- :monosp:`variants_vec_spectral`: all non-scalar ``*_spectral`` variants
- :monosp:`variants_all_ad_rgb`: all ``*_ad_rgb`` variants
- :monosp:`variants_all_ad_rgb_unpolarized`: all ``*_ad_rgb`` variants, excluding polarized
- :monosp:`variants_all_ad_spectral`: all ``*_ad_spectral`` variants
- :monosp:`variants_all_ad_acoustic`: all ``*_ad_acoustic`` variants
- :monosp:`variants_all_cuda_ad_acoustic`: all ``cuda_ad_acoustic`` variants
- :monosp:`variants_all_llvm_ad_acoustic`: all ``llvm_ad_acoustic`` variants
- :monosp:`variants_all_jit_acoustic`: all non-scalar ``*_acoustic`` variants

Chi^2 tests
-----------

The ``misuka.chi2`` module implements the Pearson's chi-square test for
testing goodness of fit of a distribution to a known reference distribution.

The implementation specifically compares a Monte Carlo sampling strategy on a
2D (or lower dimensional) space against a reference distribution obtained by
numerically integrating a probability density function over grid in the
distribution's parameter domain.

This is used extensively throughout the test suite to valid the implementation
of ``BSDFs``, ``Emitters``, and other sampling code.

It is possible to test your own sampling code in the following way:

.. code-block:: python

    import misuka as mi
    mi.set_variant('llvm_rgb')

    # some sampling code
    def my_sample(sample):
        return mi.warp.square_to_cosine_hemisphere(sample)

    # the corresponding probability density function
    def my_pdf(p):
        return mi.warp.square_to_cosine_hemisphere_pdf(p)

    chi2 = mi.ChiSquareTest(
        domain=mi.SphericalDomain(),
        sample_func=my_sample,
        pdf_func=my_pdf,
        sample_dim=2
    )

    assert chi2.run()

In case of failure, the target density and histogram were written to
``chi2_data.py`` which can simply be run to plot the data:

.. code-block:: bash

    python chi2_data.py


The ``misuka.chi2`` module also provides a set of ``Adapter`` functions
which can be used to wrap different plugins (e.g. ``BSDF``, ``Emitter``, ...)
in order to test them:

.. code-block:: python

    import misuka as mi
    import drjit as dr

    mi.set_variant('llvm_rgb')

    xml = """<float name="alpha" value="0.5"/>
             <boolean name="sample_visible" value="false"/>
             <string name="distribution" value="ggx"/>
          """
    wi = dr.normalize([0.2, -0.6, -0.5])
    sample_func, pdf_func = mi.BSDFAdapter("roughdielectric", xml, wi=wi)

    chi2 = mi.ChiSquareTest(
        domain=mi.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()

    # Forces the chi2 test to dump the plotting script (optional)
    chi2._dump_tables()


Here is the figure generated by the ``chi2_data.py`` script from the example above:

.. image:: ../../../resources/data/docs/images/misc/chi2_example.png
    :align: center
    :width: 100%

The plot on the left shows the density function generated by numerically
integrating the analytical ``pdf()`` method of a ``roughdielectric`` BSDF with
an incoming vector coming from inside. Most of the energy leaves the surface
(upper half of the plot) while some energy gets reflected back inside the
surface (lower half of the plot).

The middle plot shows the same density function but this time computed as a
histogram of sampled directions resulting from the ``sample()`` method of the
``roughdielectric`` BSDF.

The right plot shows the difference between the two density functions. The
sampling routine of the BSDF being stochastic, it is expected to see a mix of
negative and positive values as the histogram is still noisy. The main role of
the ``ChiSquareTest`` is to decide whether the observed deviation is within the
range of random noise, or whether there are systematic biases that should lead
to a test failure.

For more information, see :py:class:`misuka.chi2.ChiSquareTest`.


Rendering test suite and Z-test
-------------------------------

On top of test *unit tests*, the framework implements a mechanism that
automatically renders a set of test scenes and applies the `Z-test
<https://en.wikipedia.org/wiki/Z-test>`_ to compare the resulting images and
some reference images.

Those tests are really useful to reveal bugs at the interaction between the
individual components of the renderer.

The test scenes are rendered using all the different enabled variants of the
renderer, ensuring for instance that the ``scalar_rgb`` renders match the
``cuda_rgb`` renders.

To only run the rendering test suite, use the following command:

.. code-block:: bash

    pytest src/render/tests/test_renders.py

Acoustic regression suite
--------------------------

The acoustic integrators (``acoustic_ad``, ``acoustic_prb``,
``acoustic_ad_threepoint``, ``acoustic_prb_threepoint``) have a separate
regression suite in ``src/integrators/tests/test_acoustic_ad_integrators.py``.
For each integrator and a set of predefined scene configurations, it checks
primal rendering against reference ETCs stored in
``resources/data_acoustic/tests/``, and adjoint forward/backward rendering
against finite differences.

.. code-block:: bash

    pytest src/integrators/tests/test_acoustic_ad_integrators.py

Reference ETCs can be regenerated (e.g. after adding a new configuration) by
running the file directly with Python:

.. code-block:: bash

    python3 src/integrators/tests/test_acoustic_ad_integrators.py --help

One can easily add a scene to the ``resources/data/tests/scenes/`` folder to add
it to the rendering test suite. Then, the missing reference images can be
generated using the following command:

.. code-block:: bash

    python src/render/tests/test_renders.py
