.. _sec-fields:

Fields
======

This section describes generic field data sources. Fields provide spatially
varying or learned data that can be requested by plugins through a more specific
role such as :paramtype:`texture` or :paramtype:`volume`, or used directly by
plugins that operate on generic field outputs.

The XML scene format supports explicit ``<field>`` declarations for plugins
that request generic field data. Existing ``<texture>`` and ``<volume>`` tags
remain the preferred spelling for surface and volume role parameters. These
tags validate a field for the requested role; they do not produce wrapper
objects or change its runtime type.

``Field`` is the sole Python plugin interface for these data sources. For
example, both ``mi.load_dict({"type": "bitmap", ...})`` and
``mi.load_dict({"type": "gridvolume", ...})`` return an ``mi.Field``.
Python implementations derive from ``mi.Field`` and are registered using
``mi.register_field()``. The former ``mi.Texture``, ``mi.TexturePtr``, and
``mi.Volume`` classes and ``mi.register_texture()`` function are no longer
part of the Python API.

An interaction-domain Python field can be queried directly. The renderer's
volume role currently requires a native ``VolumeField`` implementation because
it additionally needs bounds and majorant metadata.
