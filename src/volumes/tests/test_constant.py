import numpy as np

import mitsuba
import enoki as ek


def test01_constant_construct(variant_scalar_rgb):
    from mitsuba.core.xml import load_string
    from mitsuba.core import BoundingBox3f

    vol = load_string("""
        <volume type="constvolume" version="2.0.0">
            <transform name="to_world">
                <scale x="2" y="0.2" z="1"/>
            </transform>
            <spectrum name="value" value="500:3.0 600:3.0"/>
        </volume>
    """)

    assert vol is not None
    assert vol.bbox() == BoundingBox3f([0, 0, 0], [2, 0.2, 1])


def test02_constant_eval(variant_scalar_rgb):
    from mitsuba.core.xml import load_string
    from mitsuba.core import BoundingBox3f, Color3f
    from mitsuba.render import Interaction3f

    vol = load_string("""
        <volume type="constvolume" version="2.0.0">
             <rgb name="value" value="0.5, 1.0, 0.3" version="2.0.0" />
        </volume>""")

    it = ek.zero(Interaction3f, 1)
    assert np.allclose(vol.eval(it), Color3f(0.5, 1.0, 0.3))
    assert vol.bbox() == BoundingBox3f([0, 0, 0], [1, 1, 1])
