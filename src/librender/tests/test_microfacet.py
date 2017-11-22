import numpy as np
import pytest

from mitsuba.core.xml import load_string
from mitsuba.render import MicrofacetDistributionf

def test01_construct():
    md = MicrofacetDistributionf(MicrofacetDistributionf.EBeckmann, 0.1)
    assert md is not None
    assert md.type() == MicrofacetDistributionf.EBeckmann
    assert np.allclose(md.alpha_u(), 0.1)
    assert np.allclose(md.alpha_v(), 0.1)

def test02_isotropic():
    md = MicrofacetDistributionf(MicrofacetDistributionf.EBeckmann, 0.1)
    assert md.is_isotropic()
    md = MicrofacetDistributionf(MicrofacetDistributionf.EBeckmann, 0.1, 0.1)
    assert md.is_isotropic()
    md = MicrofacetDistributionf(MicrofacetDistributionf.EBeckmann, 0.1, 0.2)
    assert md.is_anisotropic()

def test03_scale_alpha():
    md = MicrofacetDistributionf(MicrofacetDistributionf.EBeckmann, 0.1, 0.1)
    md.scale_alpha(2.0)
    assert np.allclose(md.alpha_u(), 0.2)
    assert np.allclose(md.alpha_v(), 0.2)
