.. _sec-compiling:

Compiling
=========

Before continuing, please make sure that you have read and followed the
instructions on :ref:`cloning Mitsuba 2 and its dependencies <sec-cloning>` and
:ref:`choosing desired variants <sec-variants>`.

Compiling Mitsuba 2 from scratch requires recent versions of CMake (at least
**3.9.0**) and Python (at least **3.6**). Further platform-specific
dependencies and compilation instructions are provided below for each operating
system.

Linux
-----

The build process under Linux requires several external dependencies that are
easily installed using the system-provided package manager (e.g.,
:monosp:`apt-get` under Ubuntu). 

Note that recent Linux distributions include two different compilers that can
both be used for C++ software development. `GCC <https://gcc.gnu.org>`_ is
typically the default, and `Clang <https://clang.llvm.org>`_ can be installed
optionally. During the development of this project, we encountered many issues
with GCC (mis-compilations, compiler errors, segmentation faults), and strongly
recommend that you use Clang instead.

To fetch all dependencies and Clang, enter the following commands on Ubuntu:

.. code-block:: bash

    # Install recent versions build tools, including Clang and libc++ (Clang's C++ library)
    sudo apt install clang-9 libc++-9-dev libc++abi-9-dev cmake ninja-build

    # Install libraries for image I/O and the graphical user interface
    sudo apt install libz-dev libpng-dev libjpeg-dev libxrandr-dev libxinerama-dev libxcursor-dev

    # Install required Python packages
    sudo apt install python3-dev python3-distutils python3-setuptools

Additional packages are required to run the included test suite or to generate HTML
documentation (see :ref:`Developer guide <sec-devguide>`). If those are interesting to you, also
enter the following commands:

.. code-block:: bash

    # For running tests
    sudo apt install python3-pytest python3-pytest-xdist python3-numpy

    # For generating the documentation
    sudo apt install python3-sphinx python3-guzzle-sphinx-theme python3-sphinxcontrib.bibtex

Next, please ensure that two environment variables :monosp:`CC` and
:monosp:`CXX` are exported. You can either run these two commands manually before using CMake 
or---even better---add them to your :monosp:`~/.bashrc` file. This ensures that
CMake will use the correct compiler.

.. code-block:: bash

    export CC=clang-9
    export CXX=clang++-9

If you installed another version of Clang, the version suffix of course has to be adjusted.
Now, compilation should be as simple as running the following from inside the
:monosp:`mitsuba2` root directory:

.. code-block:: bash

    # Create a directory where build products are stored
    mkdir build
    cd build
    cmake -GNinja ..
    ninja


Tested versions
^^^^^^^^^^^^^^^

The above procedure will likely work on many different flavors of Linux (with
slight adjustments for the package manager and package names). We have mainly
worked with software environment listed below, and our instructions should work
without modifications in that case.

* Ubuntu 19.10
* clang 9.0.0-2 (tags/RELEASE_900/final)
* cmake 3.13.4
* ninja 1.9.0
* python 3.7.5

Windows
-------

On Windows, a recent version of `Visual Studio 2019 <https://visualstudio.microsoft.com/vs/>`_ is required.
Some tools such as git, CMake, or Python (e.g. via `Miniconda 3 <https://docs.conda.io/en/latest/miniconda.html>`_) might
need to be installed manually.

From the root `mitsuba2` directory, the build can be configured with:

.. code-block:: bash

    # To be safe, explicitly ask for the 64 bit version of Visual Studio
    cmake -G "Visual Studio 16 2019" -A x64


Afterwards, open the generated ``mitsuba.sln`` file and proceed building as usual from within Visual Studio.
You will probably also want to set the build mode to *Release* there.


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


macOS
-----

On macOS, you will need to install Xcode, CMake, and `Ninja <https://ninja-build.org/>`_.
Additionally, running the Xcode command line tools once might be necessary:

.. code-block:: bash

    xcode-select --install

Note that the default Python version installed with macOS is not compatible with Mitsuba 2, and a more recent version (at least 3.6) needs to be installed (e.g. via `Miniconda 3 <https://docs.conda.io/en/latest/miniconda.html>`_ or `Homebrew <https://brew.sh/>`_).

Now, compilation should be as simple as running the following from inside the `mitsuba2` root directory:

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

.. warning::

    Neither GPU- nor differentiable rendering currently work on MacOS, and this
    is unlikely to change in the future since they require a CUDA-compatible
    graphics card. Please voice your concern to Apple if you are unhappy with
    their expulsion of NVIDIA drivers from the Mac ecosystem.

