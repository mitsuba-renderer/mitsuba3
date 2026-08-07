:tocdepth: 3
.. _sec-api:

.. image:: ../resources/data/docs/images/banners/banner_07.jpg
    :width: 100%
    :align: center


API reference
=============

Overview
--------

This reference is generated from Mitsuba's type stubs, which ship inside the
``mitsuba`` wheel. It therefore describes the Python API exactly as a type
checker or an editor sees it: real signatures, generic ``drjit.auto`` types
rather than whichever backend a particular build used, and one entry per
overload.

Mitsuba's bindings mirror the C++ API closely, so this should be useful to a
C++ developer as well. Where the two differ -- a C++ out-parameter that the
binding returns instead, or a template parameter that has no Python
counterpart -- the Python behaviour is what is documented here.

.. include:: ../generated/api/toctree.txt
