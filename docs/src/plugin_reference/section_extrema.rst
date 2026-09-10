.. _sec-extremum:

Extremum Structures
===================

This section covers the different types of extremum structures included with
Mitsuba. These plugins store local majorant/minorant bounds of a medium's
extinction coefficient and are used by tracking-based integrators (e.g.
:ref:`volpath <integrator-volpath>`) to perform delta/ratio tracking with
locally-adaptive majorants instead of a single global majorant.

