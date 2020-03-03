Debugging
=========

We generally recommend Mitsuba's ``scalar_*`` variants for tracking down
compilation errors and debugging misbehaving code (see the section on
:ref:`variants <sec-variants>` for details). These variants make minimal use of
nested C++ templates, which reduces the length of compiler error messages and
facilitates the use of debuggers like `LLDB <https://lldb.llvm.org/>`_ or `GDB
<https://www.gnu.org/software/gdb/>`_.

When using a debugger, the stringified versions of vectors and spectra are
needlessly verbose and reveal various private implementation details of the
Enoki library. For instance, printing a simple statically sized 3D vector like
``Vector3f(1, 2, 3)`` in LLDB yields

.. code-block:: text

    $0 = {
      enoki::StaticArrayImpl<float, 3, false, mitsuba::Vector<float, 3>, int> = {
        enoki::StaticArrayImpl<float, 4, false, mitsuba::Vector<float, 3>, int> = {
          m = (1, 2, 3, 0)
        }
      }
    }

Dynamic arrays used in vectorized backends (e.g.
``DynamicArray<Packet<Float>>(1, 2, 3)``) are even worse, as the values are
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

To improve readability, Enoki includes a script that improves GDB and LLDB's
understanding of its types. With this script, both of the above turn into

.. code-block:: text

    $0 = [1, 2, 3]

To install it in LLDB, copy the file ``ext/enoki/resources/enoki_lldb.py`` to
``~/.lldb`` (creating the directory, if not present) and then apppend the
following line to the file ``~/.lldbinit`` (again, creating it if, not already
present):

.. code-block:: text

    command script import ~/.lldb/enoki_lldb.py

To install it in GDB, copy the file ``ext/enoki/resources/enoki_gdb.py`` to
``~/.gdb`` (creating the directory, if not present) and then apppend the
following two lines to the file ``~/.gdbinit`` (again, creating it if, not
already present):

.. code-block:: text

    set print pretty
    source ~/.gdb/enoki_gdb.py
