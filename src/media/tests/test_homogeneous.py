import numpy as np

from mitsuba.scalar_rgb.core.math import Pi
from mitsuba.scalar_rgb.core.xml import load_string
from mitsuba.scalar_rgb.core import Ray3f


def test01_create():
    p = load_string("""<medium version='2.0.0' type='homogeneous'>
        <spectrum type="uniform" name="sigma_t">
            <float name="value" value="1.0"/>
        </spectrum>
        <rgb name="albedo" value="0.5, 0.5, 0.2"/>
        </medium>
    """)
    assert p is not None


def test02_eval():
    sigma_t = 0.5
    density = 4.0
    p = load_string(f"""<medium version='2.0.0' type='homogeneous'>
            <spectrum type="uniform" name="sigma_t">
                <float name="value" value="{sigma_t}"/>
            </spectrum>
            <rgb name="albedo" value="0.5, 0.5, 0.2"/>
            <float name="density" value="{density}"/>
        </medium>
    """)
    ray = Ray3f()
    ray.wavelengths = np.zeros(3)
    sampler = load_string("<sampler version='2.0.0' type='independent'/>")
    for l in [0.0, 0.5, 5, 10]:
        ray.mint = 0
        ray.maxt = l
        eval_tr = p.eval_transmittance(ray, sampler)
        eval_tr = np.mean(eval_tr)
        ref_tr = np.exp(-(ray.maxt - ray.mint) * sigma_t * density)
        assert np.allclose(eval_tr, ref_tr)

