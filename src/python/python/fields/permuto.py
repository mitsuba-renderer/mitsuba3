r"""
.. _field-permutofield:

Permutohedral field (:monosp:`permutofield`)
--------------------------------------------

.. pluginparameters::

 * - input_dim
   - |int|
   - Number of coordinates encoded from the interaction record. A value of 2
     uses surface UV coordinates, while 3 uses position. (Default: 2)

 * - n_levels
   - |int|
   - Number of multiresolution permutohedral levels. (Default: 16)

 * - n_features_per_level
   - |int|
   - Number of feature channels stored at each level. (Default: 2)

 * - hashmap_size
   - |int|
   - Number of entries in the parameter table. (Default: 524288)

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

This JIT-only field wraps Dr.Jit's permutohedral encoding and returns
``Features[n_levels * n_features_per_level]``. It is intended for direct field
composition, usually as the ``encoding`` child of :monosp:`neuralfield`.

"""

from __future__ import annotations # Delayed parsing of type annotations

import drjit.nn as nn

from .common import _make_drjit_feature_field


def _make_permuto_field(mi):
    return _make_drjit_feature_field(
        mi,
        plugin_name="permutofield",
        encoding_cls=nn.PermutoEncoding,
        class_name="PermutoField",
    )
