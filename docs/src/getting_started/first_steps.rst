First steps
===============

Compiling Mitsuba 2 from scratch requires recent versions of CMake (at least **3.9.0**)
and Python (at least **3.6**). Further platform-specific dependencies and compilation instructions
can be found below.

Compiling (Linux)
-----------------

The build process under Linux requires a few external dependencies that can be installed via a package manager (e.g. apt-get under Ubuntu):

.. code-block:: bash

    # Install recent versions of tools
    sudo apt install clang-9 libc++-9-dev libc++abi-9-dev cmake ninja-build

    # Install required libraries
    sudo apt install libz-dev libpng-dev libjpeg-dev libxrandr-dev libxinerama-dev libxcursor-dev

    # Install required Python packages
    sudo apt install python3-dev python3-distutils python3-setuptools


Make sure `CC` and `CXX` variables are exported manually or inside ``.bashrc`` in order for CMake to locate Clang:

.. code-block:: bash

    export CC=clang
    export CXX=clang++

Now, compilation should be as simple as running this from inside the `mitsuba2` root directory:

.. code-block:: bash

    mkdir build
    cd build
    cmake -GNinja ..
    ninja

Optional
^^^^^^^^

Additional packages are required in order to run the tests or to generate the documentation (see :ref:`Developer guide <sec-devguide>`):

.. code-block:: bash

    # For running tests
    sudo apt install python3-pytest python3-pytest-xdist python3-numpy

    # For generating the documentation
    sudo apt install python3-sphinx python3-guzzle-sphinx-theme python3-sphinxcontrib.bibtex


Tested versions (Ubuntu 19.10)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
* clang 9.0.0-2 (tags/RELEASE_900/final)
* cmake 3.13.4
* ninja 1.9.0
* python 3.7.5

Compiling (Windows)
-------------------

On Windows, a recent version of `Visual Studio 2019 <https://visualstudio.microsoft.com/vs/>`_ is required.
Some tools such as git, CMake, or Python (e.g. via `Miniconda 3 <https://docs.conda.io/en/latest/miniconda.html>`_) might
need to be installed manually.

From the root `mitsuba2` directory, the build can be setup configured with:

.. code-block:: bash

    # To be safe, explicitly ask for the 64 bit version of Visual Studio
    cmake -G "Visual Studio 16 2019" -A x64


Afterwards, open the generated ``mitsuba.sln`` file and proceed building as usual from within Visual Studio.


Optional
^^^^^^^^

Running the test (see :ref:`Developer guide <sec-devguide>`) additionally requires pytest and NumPy to be installed, e.g. from within conda:

.. code-block:: bash

    conda install pytest numpy


Tested versions (Windows 10)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^
* Visual Studio 2019 (Community Edition) Version 16.4.5
* cmake 3.16.4 (64bit)
* git 2.25.1 (64bit)
* Miniconda3 4.7.12.1 (64bit)


Compiling (macOS)
-----------------

On macOS, you will need to install Xcode, CMake, and `Ninja <https://ninja-build.org/>`_.
Additionally, running the Xcode command line tools might be necessary:

.. code-block:: bash

    xcode-select --install

Note that the default Python version installed with the system is not compatible with Mitsuba 2, and a more recent version (at least 3.6) needs to be installed (e.g. via `Miniconda 3 <https://docs.conda.io/en/latest/miniconda.html>`_ or `Homebrew <https://brew.sh/>`_).

Now, compilation should be as simple as running this from inside the `mitsuba2` root directory:

.. code-block:: bash

    mkdir build
    cd build
    cmake -GNinja ..
    ninja


Tested versions (macOS Catalina 10.15.2)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
* Xcode 11.3.1
* cmake 3.16.4
* Python 3.7.3



Running Mitsuba
---------------

Once Mitsuba is compiled, run the ``setpath.sh/bat`` script to configure
environment variables (``PATH/LD_LIBRARY_PATH/PYTHONPATH``) that are
required to run Mitsuba.

.. code-block:: bash

    # On Linux / Mac OS
    source setpath.sh

    # On Windows
    C:/.../mitsuba2> setpath

Mitsuba can then be used to render scenes by typing

.. code-block:: bash

    mitsuba scene.xml

where ``scene.xml`` is a Mitsuba scene file. Calling ``mitsuba --help`` will print additional information about various command line arguments.



GPU & Autodiff variants
-----------------------

When enabling GPU (e.g. `gpu_rgb`) or autodiff (e.g. `gpu_autodiff_spectral`) variants in ``mitsuba.conf``, Mitsuba 2 additionally depends on the `NVIDIA CUDA Toolkit <https://developer.nvidia.com/cuda-downloads>`_ for computation and `NVIDIA OptiX <https://developer.nvidia.com/designworks/optix/download>`_ for ray tracing that both need to be installed manually.

Tested versions of CUDA include 10.0, 10.1, and 10.2. Currently only OptiX 6.5 is supported.

.. warning:: These components are not supported on macOS.






Frequently asked questions
--------------------------

* Differentiable rendering fails with the error message "Failed to load Optix library": It is likely that the version of OptiX installed on your system is not compatible with the video driver (the two must satisfy version requirements that are detailed on the OptiX website).


