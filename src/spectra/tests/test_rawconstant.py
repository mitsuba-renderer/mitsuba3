import pytest
import drjit as dr
import mitsuba as mi


def make_texture(value, xml: bool = False):
    if xml:
        if isinstance(value, float):
            value_string = f'<float name="value" value="{value}"/>'
        else:
            assert len(value) == 3
            value_string = f'<vector name="value" value="{value[0]}, {value[1]}, {value[2]}"/>'
        return mi.load_string(f'<texture type="rawconstant" version="3.0.0">{value_string}</texture>')
    else:
        return mi.load_dict({
            "type" : "rawconstant",
            "value": value
        })

@pytest.mark.parametrize("use_xml", [True, False])
def test01_construct(variants_all, use_xml):
    si = dr.zeros(mi.SurfaceInteraction3f)

    # 1D raw constant texture
    tex1 = make_texture(-2.5, xml=use_xml)
    assert dr.allclose(tex1.eval_1(si), -2.5)
    assert dr.allclose(tex1.eval_3(si), [-2.5, -2.5, -2.5])

    # 3D raw constant texture
    tex3 = make_texture([-0.5, 2.0, -1.3], xml=use_xml)
    with pytest.raises(RuntimeError, match=r"eval_1\(\) is not defined for 3D-valued textures"):
        tex3.eval_1(si)
    assert dr.allclose(tex3.eval_3(si), [-0.5, 2.0, -1.3])


def test02_eval(variants_all):
    si = dr.zeros(mi.SurfaceInteraction3f)

    for v in ([-0.5, 2.0, -1.3], -0.5):
        channels = 1 if isinstance(v, float) else len(v)
        tex = make_texture(v)

        # Either returns the original 3 values, or broadcasts the single value to 3D
        assert dr.allclose(tex.eval_3(si), v)
        if channels == 1:
            assert dr.allclose(tex.eval_1(si), v)
        else:
            with pytest.raises(RuntimeError, match=r"eval_1\(\) is not defined for 3D-valued textures"):
                tex.eval_1(si)

        # eval() is more tricky as it depends on the variant
        if channels == 1:
            assert dr.allclose(tex.eval(si), mi.UnpolarizedSpectrum(v))
        else:
            if mi.is_rgb or dr.size_v(mi.UnpolarizedSpectrum) == channels:
                # In RGB mode, should return full vector
                assert dr.allclose(tex.eval(si), v)
            else:
                with pytest.raises(RuntimeError, match=r"RawConstantTexture: eval\(\) is not defined for 3 channels.*"):
                    tex.eval(si)


def test03_sample_spectrum(variants_all):
    si = dr.zeros(mi.SurfaceInteraction3f)

    for v in ([-0.5, 4.0, -3.3], -0.5):
        channels = 1 if isinstance(v, float) else len(v)
        tex = make_texture(v)

        if channels == 1 or dr.size_v(mi.UnpolarizedSpectrum) == channels:
            wav, spec = tex.sample_spectrum(si, sample=si.wavelengths)
            assert dr.allclose(spec, tex.eval(si))
        else:
            with pytest.raises(RuntimeError, match=r"RawConstantTexture: eval\(\) is not defined for 3 channels.*"):
                tex.eval(si)


def test04_mean_max(variants_all):
    for v in ([-0.5, 4.0, -3.3], -0.5):
        vmax = 4.0 if isinstance(v, list) else v
        tex = make_texture(v)

        assert dr.allclose(tex.mean(), dr.mean(v))
        assert dr.allclose(tex.max(), vmax)
