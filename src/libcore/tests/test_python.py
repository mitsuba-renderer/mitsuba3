import enoki as ek
import pytest
import mitsuba

def test01_set_variants():
    for v in mitsuba.variants():
        mitsuba.set_variant(v)