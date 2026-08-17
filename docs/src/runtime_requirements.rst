.. _sec-runtime-requirements:

Runtime requirements
====================

misuka renders on the CPU through LLVM and on the GPU through CUDA or Metal.
All three are loaded from your system at runtime rather than shipped with
misuka, so each one has to be present before the matching variants can be used.
This page covers what to install and, where it is needed, how to point misuka
at it. It applies to the wheels from PyPI and to a build from source alike.

If you built misuka yourself, source the ``setpath`` script in your build
directory first. See :ref:`sec-compiling`.

.. _sec-gpu-variants:

GPU variants
------------

Variants of misuka that run on NVIDIA GPUs with CUDA support (e.g.
:monosp:`cuda_ad_acoustic`, :monosp:`cuda_ad_rgb`, etc.) will try to
dynamically load the CUDA shared libraries from your system. There is no need
to manually install any specific version of CUDA.
Make sure to have an up-to-date GPU driver if the framework fails to compile
the CUDA variants of misuka. The minimum requirement is currently v535.

.. _optix-wsl2:

Under WSL 2, the OptiX libraries need some extra setup.
Please refer to the instructions in the
:external+mitsuba:doc:`Mitsuba 3 documentation <src/optix_setup>`.

The ``metal`` variants run on Apple Silicon GPUs through the Metal framework,
which is part of macOS. There is nothing to install here, but two conditions
have to be met: the GPU has to support the Metal 3 feature set, which every M1
or newer chip does, and macOS has to be at version 15.0 or newer, because
Dr.Jit emits Metal Shading Language 3.2.

.. _sec-llvm-variants:

LLVM variants
-------------

The ``llvm`` variants render on the CPU across all available cores.
They run on every supported platform, which makes them the option to reach for
when no compatible GPU is available.

Dr.Jit loads the LLVM shared library at runtime. misuka installs and compiles
without LLVM present, so a missing library only shows up when you render:

.. code-block:: text

    jit_init_thread_state(): the LLVM backend is inactive because the LLVM
    shared library ("libLLVM.so") could not be found! Set the
    DRJIT_LIBLLVM_PATH environment variable to specify its path.

The library is named ``libLLVM.so`` on Linux, ``libLLVM.dylib`` on macOS, and
``LLVM-C.dll`` on Windows, and the message names whichever applies to you.

LLVM 15 or newer is required.
These are the installation options we recommend, but any installation is fine
as long as Dr.Jit can find the shared library.

.. tabs::

    .. code-tab:: bash Linux

        # whichever applies to your distribution
        sudo apt install llvm        # Debian / Ubuntu
        sudo dnf install llvm-libs   # Fedora
        sudo pacman -S llvm-libs     # Arch

    .. code-tab:: bash macOS

        brew install llvm

    .. code-tab:: powershell Windows

        winget install -e --id LLVM.LLVM

Dr.Jit first searches the default library path. On Linux and macOS it then
falls back to a fixed pattern covering the directories the commands above
install into, ``/usr/lib/x86_64-linux-gnu`` and friends on Linux, and
``/opt/homebrew/Cellar/llvm/*`` or ``/usr/local/Cellar/llvm/*`` on macOS. A
current install through those commands is therefore picked up on its own. On
Windows there is no such fallback, and the library has to sit somewhere on
``PATH``.

Anything else has to be pointed at explicitly through the
``DRJIT_LIBLLVM_PATH`` environment variable, which takes precedence over the
whole search. Cases that need it:

* a versioned Homebrew formula, since ``llvm@18`` installs into
  ``Cellar/llvm@18`` and the pattern above only matches the unversioned formula
* a custom prefix or a Conda environment
* a Windows installation that is not on ``PATH``
* an installation that is found but too old, where the variable points at a
  newer library instead

.. tabs::

    .. code-tab:: bash Linux

        # add to ~/.bashrc, then restart the shell
        export DRJIT_LIBLLVM_PATH="$CONDA_PREFIX/lib/libLLVM.so"

    .. code-tab:: bash macOS

        # add to ~/.zshrc, then restart the shell
        export DRJIT_LIBLLVM_PATH="$(brew --prefix llvm@18)/lib/libLLVM.dylib"

    .. code-tab:: powershell Windows

        # persists for future shells
        setx DRJIT_LIBLLVM_PATH "C:\Program Files\LLVM\bin\LLVM-C.dll"

Adjust the paths to match your installation.

.. note::
    On macOS, `Homebrew <https://brew.sh/>`_ suggests setting the ``LDFLAGS`` or ``CPPFLAGS`` after
    installation. There is no need to do that for misuka.

.. note::

    ``DRJIT_LIBLLVM_PATH`` is unrelated to the ``LLVM_DIR_PATH`` and
    ``LIBCLANG_PATH`` variables described in :ref:`sec-writing-documentation`.
    Those affect docstring extraction at build time rather than the Dr.Jit
    backend at runtime.