from mitsuba.core.xml import load_string
import numpy as np
import os


def test01_ply_triangle():
    spectrum = load_string("<spectrum version='2.0.0' type='importance'/>")
    print(spectrum)
    print(type(spectrum))
    print(spectrum.eval(400))
    print(spectrum.eval([400,500,600]))
    raise "asdf"
