Testing
=======

To run the test suite, simply invoke ``pytest``:

.. code-block:: bash

    pytest

    # or to run a single test file
    pytest src/bsdfs/tests/test_diffuse.py

The build system also exposes a ``pytest`` target that executes ``setpath`` and
parallelizes the test execution.

.. code-block:: bash

    ninja pytest


Chi^2 tests
-----------

The ``mitsuba.python.chi2`` module implements the Pearson's chi-square test for
testing goodness of fit of a distribution to a known reference distribution.

The implementation specifically compares a Monte Carlo sampling strategy on a
2D (or lower dimensional) space against a reference distribution obtained by
numerically integrating a probability density function over grid in the
distribution's parameter domain.

This is used extensively throughout the test suite to valid the implementation
of ``BSDFs``, ``Emitters``, and other sampling code.

It is possible to test your own sampling code in the following way:

.. code-block:: python

    import mitsuba
    mitsuba.set_variant('packet_rgb')

    from mitsuba.python.chi2 import ChiSquareTest, SphericalDomain

    # some sampling code
    def my_sample(sample):
        return mitsuba.core.warp.square_to_cosine_hemisphere(sample)

    # the corresponding probability density function
    def my_pdf(p):
        return mitsuba.core.warp.square_to_cosine_hemisphere_pdf(p)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=my_sample,
        pdf_func=my_pdf,
        sample_dim=2
    )

    assert chi2.run()

In case of failure, the target density and histogram were written to
``chi2_data.py`` which can simply be run to plot the data:

.. code-block:: bash

    python chi2_data.py


The ``mitsuba.python.chi2`` module also provides a set of ``Adapter`` functions
which can be used to wrap different plugins (e.g. ``BSDF``, ``Emitter``, ...)
in order to test them:

.. code-block:: python

    import mitsuba
    mitsuba.set_variant('packet_rgb')

    from mitsuba.python.chi2 import BSDFAdapter, ChiSquareTest, SphericalDomain
    sample_func, pdf_func = BSDFAdapter("roughconductor",
                                        """<float name="alpha" value="0.05"/>""")

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()


.. Rendering test suite and Student-T test
.. ---------------------------------------

.. On top of test *unit tests*, the framework implements a mechanism that automatically renders a set
.. of test scenes and applies the *Student-T test* to compare the resulting images and some reference
.. images.

.. Those tests are really useful to reveal bugs at the interaction between the individual
.. components of the renderer.

.. The test scenes are rendered using all the different enabled variants of the renderer, ensuring for
.. instance that the ``scalar_rgb`` renders match the ``gpu_rgb`` renders.

.. To only run the rendering test suite, use the following command:

.. .. code-block:: bash

..     pytest src/librender/tests/test_renders.py

.. One can easily add a scene to the ``resources/data/tests/scenes/`` folder to add it to the rendering
.. test suite. Then, the missing reference images can be generated using the following command:

.. .. code-block:: bash

..     python src/librender/tests/test_renders.py --spp 512
