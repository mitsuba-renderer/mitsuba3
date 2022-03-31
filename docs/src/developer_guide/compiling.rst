.. _sec-compiling:

Compiling the system
====================

Cloning the repository
----------------------

Compiling Mitsuba 3 from scratch requires recent versions of CMake (at least
**3.9.0**) and Python (at least **3.6**). Further platform-specific dependencies
and compilation instructions are provided below for each operating system. Some
additional steps are required for GPU-based backends that are described at the
end of this section.

Mitsuba depends on several external dependencies, and its repository directly
refers to specific versions of them using a Git feature called *submodules*.
Cloning Mitsuba's repository will recursively fetch these dependencies, which
are subsequently compiled using a single unified build system. This dramatically
reduces the number steps needed to set up the renderer compared to previous
versions of Mitsuba.

For all of this to work out properly, you will have to specify the
``--recursive`` flag when cloning the repository:

.. code-block:: bash

    git clone --recursive https://github.com/mitsuba-renderer/mitsuba3

If you already cloned the repository and forgot to specify this flag, it's
possible to fix the repository in retrospect using the following command:

.. code-block:: bash

    git submodule update --init --recursive

**Staying up-to-date**

Unfortunately, pulling from the main repository won't automatically keep the
submodules in sync, which can lead to various problems. The following command
installs a git alias named ``pullall`` that automates these two steps.

.. code-block:: bash

    git config --global alias.pullall '!f(){ git pull "$@" && git submodule
    update --init --recursive; }; f'

Afterwards, simply write

.. code-block:: bash

    git pullall

to fetch the latest version of Mitsuba 3.

Configuring :monosp:`mitsuba.conf`
----------------------------------

Mitsuba 3 variants are specified in the file :monosp:`mitsuba.conf`. This file
can be found in the build directory and will be created when executing CMake the
first time.

Open :monosp:`mitsuba.conf` in your favorite text editor and scroll down to the
declaration of the enabled variants (around line 86):

.. code-block:: text

    "enabled": [
        "scalar_rgb", "scalar_spectral", "cuda_ad_rgb", "llvm_ad_rgb"
    ],

The default file specifies two scalar variants that you may wish to extend
according to your requirements and the explanations given above. Note that
``scalar_spectral`` can be removed, but ``scalar_rgb`` *must* currently be part
of the list as some core components of Mitsuba depend on it. If Mitsuba is
launched from the command line without any specific mode parameter, the first
variant of the list below will be used.

You may also wish to change the *Python default* variant that is executed if no
variant is explicitly specified (this must be one of the entries of the
``enabled`` list):

.. code-block:: text

    "python-default": "llvm_ad_rgb",

The remainder of this file lists the C++ types defining the available variants
and can safely be ignored.

TLDR: If you plan to use Mitsuba from Python, we recommend adding one of
``llvm_ad_rgb`` or ``llvm_ad_spectral`` for CPU rendering, or one of
``cuda_ad_rgb`` or ``cuda_ad_spectral`` for differentiable GPU rendering.

.. warning::

    Note that compilation time and compilation memory usage is roughly
    proportional to the number of enabled variants, hence including many of them
    (more than five) may not be advisable. Mitsuba 3 developers will typically
    want to restrict themselves to 1-2 variants used by their current experiment
    to minimize edit-recompile times. Also note that the ``scalar_rgb`` variant
    is mandatory.

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

    # Install recent versions build tools, including Clang and libc++ (Clang's
    C++ library) sudo apt install clang-9 libc++-9-dev libc++abi-9-dev cmake
    ninja-build

    # Install libraries for image I/O and the graphical user interface sudo apt
    install libz-dev libpng-dev libjpeg-dev libxrandr-dev libxinerama-dev
    libxcursor-dev

    # Install required Python packages sudo apt install python3-dev
    python3-distutils python3-setuptools

Additional packages are required to run the included test suite or to generate
HTML documentation (see :ref:`Developer guide <sec-devguide>`). If those are
interesting to you, also enter the following commands:

.. code-block:: bash

    # For running tests sudo apt install python3-pytest python3-pytest-xdist
    python3-numpy

    # For generating the documentation sudo apt install python3-sphinx
    python3-guzzle-sphinx-theme python3-sphinxcontrib.bibtex

Next, ensure that two environment variables :monosp:`CC` and :monosp:`CXX` are
exported. You can either run these two commands manually before using CMake
or---even better---add them to your :monosp:`~/.bashrc` file. This ensures that
CMake will always use the correct compiler.

.. code-block:: bash

    export CC=clang-9 export CXX=clang++-9

If you installed another version of Clang, the version suffix of course has to
be adjusted. Now, compilation should be as simple as running the following from
inside the :monosp:`mitsuba3` root directory:

.. code-block:: bash

    # Create a directory where build products are stored mkdir build cd build
    cmake -GNinja .. ninja


**Tested version**

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

On Windows, a recent version of `Visual Studio 2019
<https://visualstudio.microsoft.com/vs/>`_ is required. Some tools such as git,
CMake, or Python (e.g. via `Miniconda 3
<https://docs.conda.io/en/latest/miniconda.html>`_) might need to be installed
manually. Mitsuba's build system *requires* access to Python >= 3.6 even if you
do not plan to use Mitsuba's python interface.

From the root `mitsuba3` directory, the build can be configured with:

.. code-block:: bash

    # To be safe, explicitly ask for the 64 bit version of Visual Studio cmake
    -G "Visual Studio 16 2019" -A x64


Afterwards, open the generated ``mitsuba.sln`` file and proceed building as
usual from within Visual Studio. You will probably also want to set the build
mode to *Release* there.

Additional packages are required to run the included test suite or to generate
HTML documentation (see :ref:`Developer guide <sec-devguide>`). If those are
interesting to you, also enter the following commands:

.. code-block:: bash

    conda install pytest numpy sphinx


**Tested version**

* Windows 10
* Visual Studio 2019 (Community Edition) Version 16.4.5
* cmake 3.16.4 (64bit)
* git 2.25.1 (64bit)
* Miniconda3 4.7.12.1 (64bit)


macOS
-----

On macOS, you will need to install Xcode, CMake, and `Ninja
<https://ninja-build.org/>`_. Additionally, running the Xcode command line tools
once might be necessary:

.. code-block:: bash

    xcode-select --install

Note that the default Python version installed with macOS is not compatible with
Mitsuba 3, and a more recent version (at least 3.6) needs to be installed (e.g.
via `Miniconda 3 <https://docs.conda.io/en/latest/miniconda.html>`_ or `Homebrew
<https://brew.sh/>`_).

Now, compilation should be as simple as running the following from inside the
`mitsuba3` root directory:

.. code-block:: bash

    mkdir build cd build cmake -GNinja .. ninja


**Tested version**

* macOS Catalina 10.15.2
* Xcode 11.3.1
* cmake 3.16.4
* Python 3.7.3


Running Mitsuba
---------------

Once Mitsuba is compiled, run the ``setpath.sh/bat`` script in your build
directory to configure environment variables
(``PATH/LD_LIBRARY_PATH/PYTHONPATH``) that are required to run Mitsuba.

.. code-block:: bash

    # On Linux / Mac OS source setpath.sh

    # On Windows C:/.../mitsuba3/build> setpath

Mitsuba can then be used to render scenes by typing

.. code-block:: bash

    mitsuba scene.xml

where ``scene.xml`` is a Mitsuba scene file. Alternatively,

.. code-block:: bash

    mitsuba -m scalar_spectral_polarized scene.xml

renders with a specific variant that was previously enabled in
:monosp:`mitsuba.conf`. Call ``mitsuba --help`` to print additional information
about the various possible command line options.


GPU variants
------------

Variants of Mitsuba that run on the GPU (e.g. :monosp:`cuda_rgb`,
:monosp:`cuda_ad_spectral`, etc.) additionally depend on the `NVIDIA CUDA
Toolkit <https://developer.nvidia.com/cuda-downloads>`_ and `NVIDIA OptiX
<https://developer.nvidia.com/designworks/optix/download>`_. CUDA needs to be
installed manually while OptiX 7 ships natively with the latest GPU driver. Make
sure to have an up-to-date GPU driver if the framework fails to compile the GPU
variants of Mitsuba.

Tested versions of CUDA include 10.0, 10.1, and 10.2. Only OptiX 7 is supported
at this moment.

In case your CUDA installation is not automatically found by CMake (for instance
because the directory is not in `PATH`), you need to either set the environment
variable `CUDACXX` or the CMake cache entry `CMAKE_CUDA_COMPILER` to the full
path to the compiler. E.g.

.. code-block:: bash

    # Environment variable export CUDACXX=/usr/local/cuda/bin/nvcc

    # or

    # As part of the CMake process cmake ..
    -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc

By default, Mitsuba is able to resolve the OptiX API itself, and therefore does
not rely on the ``optix.h`` header file. The ``MI_USE_OPTIX_HEADERS`` CMake flag
can be used to turn off this feature if a developer wants to experiment with
parts of the OptiX API not yet exposed to the framework.

Embree
------

By default, Mitsuba's ``scalar`` and ``llvm`` backends use Intel's Embree
library for ray tracing instead of the builtin kd-tree in Mitsuba 3. To change
this behavior, invoke CMake with the ``-DMI_ENABLE_EMBREE=0`` parameter
or use a visual CMake tool like ``cmake-gui`` or ``ccmake`` to flip the value of
this parameter. Embree tends to be faster but lacks some features such as
support for double precision ray intersection.
