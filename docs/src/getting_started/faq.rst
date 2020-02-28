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
    ssue by completely disabling vectorization at some cost in performance. In
    this case, add the following line to the top-level :monosp:`CMakeLists.txt`
    file (e.g. in line 4 after the :monosp:`project()` declaration).

    .. code-block:: cmake

        add_definitions(-DENOKI_DISABLE_VECTORIZATION=1)


- Differentiable rendering fails with the error message ":monosp:`Failed to load
  Optix library`".

    It is likely that the version of OptiX installed on your system is not
    compatible with the video driver (the two must satisfy version requirements
    that are detailed on the OptiX website)
