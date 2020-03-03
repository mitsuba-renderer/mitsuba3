.. _sec-devguide:

Introduction
============

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
``camelCase`` everywhere). We've essentially imported Python's `PEP 8
<https://www.python.org/dev/peps/pep-0008>`_ into the C++ side (which does not
specify a recommended naming convention), ensuring that code that uses
functionality from both languages looks natural.


Code structure
--------------

Mitsuba is split into 3 basic support libraries:

* The core library (in :code:`src/libcore`) implements basic functionality such
  as cross-platform file and bitmap I/O, data structures, scheduling, as well
  as logging and plugin management.
* The rendering library (in :code:`src/librender`) contains abstractions needed
  to load and represent scenes containing light sources, shapes, materials, and
  participating media.
* The python library (in :code:`src/python`) contains components of the system
  that are written in Python, and which access Mitsuba through bindings. This
  includes statistical tests (Chi^2, etc.) and tooling for differentiable
  rendering.


