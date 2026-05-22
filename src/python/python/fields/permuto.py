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
