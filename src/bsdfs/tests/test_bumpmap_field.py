import pytest
import mitsuba as mi


def test01_bumpmap_rejects_non_scalar_direct_field_children(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match="BumpMap texture|Texture role|Features"):
        mi.load_dict({
            "type": "bumpmap",
            "height": {
                "type": "sinusoidalfield",
                "input_dim": 2,
                "out_dim": 8,
            },
            "bsdf": {
                "type": "diffuse",
            },
        })
