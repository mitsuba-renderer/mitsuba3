Getting Started
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
    C:\...\mitsuba2> setpath


Mitsuba can then be used to render scenes by typing

.. code-block:: bash

    mitsuba scene.xml

where ``scene.xml`` is a Mitsuba scene file. Calling ``mitsuba --help`` will print additional information about various command line arguments.


Running the tests
-----------------
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


Development
--------------
New developers will want to begin by *thoroughly* reading the documentation of
`Enoki <https://enoki.readthedocs.io/en/master/index.html>`_ before looking at
any Mitsuba code. Enoki is a template library for vector and matrix arithmetic
that constitutes the foundation of Mitsuba 2. It also drives the code
transformations that enable systematic vectorization and automatic
differentiation of the renderer.

Mitsuba 2 is a completely new codebase, and existing Mitsuba 0.6 plugins will
require significant changes to be compatible with the architecture of the new
system. Apart from differences in the overall architecture, a superficial
change is that Mitsuba 2 code uses an ``underscore_case`` naming convention for
function names and variables (in contrast to Mitsuba 0.4, which used
``camelCase`` everywhere). We've essentially imported Python's `PEP
8 <https://www.python.org/dev/peps/pep-0008>`_ into the C++ side (which does not
specify a recommended naming convention), ensuring that code that uses
functionality from both languages looks natural.

Debugging
---------
When debugging Mitsuba using `LLDB
<https://lldb.llvm.org/>`_, the stringified versions of vectors and spectra are
needlessly verbose and reveal various private implementation details of the
Enoki library. For instance, printing a simple statically sized 3D vector like
``Array<float, 3>(1, 2, 3)`` yields

.. code-block:: text

    $0 = {
      enoki::StaticArrayImpl<float, 3, false, mitsuba::Vector<float, 3>, int> = {
        enoki::StaticArrayImpl<float, 4, false, mitsuba::Vector<float, 3>, int> = {
          m = (1, 2, 3, 0)
        }
      }
    }

Dynamic arrays (e.g. ``FloatX(1, 2, 3)``) are even worse, as the values are
obscured behind a pointer:

.. code-block:: text

    $0 = {
      enoki::DynamicArrayImpl<enoki::Packet<float, 8>, enoki::DynamicArray<enoki::Packet<float, 8> > > = summary {
        m_packets = {
          __ptr_ = {
            std::__1::__compressed_pair_elem<enoki::Packet<float, 8> *, 0, false> = {
              __value_ = 0x0000000100300200
            }
          }
        }
        m_size = 1
        m_packets_allocated = 1
      }
    }

To improve readability, Enoki includes a script that improves LLDB's
understanding of its types. With this script, both of the above turn into

.. code-block:: text

    $0 = [1, 2, 3]

To install it, copy the file ``ext/enoki/resources/enoki_lldb.py`` to ``~/.lldb``
(creating the directory, if not present) and then apppend the following line to
the file ``~/.lldbinit`` (again, creating it if, not already present):

.. code-block:: text

   command script import ~/.lldb/enoki_lldb.py
