import pytest
import drjit as dr
import mitsuba as mi

import numpy as np
import os

def test01_roundtrip_file_text(variant_scalar_rgb, np_rng, tmpdir):
    wav = list(np_rng.random(48))
    val = list(np_rng.random(48))

    tmp_file = os.path.join(str(tmpdir), "test_spectrum.spd")
    mi.spectrum_to_file(tmp_file, wav, val)

    wav2, val2 = mi.spectrum_from_file(tmp_file)
    assert dr.allclose(wav, wav2)
    assert dr.allclose(val, val2)

def test02_format_not_valid(variant_scalar_rgb, tmpdir):
    wav = [2.0, 4.0]
    val = [1.0, 2.0]
    tmp_file = os.path.join(str(tmpdir), "test_spectrum.spf")
    with pytest.raises(RuntimeError):
        mi.spectrum_to_file(tmp_file, wav, val)
    with pytest.raises(RuntimeError):
        _, _ = mi.spectrum_from_file(tmp_file)

