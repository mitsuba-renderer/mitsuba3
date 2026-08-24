.. _sec-compiling:

Compiling the system
====================

Cloning the repository
----------------------

Compiling misuka from scratch requires recent versions of CMake (at least
**3.9.0**) and Python (at least **3.9**). Further platform-specific dependencies
and compilation instructions are provided below for each operating system. Some
additional steps are required for GPU-based backends that are described at the
end of this section.

misuka depends on several external dependencies, and its repository directly
refers to specific versions of them using a Git feature called *submodules*.
Cloning misuka's repository will recursively fetch these dependencies, which
are subsequently compiled using a single unified build system. This dramatically
reduces the number steps needed to set up the renderer compared to previous
versions of Mitsuba.

Most of misuka's active development happens on the ``master`` Git branch. We
therefore recommend using the ``stable`` branch which points to the most recent
release.

For all of this to work out properly, you will have to specify the
``--recursive`` flag when cloning the repository:

.. code-block:: bash

    git clone -b stable --recursive https://github.com/misuka-renderer/misuka

If you already cloned the repository and forgot to specify this flag, it's
possible to fix the repository in retrospect using the following command:

.. code-block:: bash

    git submodule update --init --recursive

Staying up-to-date
^^^^^^^^^^^^^^^^^^

Unfortunately, pulling from the main repository won't automatically keep the
submodules in sync, which can lead to various problems. The following command
installs a git alias named ``pullall`` that automates these two steps.

.. code-block:: bash

    git config --global alias.pullall '!f(){ git pull "$@" && git submodule update --init --recursive; }; f'

Afterwards, simply write

.. code-block:: bash

    git pullall

to fetch the latest version of misuka.

Switching between branches
^^^^^^^^^^^^^^^^^^^^^^^^^^

Switching between branches whose submodule versions differ needs one extra step.

.. code-block:: bash

    git checkout <branch>
    git submodule update --init --recursive

This updates the submodules to the versions the new branch expects. If you
instead see ``error: pathspec '...' did not match any file(s) known to git``, a
submodule was renamed in the new branch. Run ``git status``, delete any
untracked submodule directories it reports, then run the update again. A second
``git status`` should now come back clean.

Setting up a Python environment
-------------------------------

Build misuka from inside a virtual environment, and activate that environment
*before* configuring the project with CMake. See :ref:`sec-python-environments`
for how to create one and how to make ``setpath.sh`` load automatically.

.. warning::

    CMake compiles the extension modules against whichever ``python`` is first
    on ``PATH`` at configure time. If the environment is not active at compile
    time, CMake finds the system interpreter instead, and the resulting binaries
    will not load under the interpreter you actually use. The symptom is an
    import error naming ``_drjit_ext`` when you run ``import misuka``, which does not point
    at the real cause.

    Check the Python version CMake reports while configuring. If it is the wrong
    one, activate the environment, delete the contents of the build directory,
    and configure again. In VS Code you can also run
    ``CMake: Delete Cache and Reconfigure`` from the command palette.

Configuring :monosp:`misuka.conf`
----------------------------------

misuka variants are specified in the file :monosp:`misuka.conf`. This file
can be found in the build directory and will be created when executing CMake the
first time.

Open :monosp:`misuka.conf` in your favorite text editor and scroll down to the
declaration of the enabled variants (around line 86):

.. code-block:: text

    "enabled": [
        "scalar_rgb", "scalar_acoustic", "llvm_ad_rgb", "llvm_ad_acoustic"
    ],

The default file specifies a set of that you may wish to extend
according to your requirements and the explanations given above. Note that
``scalar_rgb`` *must* currently be part of the list as some core components of
Mitsuba depend on it, and at least one ``ad``-enabled variant must also be
compiled. When the ``mitsuba`` command line executable is launched without a
specific mode parameter, it will automatically select the most capable variant
whose backend is available at runtime (preferring an RGB color representation).

**Acoustic rendering requires an** ``*_acoustic`` **variant**. The example
above enables ``scalar_acoustic`` and ``llvm_ad_acoustic``. Without one of
these (or another ``*_acoustic`` variant) in the enabled list, acoustic scenes
cannot be rendered, since the stock RGB/spectral variants use
``Spectrum<Float, 3>``/``Spectrum<Float, N>`` color representations rather than
the single-channel energy representation acoustic rendering needs. See
:ref:`sec-acoustic-rendering` for background on the ``acoustic`` variant family.

The remainder of this file lists the C++ types defining the available variants
and can safely be ignored.

TLDR: If you plan to use misuka from Python, we recommend adding the
following variants for differentiable rendering:

* If you have a CUDA-capable GPU: ``cuda_ad_acoustic`` and ``cuda_ad_rgb``.
* If you have an Apple Silicon GPU: ``metal_ad_acoustic`` and ``metal_ad_rgb``.
* If you have neither: ``llvm_ad_acoustic`` and ``llvm_ad_rgb``.

.. warning::

    Note that compilation time and compilation memory usage is roughly
    proportional to the number of enabled variants, hence including many of them
    (more than five) may not be advisable. Also note that the ``scalar_rgb``
    and *at least one AD variant* is mandatory.

.. warning::

    misuka also generates corresponding
    `Python stub files <https://typing.readthedocs.io/en/latest/spec/distributing.html#stub-files>`_
    during compilation. The process involves selecting one of the available variants
    to extract the relevant type information. However, these stub files have to
    be variant-agnostic and hence certain combinations of variants won't be allowed.
    For example, including just `scalar_rgb`, `scalar_spectral` and `llvm_ad_rgb`
    creates ambiguity as to which variant we should select to generate the Python stubs.
    In short, if a disallowed combination of variants is selected, a compilation
    error will report what variant should be added to remove any ambiguity.

.. _sec-compiling-linux:

Linux
-----

The build process under Linux requires several external dependencies that are
easily installed using the system-provided package manager (e.g.,
:monosp:`apt-get` under Ubuntu).

To fetch all dependencies, enter the following commands on Ubuntu:

.. code-block:: bash

    # Install required build tools
    sudo apt install g++ cmake ninja-build

    # Install libraries for image I/O
    sudo apt install libpng-dev libjpeg-dev nasm

    # Install required Python packages
    sudo apt install libpython3-dev python3-distutils

Additional packages are required to run the included test suite or to generate
HTML documentation (see :ref:`Developer guide <sec-writing-documentation>`). If those are
interesting to you, also enter the following commands:

.. code-block:: bash

    # For running tests
    sudo apt install python3-pytest python3-pytest-xdist python3-numpy

Now, compilation should be as simple as running the following from
inside the :monosp:`misuka` root directory:

.. code-block:: bash

    # Create a directory where build products are stored
    mkdir build
    cd build
    cmake -GNinja ..
    ninja


**Tested version**

The above procedure will likely work on many different flavors of Linux (with
slight adjustments for the package manager and package names). We have mainly
worked with software environments listed below, and our instructions should work
without modifications in those cases.

.. tabularcolumns:: |p{0.49\width}|p{0.49\width}|

+--------------------------+--------------------------+
| **Noble**                | **Arch**                 |
|                          |                          |
| - Ubuntu 24.04           | - Omarchy 4.0.0          |
| - g++ 13                 | - clang 22.1.8           |
| - LLVM 18.1.8            | - LLVM 22.1.8            |
| - cmake 4.4.2            | - cmake 4.4.2            |
| - ninja 1.13.0           | - ninja 1.13.2           |
| - python 3.14.3          | - python 3.14.7          |
+--------------------------+--------------------------+

.. _sec-compiling-windows:

Windows
-------

On Windows, a recent version of `Visual Studio
<https://visualstudio.microsoft.com/vs/>`_ is required; 2022 and 2026 both work.
The Community Edition is free and sufficient. The following components have to
be selected in the Visual Studio installer:

* *Desktop development with C++*
* *MSVC* build tools for x64/x86
* *C++ CMake tools for Windows*

The last of these provides CMake and `Ninja <https://ninja-build.org/>`_. Some
tools such as git or Python might still need to be installed manually. misuka's
build system *requires* access to Python >= 3.9 even if you do not plan to use
misuka's python interface.

Ninja needs the MSVC toolchain on ``PATH``, so run the build from a *Developer
Command Prompt* or a *Developer PowerShell* for your Visual Studio version
rather than a plain shell. Both are installed alongside Visual Studio. From the
root `misuka` directory:

.. code-block:: bash

    mkdir build
    cd build
    cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
    ninja

This matches the Linux and macOS instructions above, and places the
``setpath`` scripts directly in ``build``.

.. .. note::

..    The ``llvm_*`` variants currently abort on Windows: LLVM's RuntimeDyld does
..    not resolve the COFF constant-pool symbols the JIT emits, so every kernel
..    fails to materialise. Applying ``.github/patches/drjit-core-elf-triple.patch``
..    to ``ext/drjit/ext/drjit-core`` before configuring makes the JIT emit ELF
..    instead and works around it. See `issue #22
..    <https://github.com/misuka-renderer/misuka/issues/22>`_.

Building from Visual Studio
^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you would rather work inside the IDE, configure the Visual Studio generator
instead:

.. code-block:: bash

    # To be safe, explicitly ask for the 64 bit version of Visual Studio

    # Visual Studio 2022
    cmake -G "Visual Studio 17 2022" -A x64 -B build

    # Visual Studio 2026 (requires CMake 4.2 or newer)
    cmake -G "Visual Studio 18 2026" -A x64 -B build

Naming a generator that is not installed fails with ``could not find any
instance of Visual Studio``.

Afterwards, open the generated ``mitsuba.sln`` file in the build folder and
proceed building as usual from within Visual Studio. You will probably also
want to set the build mode to *Release* there.

It is also possible to directly build from the terminal running the following
command:

.. code-block:: bash

    cmake --build build --config Release

Note that this generator is multi-config: the build mode is chosen per build
rather than at configure time, and the working ``setpath`` scripts end up in a
``Release`` subdirectory of ``build``.


**Tested version**

This is the configuration exercised by the Windows CI job on every pull request.

* Windows Server 2025
* Visual Studio 18 2026
* MSVC toolset 14.51.36231
* LLVM 18.1.8
* cmake 4.4.2
* ninja 1.13.0
* git 2.55.0
* Python 3.14.3


.. _sec-compiling-macos:

macOS
-----

On macOS, you will need to install Xcode, CMake, and `Ninja
<https://ninja-build.org/>`_. Additionally, running the Xcode command line tools
once might be necessary:

.. code-block:: bash

    xcode-select --install

Note that the default Python version installed with macOS is not compatible with
misuka, and a more recent version (at least 3.9) needs to be installed (e.g.
via `Miniconda 3 <https://docs.conda.io/en/latest/miniconda.html>`_ or `Homebrew
<https://brew.sh/>`_).

Now, compilation should be as simple as running the following from inside the
`misuka` root directory:

.. code-block:: bash

    mkdir build
    cd build
    cmake -GNinja ..
    ninja


**Tested version**

* macOS 26.5.2
* Xcode 26.6
* AppleClang 21.0.0
* LLVM 22.1.7
* cmake 4.3.0
* ninja 1.12.1
* Python 3.14.3

The ``Noble`` column above describes a GitHub Actions runner. The workflows in
``.github/workflows`` build misuka on Ubuntu 24.04, macOS 26 (Apple Silicon) and
Windows with the same variants (``scalar_rgb``, ``scalar_acoustic``,
``llvm_ad_rgb``, ``llvm_ad_acoustic``) and run the test suite with
``pytest -m 'not slow'``. The compiler is the one shipped by the runner image and
cannot be pinned; every other tool version is fixed by the workflow.


After compiling
---------------

Once misuka is compiled, run the ``setpath.sh/.bat/.ps1`` script in your build
directory to configure environment variables (``PATH/PYTHONPATH``) that are
required to run misuka.

.. code-block:: bash

    # On Linux / Mac OS
    source setpath.sh

    # On Windows, Ninja build (cmd)
    C:/.../misuka/build> setpath

    # On Windows, Ninja build (powershell)
    C:/.../misuka/build> .\setpath.ps1

    # On Windows, Visual Studio build (cmd)
    C:/.../misuka/build/Release> setpath

    # On Windows, Visual Studio build (powershell)
    C:/.../misuka/build/Release> .\setpath.ps1

.. note::

    On Windows, the ``Release`` in those paths only applies to builds made with
    the Visual Studio generator, which writes the working scripts into that
    subdirectory. A Ninja build puts them directly in ``build``. Running the
    wrong one fails silently, and the symptom appears later as a
    ``ModuleNotFoundError`` for ``drjit`` or an unrecognised ``misuka``
    command.

Sourcing this script in every new shell gets old quickly. See
:ref:`sec-python-environments` for how to run it automatically whenever your
Python environment is activated.

The renderer is then ready to use. See :ref:`sec-runtime-requirements` for what
the LLVM, CUDA, and Metal backends expect to find on your machine at runtime.

Embree
------

By default, misuka's ``scalar`` and ``llvm`` backends use Intel's Embree
library for ray tracing instead of the builtin kd-tree. To change
this behavior, invoke CMake with the ``-DMI_ENABLE_EMBREE=0`` parameter
or use a visual CMake tool like ``cmake-gui`` or ``ccmake`` to flip the value of
this parameter. Embree tends to be faster but lacks some features such as
support for double precision ray intersection.

OptiX
-----

By default, misuka is also able to resolve the OptiX API itself, and therefore
does not rely on the ``optix.h`` header file. The ``MI_USE_OPTIX_HEADERS`` CMake
flag can be used to turn off this feature if a developer wants to experiment
with parts of the OptiX API not yet exposed to the framework.
