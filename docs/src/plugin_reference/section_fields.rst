.. _sec-fields:

Fields
======

This section describes generic field data sources. Fields provide spatially
varying or learned data that can be requested by plugins through a more specific
role such as :paramtype:`texture` or :paramtype:`volume`, or used directly by
plugins that operate on generic field outputs.

The XML scene format supports explicit ``<field>`` declarations for plugins
that request generic field data. Existing ``<texture>`` and ``<volume>`` tags
remain the preferred spelling for surface and volume role parameters.
