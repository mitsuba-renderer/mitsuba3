Writing and running tests
=========================

To run the test suite, simply invoke ``pytest``:

.. code-block:: bash

    pytest

The build system also exposes a ``pytest`` target that executes ``setpath`` and
parallelizes the test execution.

.. code-block:: bash

    # if using Makefiles
    make pytest

    # if using ninja
    ninja pytest