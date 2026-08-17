.. _sec-python-environments:

Python environments
===================

misuka's build system compiles its Python bindings against one specific
interpreter, and the resulting packages only become importable once the
``setpath`` script in the build directory has been run. Both of these are easy
to get wrong, and both fail in confusing ways. This page shows how to set up an
environment for building misuka and how to wire the two steps together so that
they happen automatically.

If you only want to use misuka with the default variant set and never modify
it, install it from PyPI as described in :doc:`Getting started </index>` and
ignore this page.

.. _sec-installing-from-source:

Installing from source
----------------------

If you need variants that the PyPI package does not ship, but no development
loop, install misuka from a clone of the repository:

.. code-block:: bash

    git clone -b stable --recursive https://github.com/misuka-renderer/misuka
    cd misuka
    pip install .

This compiles misuka and installs it into the active environment like any other
package. There is no ``setpath`` script to run afterwards, so the rest of this
page does not apply to this workflow.

Which variants get compiled is controlled by a :monosp:`misuka.conf` file. The
install above creates it at ``build/<wheel-tag>/misuka.conf``, and every build
after that reads it. Edit the ``"enabled"`` list in it, then install again:

.. code-block:: bash

    pip install .

CMake re-runs whenever :monosp:`misuka.conf` changes, so this picks the edit up
on its own. See :ref:`sec-compiling` for what the variant names mean and which
ones you are likely to want.

That build directory persists between installs, so repeated ``pip install .``
runs only recompile what changed. It is still a heavier loop than the one
described in :ref:`sec-compiling`, which skips the packaging step entirely and
lets edits to misuka's Python sources take effect without any rebuild at all.
Follow that route if you intend to work on the code.

Creating a virtual environment
------------------------------

We **strongly** recommend building misuka from inside a virtual environment.
CMake picks up whichever ``python`` is first on ``PATH`` when the project is
configured, so activating the environment before running CMake is what makes
the build and the interpreter you later use agree on the python version.
Any Python from 3.9 onwards works.

Using ``uv``
^^^^^^^^^^^^

`uv <https://docs.astral.sh/uv/>`_ is the quickest way to get there. It installs
and manages Python versions, creates environments, and ships a package manager
that is considerably faster than ``pip``.

.. tabs::

    .. code-tab:: bash Linux / macOS

        uv venv --prompt misukadev
        source .venv/bin/activate

    .. code-tab:: powershell Windows (PowerShell)

        uv venv --prompt misukadev
        .venv\Scripts\Activate.ps1

    .. code-tab:: batch Windows (cmd)

        uv venv --prompt misukadev
        .venv\Scripts\activate.bat

Pass ``--python 3.14`` to pick a specific interpreter version (in this case 3.14).
``uv`` downloads it if it's not already installed.

Using ``python -m venv``
^^^^^^^^^^^^^^^^^^^^^^^^

The standard library equivalent, using whichever ``python`` is currently on your
``PATH``:

.. tabs::

    .. code-tab:: bash Linux / macOS

        python -m venv --prompt misukadev .venv
        source .venv/bin/activate
        python --version

    .. code-tab:: powershell Windows (PowerShell)

        python -m venv --prompt misukadev .venv
        .venv\Scripts\Activate.ps1
        python --version

    .. code-tab:: batch Windows (cmd)

        python -m venv --prompt misukadev .venv
        .venv\Scripts\activate.bat
        python --version

Create the environment in the misuka root directory. VS Code discovers a
``.venv`` there automatically, and the activation hook below can then find the
build directory without a hardcoded path.

If you place it elsewhere, point VS Code at it manually. Press
:monosp:`Ctrl+Shift+P` (:monosp:`Cmd+Shift+P` on macOS), choose *Python: Select
Interpreter*, then *Enter interpreter path* and navigate to the interpreter
inside the environment. That is ``.venv/bin/python`` on Linux and macOS, and
``.venv\Scripts\python.exe`` on Windows. VS Code remembers this per workspace.

Building from VS Code
^^^^^^^^^^^^^^^^^^^^^

The CMake Tools extension configures the project in whatever environment VS Code
was started in, which is often not the one you want. Rather than launching VS
Code from an activated shell every time, name the interpreter explicitly in the
workspace's ``.vscode/settings.json``:

.. tabs::

    .. code-tab:: json Linux / macOS

        {
            "cmake.configureSettings": {
                "Python_EXECUTABLE": "${workspaceFolder}/.venv/bin/python"
            }
        }

    .. code-tab:: json Windows (PowerShell)

        {
            "cmake.configureSettings": {
                "Python_EXECUTABLE": "${workspaceFolder}/.venv/Scripts/python.exe"
            }
        }

    .. code-tab:: json Windows (cmd)

        {
            "cmake.configureSettings": {
                "Python_EXECUTABLE": "${workspaceFolder}/.venv/Scripts/python.exe"
            }
        }

Use an absolute path instead of ``${workspaceFolder}`` if the environment does
not live in the misuka root directory. See :ref:`sec-compiling` for what goes
wrong when the build and the interpreter disagree.

Running :monosp:`setpath` automatically
----------------------------------------

After compiling, the ``setpath`` script in the build directory has to be run in
every new shell before the python package ``misuka`` can be imported.
Skipping it produces a ``ModuleNotFoundError: No module named 'misuka'``, or
the same error for ``drjit``, even though both were built successfully.

Which script you need depends on the platform, and on Windows also on the
generator. The Visual Studio generator places the working scripts in a
``Release`` subdirectory of ``build``, while a Ninja build puts them directly in
``build``. The hooks below check both locations.

Rather than remembering to run this by hand, add it to your environment's
activation script.

Virtual environments (``uv``/``venv``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Append the following to the activation script inside your environment. It
derives the misuka root from the location of the activation script itself, which
assumes that the environment lives in the misuka root directory.

.. tabs::

    .. group-tab:: Linux / macOS

        Append to ``.venv/bin/activate``:

        .. code-block:: bash

            # Source setpath.sh. misuka_root is derived from this script's own location.
            misuka_root="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/../.." && pwd)"

            setpath_file="$misuka_root/build/setpath.sh"
            if [ ! -f "$setpath_file" ]; then
                echo "Error: Could not find setpath.sh at $setpath_file" >&2
                return 1
            fi

            source "$setpath_file"
            echo "Sourced $setpath_file"

        If your environment lives somewhere else, replace the first line with
        the actual path, for example ``misuka_root="$HOME/code/misuka"``.

    .. group-tab:: Windows (PowerShell)

        Append to ``.venv\Scripts\Activate.ps1``:

        .. code-block:: powershell

            # Run setpath.ps1. $misuka_root is derived from this script's own location.
            $misuka_root = (Resolve-Path "$PSScriptRoot\..\..").Path

            $setpath_file = "$misuka_root\build\Release\setpath.ps1"
            if (-not (Test-Path $setpath_file)) {
                $setpath_file = "$misuka_root\build\setpath.ps1"
            }

            if (Test-Path $setpath_file) {
                . $setpath_file
                Write-Host "Sourced $setpath_file"
            } else {
                Write-Error "Could not find setpath.ps1 under $misuka_root\build"
            }

        If your environment lives somewhere else, replace the first line with
        the actual path, for example
        ``$misuka_root = "$env:USERPROFILE\code\misuka"``.

    .. group-tab:: Windows (cmd)

        Append to ``.venv\Scripts\activate.bat``:

        .. code-block:: bat

            REM Run setpath.bat. misuka_root is derived from this script's own location.
            set "misuka_root=%~dp0..\.."

            set "setpath_file=%misuka_root%\build\Release\setpath.bat"
            if not exist "%setpath_file%" set "setpath_file=%misuka_root%\build\setpath.bat"

            if exist "%setpath_file%" (
                call "%setpath_file%"
                echo Sourced %setpath_file%
            ) else (
                echo Error: Could not find setpath.bat under %misuka_root%\build 1>&2
            )

        If your environment lives somewhere else, replace the first line with
        the actual path, for example
        ``set "misuka_root=%USERPROFILE%\code\misuka"``.

Conda environments
^^^^^^^^^^^^^^^^^^

Conda runs every script it finds in the environment's ``etc/conda/activate.d``
directory when that environment is activated. Activate the environment once so
that the ``CONDA_PREFIX`` variable is defined, then create the directory:

.. tabs::

    .. code-tab:: bash Linux / macOS

        conda activate misukadev
        mkdir -p "$CONDA_PREFIX/etc/conda/activate.d"

    .. code-tab:: powershell Windows (PowerShell)

        conda activate misukadev
        mkdir "$env:CONDA_PREFIX\etc\conda\activate.d"

    .. code-tab:: batch Windows (cmd)

        conda activate misukadev
        mkdir "%CONDA_PREFIX%\etc\conda\activate.d"

Name the script after the environment so that it stays recognisable, and give it
the following contents:

.. tabs::

    .. group-tab:: Linux / macOS

        Save as ``$CONDA_PREFIX/etc/conda/activate.d/misukadev-activate.sh``:

        .. code-block:: bash

            #!/usr/bin/env bash

            # Change misuka_root to the root of the misuka project.
            misuka_root="$HOME/code/misuka"

            setpath_file="$misuka_root/build/setpath.sh"
            if [ ! -f "$setpath_file" ]; then
                echo "Error: Could not find setpath.sh at $setpath_file" >&2
                return 1
            fi

            source "$setpath_file"
            echo "Sourced $setpath_file"

        Finally, make it executable:

        .. code-block:: bash

            chmod +x "$CONDA_PREFIX/etc/conda/activate.d/misukadev-activate.sh"

    .. group-tab:: Windows (PowerShell)

        Conda runs ``.bat`` scripts from ``activate.d`` on Windows, whichever
        shell you activate from. Save the following as
        ``%CONDA_PREFIX%\etc\conda\activate.d\misukadev-activate.bat``:

        .. code-block:: bat

            @echo off

            REM Change misuka_root to the root of the misuka project.
            set "misuka_root=%USERPROFILE%\code\misuka"

            set "setpath_file=%misuka_root%\build\Release\setpath.bat"
            if not exist "%setpath_file%" set "setpath_file=%misuka_root%\build\setpath.bat"

            if not exist "%setpath_file%" (
                echo Error: Could not find setpath.bat under %misuka_root%\build 1>&2
                exit /b 1
            )

            call "%setpath_file%"
            echo Sourced %setpath_file%

        No equivalent of ``chmod`` is needed.

    .. group-tab:: Windows (cmd)

        Conda runs ``.bat`` scripts from ``activate.d`` on Windows, whichever
        shell you activate from. Save the following as
        ``%CONDA_PREFIX%\etc\conda\activate.d\misukadev-activate.bat``:

        .. code-block:: bat

            @echo off

            REM Change misuka_root to the root of the misuka project.
            set "misuka_root=%USERPROFILE%\code\misuka"

            set "setpath_file=%misuka_root%\build\Release\setpath.bat"
            if not exist "%setpath_file%" set "setpath_file=%misuka_root%\build\setpath.bat"

            if not exist "%setpath_file%" (
                echo Error: Could not find setpath.bat under %misuka_root%\build 1>&2
                exit /b 1
            )

            call "%setpath_file%"
            echo Sourced %setpath_file%

        No equivalent of ``chmod`` is needed.
