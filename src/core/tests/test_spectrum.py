import pytest
import drjit as dr
import mitsuba as mi

import numpy as np
import os

def test01_roundtrip_file_text(variant_scalar_rgb, tmpdir):
    wav = np.array([1.567, 100.123, 1000.98, 10000.33])
    val = np.array([5.2, 500.5, 5000.8, 50000.54])

    tmp_file = os.path.join(str(tmpdir), "test_spectrum.spd")
    mi.spectrum_to_file(tmp_file, wav, val)

    wav2, val2 = mi.spectrum_from_file(tmp_file)
    assert dr.allclose(wav, wav2)
    assert dr.allclose(val, val2)

def test02_roundtrip_file_binary(variant_scalar_rgb, tmpdir):
    wav = np.array([1.567, 100.123, 1000.98, 10000.33])
    val = np.array([5.2, 500.5, 5000.8, 50000.54])

    tmp_file = os.path.join(str(tmpdir), "test_spectrum.spb")
    mi.spectrum_to_file(tmp_file, wav, val)

    wav2, val2 = mi.spectrum_from_file(tmp_file)
    assert dr.allclose(wav, wav2)
    assert dr.allclose(val, val2)

def test03_format_not_valid(variant_scalar_rgb, tmpdir):
    wav = [2.0, 4.0]
    val = [1.0, 2.0]
    tmp_file = os.path.join(str(tmpdir), "test_spectrum.spf")
    with pytest.raises(RuntimeError):
        mi.spectrum_to_file(tmp_file, wav, val)
    with pytest.raises(RuntimeError):
        _, _ = mi.spectrum_from_file(tmp_file)

