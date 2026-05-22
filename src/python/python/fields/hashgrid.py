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
