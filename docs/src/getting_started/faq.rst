.. _sec-faq:

Frequently asked questions
--------------------------

- Compilation fails with the error message: ":monosp:`Error: always_inline
  function <..> requires target feature <..> but would be inlined into function
  <..> that is compiled without support for <..>`".

    You are likely trying to compile Mitsuba within a virtual machine such as
    VirtualBox, QEmu, etc. This is not recommended: most of these virtual
    machines expose "funky" virtualized processors that are missing standard
    instructions extensions, which breaks compilation. You can work around this
    issue by completely disabling vectorization at some cost in performance. In
    this case, add the following line to the top-level :monosp:`CMakeLists.txt`
    file (e.g. in line 4 after the :monosp:`project()` declaration).

    .. code-block:: cmake

        add_definitions(-DENOKI_DISABLE_VECTORIZATION=1)


- Differentiable rendering fails with the error message ":monosp:`Failed to load
  Optix library`".

    It is likely that the version of OptiX installed on your system is not
    compatible with the video driver (the two must satisfy version requirements
    that are detailed on the OptiX website)

- Differentiable rendering fails with an error message similar to ":monosp:`RuntimeError:
  cuda_malloc(): out of memory!`".

    The rendering or autodiff graph exhausted your GPU's memory. Try reducing the sample count
    and / or resolution. If high sample count renders are necessary, it is possible to split computation
    in several low sample count passes and average together the gradients.

- Citing Mitsuba 2 in scientific literature.

  Please use the following BibTeX entry to cite Mitsuba 2 in research articles,
  books, etc.

  .. code-block:: bibtex

      @article{NimierDavidVicini2019Mitsuba2,
          author = {Merlin Nimier-David and Delio Vicini and Tizian Zeltner and Wenzel Jakob},
          title = {Mitsuba 2: A Retargetable Forward and Inverse Renderer},
          journal = {Transactions on Graphics (Proceedings of SIGGRAPH Asia)},
          volume = {38},
          number = {6},
          year = {2019},
          month = dec,
          doi = {10.1145/3355089.3356498}
      }

  Here is a plain-text version of the above:

      Merlin Nimier-David, Delio Vicini, Tizian Zeltner, and Wenzel Jakob.
      2019. Mitsuba 2: A Retargetable Forward and Inverse Renderer. In
      Transactions on Graphics (Proceedings of SIGGRAPH Asia)
      38(6).

  A pre-print of this article is available `here
  <http://rgl.epfl.ch/publications/NimierDavidVicini2019Mitsuba2>`_.
