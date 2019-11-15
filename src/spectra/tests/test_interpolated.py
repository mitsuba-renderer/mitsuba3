from mitsuba.scalar_rgb.core.xml import load_string

def test01_interpolation():
    s = load_string('''
            <spectrum version='2.0.0' type='interpolated'>
                <float name="lambda_min" value="500"/>
                <float name="lambda_max" value="600"/>
                <string name="values" value="1, 2"/>
            </spectrum>''')

    assert s.eval(400) == 1
    assert s.eval(500) == 1
    assert s.eval(600) == 2
    assert s.eval(700) == 2
    assert s.sample(0)[0] == 300
    assert s.sample(1)[0] == 900
