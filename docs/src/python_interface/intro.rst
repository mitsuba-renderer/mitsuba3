Introduction
==============

Mitsuba 2 has extensive Python bindings. All plugins come with Python bindings, which means that it is possible to write vectorized or differentiable integrators entirely in Python.
The bindings can not only be used to write integrators, but also for more specialized applications, where the output is not necessarily an image.
The user can access scene intersection routines, BSDF evaluations and emitters entirely from Python.

We provide several examples of how the Python bindings can be used.
In this part of the documentation, we focus entirely on the use of the bindings for forward rendering applications.
A seperate group of tutorials describes how to use Mitsuba 2 for differentiable rendering.

Moreover, the large automated test suite written in Python is an excellent source of examples.