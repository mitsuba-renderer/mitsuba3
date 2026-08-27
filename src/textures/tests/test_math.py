"""
Tests of the math texture plugin: expression parsing, operator and function
semantics, input texture binding, the various evaluation modes, gradient
propagation, and parse error reporting.
"""

import math

import pytest
import drjit as dr
import mitsuba as mi


def make(expr, *inputs):
    """Instantiate a math texture; numeric inputs turn into uniform textures"""
    d = { 'type': 'math', 'expr': expr }
    for i, value in enumerate(inputs):
        if not isinstance(value, dict):
            value = { 'type': 'uniform', 'value': value }
        d[f'input_{i}'] = value
    return mi.load_dict(d)


def eval_1(expr, *inputs):
    si = dr.zeros(mi.SurfaceInteraction3f)
    value = make(expr, *inputs).eval_1(si)
    return value if isinstance(value, float) else value[0]


@pytest.mark.parametrize('expr, expected', [
    ('1 + 2 * 3', 7),
    ('(1 + 2) * 3', 9),
    ('2 - 3 - 4', -5),
    ('16 / 4 / 2', 2),
    ('-2 * -3', 6),
    ('2 * -3', -6),
    ('-(1 + 2)', -3),
    ('1.5e2 + 0.5', 150.5),
    ('.25 * 4', 1),
    ('1e-3', 1e-3),
    ('pi', math.pi),
    ('e', math.e),
])
def test01_arithmetic(variant_scalar_rgb, expr, expected):
    """Operators, precedence, associativity, literals, and constants"""
    dr.assert_allclose(eval_1(expr), expected)


@pytest.mark.parametrize('expr, expected', [
    ('abs(-2)', 2),
    ('sign(-3)', -1),
    ('sign(3)', 1),
    ('sqrt(2.25)', 1.5),
    ('cbrt(27)', 3),
    ('rcp(4)', 0.25),
    ('rsqrt(4)', 0.5),
    ('erf(0.5)', math.erf(0.5)),
    ('sin(0.5)', math.sin(0.5)),
    ('cos(0.5)', math.cos(0.5)),
    ('tan(0.5)', math.tan(0.5)),
    ('asin(0.5)', math.asin(0.5)),
    ('acos(0.5)', math.acos(0.5)),
    ('atan(0.5)', math.atan(0.5)),
    ('sinh(0.5)', math.sinh(0.5)),
    ('cosh(0.5)', math.cosh(0.5)),
    ('tanh(0.5)', math.tanh(0.5)),
    ('asinh(0.5)', math.asinh(0.5)),
    ('acosh(1.5)', math.acosh(1.5)),
    ('atanh(0.5)', math.atanh(0.5)),
    ('exp(0.5)', math.exp(0.5)),
    ('log(0.5)', math.log(0.5)),
    ('exp2(0.5)', 2 ** 0.5),
    ('log2(0.5)', -1),
    ('round(1.4)', 1),
    ('round(-1.6)', -2),
    ('trunc(-1.5)', -1),
    ('floor(-1.5)', -2),
    ('ceil(-1.5)', -1),
    ('min(2, 3)', 2),
    ('max(2, 3)', 3),
    ('pow(2, 10)', 1024),
    ('atan2(1, 1)', math.pi / 4),
    ('fmod(7, 3)', 1),
    ('fmod(-7, 3)', -1),
    ('lerp(2, 10, 0.25)', 4),
    ('clip(5, 0, 2)', 2),
    ('clip(-1, 0, 2)', 0),
    ('clip(1, 0, 2)', 1),
    ('fma(2, 3, 4)', 10),
])
def test02_functions(variant_scalar_rgb, expr, expected):
    dr.assert_allclose(eval_1(expr), expected)


@pytest.mark.parametrize('expr, expected', [
    ('1 < 2', 1),
    ('2 < 1', 0),
    ('1 <= 1', 1),
    ('2 > 1', 1),
    ('2 >= 3', 0),
    ('1 == 1', 1),
    ('1 != 1', 0),
    ('1 + 1 == 2', 1),
    ('2 && 3', 1),
    ('0 && 1', 0),
    ('0 || 5', 1),
    ('0 || 0', 0),
    ('!0', 1),
    ('!5', 0),
    ('!!3', 1),
    ('1 || 0 && 0', 1),
    ('1 < 2 && 3 < 4', 1),
    ('1 ? 2 : 3', 2),
    ('0 ? 2 : 3', 3),
    ('0 ? 1 : 0 ? 2 : 3', 3),
    ('1 ? 0 ? 5 : 6 : 7', 6),
    ('1 < 2 ? 3 + 4 : 5', 7),
])
def test03_logic(variant_scalar_rgb, expr, expected):
    """Comparisons and logical operators produce 0/1, the ternary operator
    consumes them, and precedence follows the C language"""
    dr.assert_allclose(eval_1(expr), expected)


@pytest.mark.parametrize('expr, expected', [
    ('tmp[0] = 2; tmp[0] * tmp[0]', 4),
    ('tmp[5] = 2; tmp[5] + tmp[5]', 4),
    ('tmp[0] = 1; tmp[1] = tmp[0] + 1; tmp[2] = tmp[1] * tmp[1]; tmp[2]', 4),
    ('  tmp[ 0 ]\n = 1 ;\n tmp[ 0 ] + 2 ', 3),
])
def test04_temporaries(variant_scalar_rgb, expr, expected):
    dr.assert_allclose(eval_1(expr), expected)


def test05_inputs(variant_scalar_rgb):
    """Nested textures bind to in[0], in[1], ... in declaration order"""
    dr.assert_allclose(eval_1('in[0] - in[1]', 0.75, 0.25), 0.5)
    dr.assert_allclose(eval_1('in[0] + 2*in[1] + 4*in[2] + 8*in[3]',
                              1, 2, 3, 4), 49)
    dr.assert_allclose(
        eval_1('tmp[0] = clip(in[0], 1, 2); tmp[1] = pow(in[0], tmp[0]); tmp[1]',
               0.5), 0.5)


def test06_eval_modes(variants_all_rgb):
    """eval, eval_1, and eval_3 apply the expression componentwise to the
    matching query of the nested inputs"""
    tex = make('2 * in[0] + 1', { 'type': 'srgb', 'color': [0.1, 0.2, 0.3] })
    ref = mi.load_dict({ 'type': 'srgb', 'color': [0.1, 0.2, 0.3] })
    si = dr.zeros(mi.SurfaceInteraction3f)
    dr.assert_allclose(tex.eval(si), 2 * mi.UnpolarizedSpectrum(ref.eval(si)) + 1)
    dr.assert_allclose(tex.eval_1(si), 2 * ref.eval_1(si) + 1)
    dr.assert_allclose(tex.eval_3(si), 2 * mi.Color3f(ref.eval_3(si)) + 1)


def test07_traversal(variant_scalar_rgb):
    """Inputs are exposed as differentiable parameters named in0, in1, ...,
    and spatial variation is inherited from the inputs"""
    tex = make('in[0] + in[1]', 0.25, { 'type': 'checkerboard' })
    keys = list(mi.traverse(tex).keys())
    assert any(k.startswith('in0.') for k in keys) and \
           any(k.startswith('in1.') for k in keys)
    assert tex.is_spatially_varying()
    assert not make('in[0]', 0.25).is_spatially_varying()
    assert not make('1').is_spatially_varying()


def test08_vectorized(variants_vec_backends_once_rgb):
    """Evaluation of a batch of surface interactions matches the directly
    written Dr.Jit arithmetic"""
    tex = make('in[0] * 2 + sin(in[1])',
               { 'type': 'checkerboard' },
               { 'type': 'checkerboard', 'color1': 0.6 })
    in0 = mi.load_dict({ 'type': 'checkerboard' })
    in1 = mi.load_dict({ 'type': 'checkerboard', 'color1': 0.6 })

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.uv = mi.Point2f([0.1, 0.3, 0.6, 0.9], [0.2, 0.8, 0.4, 0.7])
    dr.assert_allclose(tex.eval_1(si),
                       in0.eval_1(si) * 2 + dr.sin(in1.eval_1(si)))


def test09_forward_ad(variants_all_ad_rgb):
    """Gradients propagate through the exposed input parameters"""
    tex = make('in[0] * in[0] + sin(in[0])', 0.5)
    params = mi.traverse(tex)
    dr.enable_grad(params['in0.value'])
    params.update()

    si = dr.zeros(mi.SurfaceInteraction3f)
    value = tex.eval_1(si)
    dr.forward(params['in0.value'])
    dr.assert_allclose(dr.grad(value), 2 * 0.5 + math.cos(0.5))


@pytest.mark.parametrize('expr, error', [
    ('', 'must end with a result expression'),
    ('tmp[0] = 1;', 'must end with a result expression'),
    ('in[1]', 'exceeds the number of declared inputs'),
    ('in[]', 'expected an index'),
    ('tmp[0] + 1', 'read before being assigned'),
    ('tmp[0] = 1; tmp[0] = 2; tmp[0]', 'assigned twice'),
    ('in[0]; in[0]', 'only the final statement'),
    ('sin(1, 2)', 'takes 1 argument'),
    ('foo(1)', 'unknown identifier "foo"'),
    ('1 +', 'unexpected end of input'),
    ('1 + .', 'invalid number'),
    ('* 2', 'expected an operand'),
    ('(1 + 2', r"expected '\)'"),
    ('1 ? 2', r"expected ':'"),
    ('2 @ 3', 'expected end of input'),
    ('1 < 2 < 3', 'cannot be chained'),
])
def test10_parse_errors(variant_scalar_rgb, expr, error):
    with pytest.raises(RuntimeError, match=error):
        make(expr, 0.5)
