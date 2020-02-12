First steps
===============

Compiling
--------------

Compiling from scratch requires CMake and a recent version of XCode on Mac,
Visual Studio 2017 on Windows, or Clang on Linux.

On Linux and MacOS, compiling should be as simple as

.. code-block:: bash

    git clone --recursive https://github.com/mitsuba-renderer/mitsuba2
    cd mitsuba2
    mkdir build
    cd build

    # Build using CMake & GNU Make (legacy)
    cmake -D MTS_ENABLE_AUTODIFF=ON .. # include differentiable rendering support, or:
    cmake .. # do not include differentiable rendering support.
    make -j

    # OR: Build using CMake & Ninja (recommended, must install Ninja (https://ninja-build.org/) first)
    cmake -GNinja ..
    ninja


On Windows, open the generated ``mitsuba.sln`` file after running
``cmake .`` and proceed building as usual from within Visual Studio.

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


Staying up-to-date
------------------

Mitsuba organizes its software dependencies in a hierarchy of sub-repositories
using *git submodule*. Unfortunately, pulling from the main repository won't
automatically keep the sub-repositories in sync, which can lead to various
problems. The following command installs a git alias named ``pullall`` that
automates these two steps.

.. code-block:: bash

    git config --global alias.pullall '!f(){ git pull "$@" && git submodule update --init --recursive; }; f'

Afterwards, simply write

.. code-block:: bash

    git pullall

to stay in sync.

Frequently asked questions
--------------------------

* Differentiable rendering fails with the error message "Failed to load Optix library": It is likely that the version of OptiX installed on your system is not compatible with the video driver (the two must satisfy version requirements that are detailed on the OptiX website).
