Building the documentation
==========================

To build the documentation, use the following command:

.. code-block:: bash

    cd build

    # Build using CMake & GNU Make (legacy)
    cmake ..
    make mkdoc

    # OR: Build using CMake & Ninja
    cmake -GNinja ..
    ninja mkdoc