import pytest
import drjit as dr
import mitsuba as mi
import numpy as np

def test01_binding_types(variants_all_rgb):
    x = mi.sggx_sample(mi.Frame3f(1), mi.Point2f(1), mi.SGGXPhaseFunctionParams([1,2,3,4,5,6]))
    assert type(x) == mi.Normal3f
    x = mi.sggx_sample(mi.Frame3f(1), mi.Point2f(1), [1,2,3,4,5,6])
    assert type(x) == mi.Normal3f
    x = mi.sggx_sample(mi.Vector3f(1), mi.Point2f(1), [1,2,3,4,5,6])
    assert type(x) == mi.Normal3f
    x = mi.sggx_pdf(mi.Vector3f(1), [1,2,3,4,5,6])
    assert type(x) == mi.Float
    x = mi.sggx_projected_area(mi.Vector3f(1), [1,2,3,4,5,6])
    assert type(x) == mi.Float