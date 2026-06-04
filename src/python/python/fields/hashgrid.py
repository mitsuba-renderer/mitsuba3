r"""
.. _field-hashgridfield:

Hash-grid field (:monosp:`hashgridfield`)
-----------------------------------------

.. pluginparameters::

 * - input_dim
   - |int|
   - Number of coordinates encoded from the interaction record. A value of 2
     uses surface UV coordinates, while 3 uses position. (Default: 2)

 * - n_levels
   - |int|
   - Number of multiresolution hash-grid levels. (Default: 16)

 * - n_features_per_level
   - |int|
   - Number of feature channels stored at each level. (Default: 2)

 * - hashmap_size
   - |int|
   - Number of entries in the hash table. (Default: 524288)

 * - base_resolution
   - |int|
   - Resolution of the coarsest level. (Default: 16)

 * - per_level_scale
   - |float|
   - Resolution multiplier between consecutive levels. (Default: 2)

 * - precision
   - |string|
   - Storage precision for the encoding parameters, either ``fp16`` or
     ``fp32``. (Default: ``fp16``)

This JIT-only field wraps Dr.Jit's hash-grid encoding and returns
``Features[n_levels * n_features_per_level]``. It is intended for direct field
composition, usually as the ``encoding`` child of :monosp:`neuralfield`.

"""

from __future__ import annotations # Delayed parsing of type annotations

import drjit.nn as nn

from .common import _make_drjit_feature_field


def _make_hashgrid_field(mi):
    return _make_drjit_feature_field(
        mi,
        plugin_name="hashgridfield",
        encoding_cls=nn.HashGridEncoding,
        class_name="HashGridField",
        torchngp_compat=True,
    )
